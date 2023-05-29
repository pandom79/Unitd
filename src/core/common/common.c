/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

char *UNITS_USER_LOCAL_PATH;
char *UNITS_USER_ENAB_PATH;

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
        if (stringEquals(stateStr, STATE_DATA_ITEMS[i].desc)) {
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
            logError(CONSOLE, "src/core/common/common.c", "getDefaultStateStr", EPERM,
                          strerror(EPERM), "The '%s' symlink is missing", defStateSymlPath);
        }
        else if (rv == 2) {
            logError(CONSOLE, "src/core/common/common.c", "getDefaultStateStr", EPERM,
                          strerror(EPERM), "The '%s' doesn't look like a symlink", defStateSymlPath);
        }
    }

    objectRelease(&defStateSymlPath);
    return rv;
}

int
setNewDefaultStateSyml(State newDefaultState, Array **messages, Array **errors)
{
    int rv = 0;
    char *from, *to;

    to = from = NULL;

    assert(*messages);
    assert(*errors);

    /* Building from */
    from = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&from, '/');
    stringAppendStr(&from, STATE_DATA_ITEMS[newDefaultState].desc);
    stringAppendStr(&from, ".state");
    /* Building to */
    to = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&to, '/');
    stringAppendStr(&to, DEF_STATE_SYML_NAME);

    /* Creating env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "FROM", from);
    addEnvVar(&envVars, "TO", to);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Execute the script */
    rv = execUScript(&envVars, "default-syml");
    if (rv != 0) {
        arrayAdd(*errors, getMsg(-1, UNITD_ERRORS_ITEMS[UNITD_GENERIC_ERR].desc));
        arrayAdd(*messages, getMsg(-1, UNITD_MESSAGES_ITEMS[UNITD_SYSTEM_LOG_MSG].desc));
    }
    else
        arrayAdd(*messages, getMsg(-1, UNITS_MESSAGES_ITEMS[UNIT_CREATED_SYML_MSG].desc,
                                   to, from));

    objectRelease(&from);
    objectRelease(&to);
    arrayRelease(&envVars);
    return rv;
}

void
arrayPrint(int options, Array **array, bool hasStrings)
{
    int len = (*array ? (*array)->size : 0);
    if (hasStrings) {
        for (int i = 0; i < len; i++)
            logInfo(options, "%s\n", (char *)(*array)->arr[i]);
    }
    else {
        for (int i = 0; i < len; i++)
            logInfo(options, "%p\n", (*array)->arr[i]);
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
        logError(CONSOLE | SYSTEM, "src/core/common/common.c", "isKexecLoaded",
                      errno, strerror(errno), "Unable to open '/sys/kernel/kexec_loaded' file!");
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
        logError(CONSOLE | SYSTEM, "src/core/common/common.c", "writeWtmp",
                      rv, strerror(rv), "Unable to open '%s' file descriptor!", OUR_WTMP_FILE);
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

    if (uWrite(fd, (char *)&utmp, sizeof(utmp)) == -1) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/core/common/common.c", "writeWtmp",
                      rv, strerror(rv), "Unable to write into '%s' file!!", OUR_WTMP_FILE);
        return rv;
    }
    if (close(fd) == -1) {
        rv = errno;
        logError(CONSOLE | SYSTEM, "src/core/common/common.c", "writeWtmp",
                      rv, strerror(rv), "Unable to close '%s' file descriptor!", OUR_WTMP_FILE);
        return rv;
    }

    if (isBooting) {
        if ((fd = open(OUR_UTMP_FILE, O_WRONLY | O_APPEND)) < 0) {
            rv = errno;
            logError(CONSOLE | SYSTEM, "src/core/common/common.c", "writeWtmp",
                          rv, strerror(rv), "Unable to open '%s' file descriptor!", OUR_UTMP_FILE);
            return rv;
        }

        if (uWrite(fd, (char *)&utmp, sizeof utmp) == -1) {
            rv = errno;
            logError(CONSOLE | SYSTEM, "src/core/common/common.c", "writeWtmp",
                          rv, strerror(rv), "Unable to write into '%s' file!", OUR_UTMP_FILE);
            return rv;
        }
        if (close(fd) == -1) {
            rv = errno;
            logError(CONSOLE | SYSTEM, "src/core/common/common.c", "writeWtmp",
                          rv, strerror(rv), "Unable to close '%s' file descriptor!", OUR_UTMP_FILE);
            return rv;
        }
    }

    return rv;
}

int
unitdUserCheck(const char *userIdStr, const char *userName)
{
    int rv = 0;
    char euirun[5] = {0};

    assert(userIdStr);
    assert(userName);
    assert(EUIRUN);
    sprintf(euirun, "%d", EUIRUN);

    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "EUIRUN", euirun);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Building command */
    char *cmd = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&cmd, "/scripts/unitd-user-check.sh");

    /* Creating script params */
    Array *scriptParams = arrayNew(objectRelease);
    arrayAdd(scriptParams, cmd); //0
    arrayAdd(scriptParams, stringNew(UNITS_USER_LOCAL_PATH)); //1
    arrayAdd(scriptParams, stringNew(UNITS_USER_ENAB_PATH)); //2
    arrayAdd(scriptParams, stringNew(UNITD_USER_TIMER_DATA_PATH)); //3
    arrayAdd(scriptParams, stringNew(userIdStr)); //4
    arrayAdd(scriptParams, stringNew(userName)); //5
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL); //6

    /* Execute the script */
    rv = execScript(UNITD_DATA_PATH, "/scripts/unitd-user-check.sh", scriptParams->arr, envVars->arr);
    if (rv != 0) {
        /* If rv == EUIRUN then the instance is already running */
        if (rv == EUIRUN) {
            logErrorStr(CONSOLE | SYSTEM, "Unitd instance is already running for %s user!", userName);
            printf("\n");
        }
        else {
            logError(CONSOLE | SYSTEM, "src/core/common/common.c",
                     "unitdUserCheck", rv, strerror(rv), "ExecScript error");
        }
    }

    arrayRelease(&envVars);
    arrayRelease(&scriptParams);
    return rv;
}

int
parseProcCmdLine()
{
    FILE *fp = NULL;
    int rv = 0;
    char *line = NULL;
    size_t len = 0;

    assert(PROC_CMDLINE_PATH);
    if ((fp = fopen(PROC_CMDLINE_PATH, "r")) == NULL) {
        rv = 1;
        logError(CONSOLE | UNITD_BOOT_LOG, "src/core/common/common.c", "parseProcCmdLine", errno,
                      strerror(errno), "Unable to open %s", PROC_CMDLINE_PATH, NULL);
        return rv;
    }

    while (getline(&line, &len, fp) != -1) {
        Array *values = stringSplit(line, " ", false);
        int len = (values ? values->size : 0);
        for (int i = 0; i < len; i++) {
            char *value = arrayGet(values, i);
            stringTrim(value, NULL);
            /* Unitd debug */
            if (stringEquals(value, PROC_CMDLINE_UNITD_DEBUG)) {
                UNITD_DEBUG = true;
                continue;
            }
            /* Single */
            else if (stringEquals(value, "single") || stringEquals(value, STATE_DATA_ITEMS[SINGLE_USER].desc)) {
                STATE_CMDLINE = SINGLE_USER;
                continue;
            }
            else {
                /* We exclude INIT, SINGLE (already handled), REBOOT,
                 * POWEROFF and FINAL STATE */
                for (State state = MULTI_USER; state <= GRAPHICAL; state++) {
                    if (stringEquals(value, STATE_DATA_ITEMS[state].desc)) {
                        STATE_CMDLINE = state;
                        break;
                    }
                }
            }
        }
        arrayRelease(&values);
    }

    objectRelease(&line);
    fclose(fp);
    fp = NULL;
    return rv;
}

int
setSigAction()
{
    int rv = 0;
    struct sigaction act = {0};

    /* Enable ctrl-alt-del signal (SIGINT) */
    reboot(RB_DISABLE_CAD);

    /* Set the values for the signals handler */
    /* We use SA_RESTART flag to avoid 'Interrupted system call' error by socket */
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    act.sa_sigaction = signalsHandler;
    if (sigaction(SIGTERM, &act, NULL) == -1 ||
        sigaction(SIGINT, &act, NULL)  == -1 ||
        sigaction(SIGALRM, &act, NULL)  == -1 ||
        sigaction(SIGCHLD, &act, NULL) == -1) {
        rv = -1;
        logError(CONSOLE | UNITD_BOOT_LOG, "src/core/common/common.c", "setSigAction", errno, strerror(errno),
                      "Sigaction returned -1 exit code");
    }

    return rv;
}

int
setUserData(int userId, struct passwd **userInfo)
{
    int rv = 0;

    /* Get user info */
    errno = 0;
    *userInfo = getpwuid(userId);
    if (!(*userInfo)) {
        logError(CONSOLE | SYSTEM, "src/core/common/common.c", "setUserData", errno,
                      strerror(errno), "Getpwuid returned a null pointer");
        rv = 1;
        goto out;
    }
    assert(*userInfo);

    /* Change the current working directory to user home */
    char *userHome = (*userInfo)->pw_dir;
    if ((rv = chdir(userHome)) == -1) {
        logError(CONSOLE | SYSTEM, "src/core/common/common.c", "setUserData", errno,
                      strerror(errno), "Chdir (user instance) for %d userId returned -1 exit code", userId);
        rv = 1;
        goto out;
    }

    /* Set user dirs */
    UNITS_USER_LOCAL_PATH = stringNew(userHome);
    stringAppendStr(&UNITS_USER_LOCAL_PATH, "/.config/unitd/units");
    UNITD_USER_CONF_PATH = stringNew(userHome);
    stringAppendStr(&UNITD_USER_CONF_PATH, "/.local/share/unitd");
    UNITD_USER_TIMER_DATA_PATH = stringNew(UNITD_USER_CONF_PATH);
    stringAppendStr(&UNITD_USER_TIMER_DATA_PATH, "/timer");
    UNITD_USER_LOG_PATH = stringNew(UNITD_USER_CONF_PATH);
    stringAppendStr(&UNITD_USER_LOG_PATH, "/unitd.log");
    UNITS_USER_ENAB_PATH = stringNew(UNITD_USER_CONF_PATH);
    stringAppendStr(&UNITS_USER_ENAB_PATH, "/units");
    assert(UNITS_USER_LOCAL_PATH);
    assert(UNITD_USER_CONF_PATH);
    assert(UNITD_USER_LOG_PATH);
    assert(UNITS_USER_ENAB_PATH);

    /* Set user socket path */
    const char *xdgRunTimeDir = getenv("XDG_RUNTIME_DIR");
    if (!xdgRunTimeDir) {
        rv = 1;
        logError(CONSOLE | SYSTEM, "src/core/common/common.c", "setUserSocketPath", rv,
                      strerror(rv), "XDG_RUNTIME_DIR for %d userId is not set", userId);
        goto out;
    }
    SOCKET_USER_PATH = stringNew(xdgRunTimeDir);
    stringAppendStr(&SOCKET_USER_PATH, "/unitd.sock");
    assert(SOCKET_USER_PATH);

    if (UNITCTL_DEBUG) {
        logInfo(CONSOLE, "Units user path = %s\n", UNITS_USER_PATH);
        logInfo(CONSOLE, "Units user local path = %s\n", UNITS_USER_LOCAL_PATH);
        logInfo(CONSOLE, "Units user conf path = %s\n", UNITD_USER_CONF_PATH);
        logInfo(CONSOLE, "Unitd user log path = %s\n", UNITD_USER_LOG_PATH);
        logInfo(CONSOLE, "Units user enab path = %s\n", UNITS_USER_ENAB_PATH);
        logInfo(CONSOLE, "socket user path = %s\n", SOCKET_USER_PATH);
    }

    out:
        return rv;
}

void
userDataRelease()
{
    objectRelease(&UNITS_USER_LOCAL_PATH);
    objectRelease(&UNITD_USER_CONF_PATH);
    objectRelease(&UNITD_USER_TIMER_DATA_PATH);
    objectRelease(&UNITD_USER_LOG_PATH);
    objectRelease(&UNITS_USER_ENAB_PATH);
    objectRelease(&SOCKET_USER_PATH);
}

void*
handleMutexThread(void *arg)
{
    MutexThreadData *mutexThreadData = NULL;
    int *rvThread = NULL, rv = 0;

    assert(arg);

    mutexThreadData = (MutexThreadData *)arg;

    if (mutexThreadData->lock) {
        if ((rv = pthread_mutex_lock(mutexThreadData->mutex)) != 0) {
            logError(SYSTEM, "src/core/common/common.c", "handleMutexThread",
                          rv, strerror(rv), "Unable to acquire the lock of the mutex");
            kill(UNITD_PID, SIGTERM);
        }
    }
    else {
        if ((rv = pthread_mutex_unlock(mutexThreadData->mutex)) != 0) {
            logError(SYSTEM, "src/core/common/common.c", "handleMutexThread",
                          rv, strerror(rv), "Unable to unlock the mutex");
            kill(UNITD_PID, SIGTERM);
        }
    }

    rvThread = calloc(1, sizeof(int));
    assert(rvThread);
    *rvThread = rv;
    return rvThread;
}

int
handleMutex(pthread_mutex_t *mutex, bool lock)
{
    int rv = 0, *rvThread = NULL;

    assert(mutex);

    pthread_t thread;
    MutexThreadData mutextThreadData = { .mutex = mutex, .lock = lock };

    if ((rv = pthread_create(&thread, NULL, handleMutexThread, &mutextThreadData)) != 0) {
        logError(SYSTEM | CONSOLE, "src/core/common/common.c", "handleMutex", rv,
                      strerror(rv), "Unable to create the thread for the mutex");
        kill(UNITD_PID, SIGTERM);
    }
    if ((rv = pthread_join(thread, (void **)&rvThread)) != 0) {
        logError(SYSTEM | CONSOLE, "src/core/common/common.c", "handleMutex", rv,
                      strerror(rv), "Unable to join the thread for the mutex");
        kill(UNITD_PID, SIGTERM);
    }
    if (*rvThread != 0)
        rv = *rvThread;

    objectRelease(&rvThread);

    return rv;
}

void
setStopAndDuration(ProcessData **processData)
{
    assert(*processData);
    // Time stop
    timeRelease(&(*processData)->timeStop);
    (*processData)->timeStop = timeNew(NULL);
    // Date time stop
    objectRelease(&(*processData)->dateTimeStopStr);
    (*processData)->dateTimeStopStr = stringGetTimeStamp((*processData)->timeStop, false, "%d-%m-%Y %H:%M:%S");
    // Duration
    objectRelease(&(*processData)->duration);
    (*processData)->duration = stringGetDiffTime((*processData)->timeStop, (*processData)->timeStart);
}

int
getMaxFileDesc(int *fdA, int *fdB)
{
    int max = *fdA + 1;
    if (*fdA < *fdB)
        max = *fdB + 1;

    return max;
}

char*
getUnitNameByOther(const char *fromUnitName, PType unitType)
{
    char *unitName = NULL;
    int endIndex = -1;

    assert(fromUnitName);
    assert(unitType != DAEMON && unitType != ONESHOT);

    switch (unitType) {
        case TIMER:
            endIndex = stringIndexOfStr(fromUnitName, ".utimer");
            break;
        case UPATH:
            endIndex = stringIndexOfStr(fromUnitName, ".upath");
        default:
            break;
    }

    assert(endIndex != -1);
    unitName = stringSub(fromUnitName, 0, endIndex - 1);
    assert(unitName);
    stringAppendStr(&unitName, ".unit");

    return unitName;
}

char*
getOtherNameByUnitName(const char *unitName, PType pType)
{
    char *otherName = NULL;
    int endIndex = -1;

    assert(unitName);
    assert(pType > NO_PROCESS_TYPE);

    endIndex = stringIndexOfStr(unitName, ".unit");

    if (endIndex != -1)
        otherName = stringSub(unitName, 0, endIndex - 1);
    else {
        otherName = stringNew(unitName);
    }
    assert(otherName);
    switch (pType) {
        case TIMER:
            stringAppendStr(&otherName, ".utimer");
            break;
        case UPATH:
            stringAppendStr(&otherName, ".upath");
            break;
        default:
            break;
    }

    return otherName;
}

void
showVersion()
{
    printf("%s%s%s\n", WHITE_UNDERLINE_COLOR, "LINK INFO", DEFAULT_COLOR);
    printf("  libunitd : ");
    logSuccess(CONSOLE, UNITD_VER);
    printf("\n");
    printf("   libulib : ");
    logSuccess(CONSOLE, ULIB_VER);
    printf("\n");
}

int
uRead(int fd, void *buf, size_t nbytes)
{
    int rv = 0;

    while ((rv = read(fd, buf, nbytes)) == -1 && errno == EINTR)
        continue;

    return rv;
}

int
uWrite(int fd, void *buf, size_t nbytes)
{
    int rv = 0;

    while ((rv = write(fd, buf, nbytes)) == -1 && errno == EINTR)
        continue;

    return rv;
}

ssize_t
uSend(int fd, const void *buf, size_t nbytes, int flags)
{
    ssize_t rv = 0;

    while ((rv = send(fd, buf, nbytes, flags)) == -1 && errno == EINTR)
        continue;

    return rv;
}

ssize_t
uRecv(int fd, void *buf, size_t nbytes, int flags)
{
    ssize_t rv = 0;

    while ((rv = recv(fd, buf, nbytes, flags)) == -1 && errno == EINTR)
        continue;

    return rv;
}
