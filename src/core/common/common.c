/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

int
msleep(long msec)
{
   struct timespec ts;
   int res;

   if (msec < 0) {
       errno = EINVAL;
       return -1;
   }
   ts.tv_sec = msec / 1000;
   ts.tv_nsec = (msec % 1000) * 1000000;
   do {
       res = nanosleep(&ts, &ts);
   } while (res && errno == EINTR);

   return res;
}

/*
0 = Valid symlink
1 = Symlink missing
2 = Not a symlink
*/
int
readSymLink(const char *symLink, char **wherePoints)
{
    struct stat sb;
    ssize_t nbytes, bufsiz;
    int rv = 0;

    assert(symLink);
    if ((rv = lstat(symLink, &sb)) == -1) {
        rv = 1;
        goto out;
    }

    /* Set buffer size */
    bufsiz = sb.st_size + 1;
    if (sb.st_size == 0)
        bufsiz = PATH_MAX;

    /* Read the symlink */
    *wherePoints = calloc(bufsiz, sizeof(char));
    assert(*wherePoints);
    nbytes = readlink(symLink, *wherePoints, bufsiz);
    if (nbytes == -1)
        rv = 2;

    out:
        return rv;
}

void
addEnvVar(Array **envVarArr, const char *envVarName, const char *envVarValue)
{
    char *envVar  = NULL;

    assert(envVarName);
    assert(envVarValue);

    if (!(*envVarArr))
        *envVarArr = arrayNew(objectRelease);

    envVar = stringNew(envVarName);
    stringAppendStr(&envVar, ASSIGNER);
    stringAppendStr(&envVar, envVarValue);
    arrayAdd(*envVarArr, envVar);
}

State
getStateByStr(char *stateArg)
{
    State ret = NO_STATE;
    int startIdx, endIdx;
    char *stateStr = NULL;

    startIdx = endIdx = -1;

    startIdx = stringLastIndexOfChr(stateArg, '/');
    endIdx = stringLastIndexOfStr(stateArg, ".state");

    if (startIdx != -1 && endIdx != -1)
        stateStr = stringSub(stateArg, startIdx + 1, endIdx - 1);
    else if (startIdx == -1 && endIdx != -1)
        stateStr = stringSub(stateArg, 0, endIdx - 1);
    else
        stateStr = stringNew(stateArg);

    for (int i = 0; i < STATE_DATA_ITEMS_LEN; i++) {
        if (strcmp(stateStr, STATE_DATA_ITEMS[i].desc) == 0) {
            ret = i;
            break;
        }
    }

    objectRelease(&stateStr);
    return ret;
}

int
getDefaultStateStr(char **destDefStateSyml)
{
    int rv = 0;
    char *defStateSymlPath = NULL;

    /* Building path */
    defStateSymlPath = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&defStateSymlPath, '/');
    stringAppendStr(&defStateSymlPath, DEF_STATE_SYML_NAME);
    if ((rv = readSymLink(defStateSymlPath, destDefStateSyml)) != 0) {
        /* These errors would have to never happen */
        if (rv == 1) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/common/common.c", "getDefaultStateStr", EPERM,
                          strerror(EPERM), "The '%s' symlink is missing", defStateSymlPath);
        }
        else if (rv == 2) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/common/common.c", "getDefaultStateStr", EPERM,
                          strerror(EPERM), "The '%s' doesn't look like a symlink", defStateSymlPath);
        }
    }

    objectRelease(&defStateSymlPath);
    return rv;
}

int
setNewDefaultStateSyml(State newDefaultState, Array **messages)
{
    int rv = 0;
    Array *scriptParams = arrayNew(objectRelease);
    char *command, *from, *to;

    command = to = from = NULL;

    /* Building from */
    from = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&from, '/');
    stringAppendStr(&from, STATE_DATA_ITEMS[newDefaultState].desc);
    stringAppendStr(&from, ".state");
    /* Building to */
    to = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&to, '/');
    stringAppendStr(&to, DEF_STATE_SYML_NAME);
    /* Building command */
    command = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&command, "/scripts/symlink-handle.sh");
    arrayAdd(scriptParams, command); //0
    arrayAdd(scriptParams, stringNew(SYML_ADD_OP)); //1
    arrayAdd(scriptParams, from); //2
    arrayAdd(scriptParams, to); //3
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL); //4
    /* Execute the script */
    rv = execScript(UNITD_DATA_PATH, "/scripts/symlink-handle.sh", scriptParams->arr, NULL);
    if (rv != 0) {
        /* We don't put this error into response because it should never occurred */
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common.c::setNewDefaultStateSyml."
                                     "ExecScript func has returned %d = %s", rv, strerror(rv));
    }
    else
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CREATED_SYML_MSG].desc,
                                   to, from));
    arrayRelease(&scriptParams);
    return rv;
}

void
arrayPrint(int options, Array **array, bool hasStrings)
{
    int len = (*array ? (*array)->size : 0);
    if (hasStrings) {
        for (int i = 0; i < len; i++)
            unitdLogInfo(options, "%s\n", (char *)(*array)->arr[i]);
    }
    else {
        for (int i = 0; i < len; i++)
            unitdLogInfo(options, "%p\n", (*array)->arr[i]);
    }
}

bool
isKexecLoaded()
{
    FILE *fp = NULL;
    char *line = NULL;
    bool res = false;
    size_t len = 0;

    if ((fp = fopen("/sys/kernel/kexec_loaded", "r")) == NULL) {
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common::isKexecLoaded.\n"
                                     "Unable to open '/sys/kernel/kexec_loaded' file!");
        return res;
    }
    if (getline(&line, &len, fp) != -1) {
        if (strncmp(line, "1", 1) == 0)
            res = true;
    }
    objectRelease(&line);
    fclose(fp);
    fp = NULL;
    return res;
}

int
writeWtmp(bool isBooting) {
    int fd, rv;
    rv = 0;

    if ((fd = open(OUR_WTMP_FILE, O_WRONLY|O_APPEND)) < 0) {
        rv = errno;
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common::writeWtmp.\n"
                                     "Unable to open '%s' file descriptor! Rv = %d (%s).", OUR_WTMP_FILE,
                                     rv, strerror(rv));
        return rv;
    }

    struct utmp utmp = {0};
    struct utsname uname_buf;
    struct timeval tv;

    gettimeofday(&tv, 0);
    utmp.ut_tv.tv_sec = tv.tv_sec;
    utmp.ut_tv.tv_usec = tv.tv_usec;

    utmp.ut_type = isBooting ? BOOT_TIME : RUN_LVL;

    strncpy(utmp.ut_name, isBooting ? "reboot" : "shutdown", sizeof(utmp.ut_name));
    strncpy(utmp.ut_id , "~~", sizeof(utmp.ut_id));
    strncpy(utmp.ut_line, isBooting ? "~" : "~~", sizeof(utmp.ut_line));
    if (uname(&uname_buf) == 0)
        strncpy(utmp.ut_host, uname_buf.release, sizeof(utmp.ut_host));

    if (write(fd, (char *)&utmp, sizeof(utmp)) == -1) {
        rv = errno;
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common::writeWtmp.\n"
                                     "Unable to write into '%s' file! Rv = %d (%s).", OUR_WTMP_FILE,
                                     rv, strerror(rv));
        return rv;
    }
    if (close(fd) == -1) {
        rv = errno;
        syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common::writeWtmp.\n"
                                     "Unable to close '%s' file descriptor! Rv = %d (%s).", OUR_WTMP_FILE,
                                     rv, strerror(rv));
        return rv;
    }

    if (isBooting) {
        if ((fd = open(OUR_UTMP_FILE, O_WRONLY|O_APPEND)) < 0) {
            rv = errno;
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common::writeWtmp.\n"
                                         "Unable to open '%s' file descriptor! Rv = %d (%s).", OUR_UTMP_FILE,
                                         rv, strerror(rv));
            return rv;
        }

        if (write(fd, (char *)&utmp, sizeof utmp) == -1) {
            rv = errno;
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common::writeWtmp.\n"
                                         "Unable to write into '%s' file! Rv = %d (%s).", OUR_UTMP_FILE,
                                         rv, strerror(rv));
            return rv;
        }
        if (close(fd) == -1) {
            rv = errno;
            syslog(LOG_DAEMON | LOG_ERR, "An error has occurred in common::writeWtmp.\n"
                                         "Unable to close '%s' file descriptor! Rv = %d (%s).", OUR_UTMP_FILE,
                                         rv, strerror(rv));
            return rv;
        }
    }

    return rv;
}
