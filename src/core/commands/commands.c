/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

void
reapPendingChild()
{
    pid_t p;
    do {
        p = waitpid(-1, NULL, WNOHANG);
        if (p > 0 && UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "The pid %d has been reaped!\n", p);
    } while (p != (pid_t)0 && p != (pid_t)-1);
    if (UNITD_DEBUG)
        syslog(LOG_DAEMON | LOG_DEBUG, "reapPendingChild terminated! Res = %d\n", p);
}

int
execScript(const char *unitdDataDir, const char *relScriptName, char **argv, char **envVar)
{
    pid_t child;
    int status;
    int exitCode = -1;
    char *command = NULL;
    Array *params = NULL;

    assert(unitdDataDir);
    assert(relScriptName);

    /* Building the command */
    command = stringNew(unitdDataDir);
    stringAppendStr(&command, relScriptName);

    if (!argv) {
        params = arrayNew(objectRelease);
        arrayAdd(params, stringNew(command));
        arrayAdd(params, NULL);
        argv = (char **)params->arr;
    }

    child = fork();
    switch (child) {
        case 0:
            /* Execute the command */
            if (envVar)
                (void)execve(command, argv, envVar);
            else
                (void)execv(command, argv);

            _exit(errno);
        case -1:
            unitdLogError(LOG_UNITD_BOOT, "src/core/commands/commands.c", "execScript", errno,
                          strerror(errno), "Unable to execute fork");
            return EXIT_FAILURE;
    }
    waitpid(child, &status, 0);
    if (WIFEXITED(status))
        exitCode = WEXITSTATUS(status);

    if (exitCode == -1) {
        unitdLogError(LOG_UNITD_BOOT, "src/core/commands/commands.c", "execScript", -1,
                      "Bad exit code for the %s script", relScriptName);
    }

    arrayRelease(&params);
    objectRelease(&command);
    return exitCode;
}

int
execProcess(const char *command, char **argv, Unit **unit)
{

    pid_t child;
    int status, millisec, res;
    ProcessData *pData = NULL;
    bool showWaitMsg, showResult;
    Array *wantedBy = NULL;
    Pipe *unitPipe = NULL;

    showWaitMsg = showResult = false;

    assert(command);
    assert(*unit);

    pData = (*unit)->processData;
    wantedBy = (*unit)->wantedBy;
    unitPipe = (*unit)->pipe;
    showResult = (*unit)->showResult;

    child = fork();
    switch (child) {
        case 0:
            /* Execute the command */
            if (arrayContainsStr(wantedBy, STATE_DATA_ITEMS[INIT].desc) ||
                arrayContainsStr(wantedBy, STATE_DATA_ITEMS[FINAL].desc)) {
                /* For the initialization and finalization units we pass
                 * the environment variables to the scripts
                 */
                (void)execve(command, argv, (char **)UNITD_ENV_VARS->arr);
            }
            else
                (void)execv(command, argv);

            _exit(errno);
        case -1:
            unitdLogError(LOG_UNITD_BOOT, "src/core/commands/commands.c", "execProcess", errno,
                          strerror(errno), "Unable to execute fork");
            *pData->exitCode = EXIT_FAILURE;
            return EXIT_FAILURE;
    }

    /* Assigning the pid */
    *pData->pid = child;
    assert(*pData->pid > 0);

    switch ((*unit)->type) {
        default:
            break;
        case DAEMON:
            /* We don't wait for the daemon. If it will exit or will be signaled for some reason then
             * the signal handler will catch it
            */
            while (waitpid(child, &status, WNOHANG) == 0) {
                *pData->exitCode = -1;
                *pData->pStateData = PSTATE_DATA_ITEMS[RUNNING];
                break;
            }
            break;
        case ONESHOT:
            millisec = 0;
            /* Timeout */
            while (millisec <= TIMEOUT_MS) {
                res = waitpid(child, &status, WNOHANG);
                if ((res > 0 && WIFEXITED(status)) ||
                    res == -1 ||
                    *pData->exitCode != -1) {

                    /* If the values has not been set by signal handler then we set them here */
                    if (*pData->exitCode == -1) {
                        timeSetCurrent(&pData->timeStop);
                        stringSetTimeStamp(&pData->dateTimeStopStr, pData->timeStop);
                        stringSetDiffTime(&pData->duration, pData->timeStop, pData->timeStart);
                        *pData->pStateData = PSTATE_DATA_ITEMS[EXITED];
                        *pData->exitCode = WEXITSTATUS(status);
                    }

                    /* we communicate the failure result if the unit has a pipe */
                    if (unitPipe && *pData->exitCode != EXIT_SUCCESS) {
                        if (write(unitPipe->fds[1], pData->exitCode, sizeof(int)) == -1) {
                            unitdLogError(LOG_UNITD_CONSOLE, "src/core/commands/commands.c", "execProcess",
                                          errno, strerror(errno), "Unable to write into pipe for the %s unit (oneshot case)",
                                          (*unit)->name);
                        }
                    }
                    break;
                }
                else {
                    if (millisec > MIN_TIMEOUT_MS) {
                        if (showResult && !showWaitMsg) {
                            unitdLogWarning(LOG_UNITD_CONSOLE, "Waiting for '%s' to exit (%d sec) ...\n",
                                         (*unit)->desc, (TIMEOUT_MS / 1000));
                            showWaitMsg = true;
                        }
                    }
                    msleep(TIMEOUT_INC_MS);
                    millisec += TIMEOUT_INC_MS;
                }
            }

            /* If it's not exited yet, kill it */
            if (res == 0 && pData->pStateData->pState != EXITED) {
                kill(child, SIGKILL);
                /* After killed, waiting for the pid's status
                 * to avoid zombie process creation
                */
                waitpid(child, &status, WNOHANG);

                *pData->exitCode = -1;
                *pData->pStateData = PSTATE_DATA_ITEMS[KILLED];
                *pData->signalNum = SIGKILL;
                timeSetCurrent(&pData->timeStop);
                stringSetTimeStamp(&pData->dateTimeStopStr, pData->timeStop);
                stringSetDiffTime(&pData->duration, pData->timeStop, pData->timeStart);
                if ((*unit)->showResult)
                    unitdLogErrorStr(LOG_UNITD_ALL, "Timeout expired for the %s unit!\n", (*unit)->name);
                arrayAdd((*unit)->errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNIT_TIMEOUT_ERR].desc,
                                                (*unit)->name));
            }
            break;
    }

    if (UNITD_DEBUG) {
        unitdLogInfo(LOG_UNITD_BOOT, "The %s unit with the %s command has returned the following values:\n"
                                     "type = %s\n"
                                     "pid = %d\n"
                                     "exitcode = %d\n"
                                     "signal = %d\n"
                                     "state = %s\n"
                                     "finalStatus = %d\n"
                                     "dateTimeStart = %s\n"
                                     "dateTimeStop = %s\n"
                                     "duration = %s\n",
                    (*unit)->name, command, PTYPE_DATA_ITEMS[(*unit)->type].desc, *pData->pid, *pData->exitCode,
                    *pData->signalNum, pData->pStateData->desc, *pData->finalStatus,
                    pData->dateTimeStartStr, pData->dateTimeStopStr, pData->duration);
    }

    return *pData->exitCode;
}

int
stopDaemon(const char *command, char **argv, Unit **unit)
{
    pid_t child;
    int status, millisec, res;
    pid_t pid = 0;
    ProcessData *pData = NULL;
    const char *unitName = NULL;
    int waitPidRes = -1;
    Array *wantedBy = NULL;

    assert(*unit);
    assert((*unit)->type == DAEMON);

    unitName = (*unit)->name;
    pData = (*unit)->processData;
    pid = *pData->pid;
    wantedBy = (*unit)->wantedBy;

    if (pid != -1) {
        /* Check if the pid exists */
        waitPidRes = waitpid(pid, &status, WNOHANG);
        if (waitPidRes == 0) {
            if (command && argv) {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "To stop the %s unit will be used a COMMAND\n",
                                 unitName);
                child = fork();
                switch (child) {
                    case 0:
                        if (arrayContainsStr(wantedBy, STATE_DATA_ITEMS[INIT].desc) ||
                            arrayContainsStr(wantedBy, STATE_DATA_ITEMS[FINAL].desc)) {
                            /* For the initialization and finalization units we pass
                            * the environment variables to the scripts
                            */
                            (void)execve(command, argv, (char **)UNITD_ENV_VARS->arr);
                        }
                        else
                            (void)execv(command, argv);

                        _exit(errno);
                    case -1:
                        unitdLogError(LOG_UNITD_BOOT, "src/core/commands/commands.c", "stopDaemon", errno,
                                      strerror(errno), "Unable to execute fork");
                        *pData->exitCode = EXIT_FAILURE;
                        return EXIT_FAILURE;
                }
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "To stop the %s unit will be used a SIGTERM signal\n",
                                 unitName);
                kill(pid, SIGTERM);
            }
        }

        /* After the stop command or sigterm signal, we waiting for it to exit/terminate
         * at most for 1 second with a milliseconds resolution to be more precise.
         * We have not to necessarily wait for 1 second !!
         * ATTENTION: we don't wait the status change of the child but the process in execution instead
        */
        millisec = 0;
        /* Timeout */
        while (millisec <= TIMEOUT_STOP_MS) {
            res = waitpid(pid, &status, WNOHANG);
            if (res > 0) {
                if (WIFEXITED(status) || WIFSIGNALED(status))
                    break;
            }
            else if (res == 0) {
                msleep(TIMEOUT_INC_MS);
                millisec += TIMEOUT_INC_MS;
            }
            else break;
        }

        /* If it's not exited yet, kill it! */
        if (res == 0) {
            kill(pid, SIGKILL);
            /* After killed, waiting for the pid's status
             * to avoid zombie process creation.
            */
            waitpid(pid, &status, WNOHANG);
        }
    }

    /* Set the values */
    *pData->exitCode = -1;
    *pData->pStateData = PSTATE_DATA_ITEMS[DEAD];
    *pData->signalNum = SIGKILL;
    timeSetCurrent(&pData->timeStop);
    stringSetTimeStamp(&pData->dateTimeStopStr, pData->timeStop);
    stringSetDiffTime(&pData->duration, pData->timeStop, pData->timeStart);

    if (UNITD_DEBUG)
        unitdLogInfo(LOG_UNITD_BOOT, "The %s unit has been stopped with the following values:\n"
                                     "type = %s\n"
                                     "pid = %d\n"
                                     "exitcode = %d\n"
                                     "signal = %d\n"
                                     "state = %s\n"
                                     "finalStatus = %d\n"
                                     "dateTimeStart = %s\n"
                                     "dateTimeStop = %s\n"
                                     "duration = %s\n",
                    unitName, PTYPE_DATA_ITEMS[(*unit)->type].desc, *pData->pid, *pData->exitCode,
                    *pData->signalNum, pData->pStateData->desc, *pData->finalStatus,
                    pData->dateTimeStartStr, pData->dateTimeStopStr, pData->duration);

    return *pData->exitCode;
}

char **
cmdlineSplit(const char *str)
{
    const char *c, *end;
    char **ret, **save;
    size_t count = 0;

    /* Initialize */
    end = NULL;
    ret = save = NULL;

    assert(str);

    for (c = str; isspace(*c); c++);
    while (*c) {
        size_t wordLen = 0;

        /* Reallocate our array */
        save = ret;
        if((ret = realloc(ret, (count + 1) * sizeof(char *))) == NULL) {
            ret = save;
            goto err;
        }

        /* Computing word length and check for unbalanced quotes */
        for (end = c; *end && !isspace(*end); end++) {
            if (*end == '\'' || *end == '"') {
                char quote = *end;
                while (*(++end) && *end != quote) {
                    if (*end == '\\' && *(end + 1) == quote) {
                        end++;
                    }
                    wordLen++;
                }
                if (*end != quote) {
                    errno = EINVAL;
                    goto err;
                }
            }
            else {
                if (*end == '\\' && (end[1] == '\'' || end[1] == '"')) {
                    end++; /* skipping the '\\' */
                }
                wordLen++;
            }
        }

        if (wordLen == (size_t)(end - c)) {
            /* nothing strange, copy it */
            if ((ret[count++] = strndup(c, wordLen)) == NULL)
                goto err;
        }
        else {
            /* We copy it removing quotes and escapes */
            char *dest = ret[count++] = malloc(wordLen + 1);
            if (dest == NULL)
                goto err;

            while (c < end) {
                if (*c == '\'' || *c == '"') {
                    char quote = *c;
                    /* we know there must be a matching end quote,
                     * no need to check for '\0'
                     */
                    for (c++; *c != quote; c++) {
                        if (*c == '\\' && *(c + 1) == quote) {
                            c++;
                        }
                        *(dest++) = *c;
                    }
                    c++;
                }
                else {
                    if (*c == '\\' && (c[1] == '\'' || c[1] == '"')) {
                        c++; /* skipping the '\\' */
                    }
                    *(dest++) = *(c++);
                }
            }
            *dest = '\0';
        }

        if (*end == '\0')
            break;
        else
            for (c = end + 1; isspace(*c); c++);
    }

    save = ret;
    if ((ret = realloc(ret, (count + 1) * sizeof(char *))) == NULL) {
        ret = save;
        goto err;
    }

    ret[count++] = NULL;
    return ret;

    err:
        /* we can't use cmdlineRelease() here because NULL has not been appended */
        while (count)
            free(ret[--count]);

        free(ret);
        return NULL;
}

void
cmdlineRelease(char **cmdline)
{
    if (cmdline) {
        char **c;
        for(c = cmdline; *c; c++) {
            free(*c);
            *c = NULL;
        }
        free(cmdline);
        cmdline = NULL;
    }
}
