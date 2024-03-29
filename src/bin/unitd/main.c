/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../core/unitd_impl.h"

/* These variables are initialized here and defined as extern in the unitd_impl header.
 * If the header's inclusion will not be enough to get them then just re-define it
 * in the translation unit.
*/
pid_t UNITD_PID = 0;
bool DEBUG = false;
FILE *UNITD_LOG_FILE = NULL;
State STATE_DEFAULT = NO_STATE;
State STATE_NEW_DEFAULT = NO_STATE;
State STATE_CMDLINE = NO_STATE;
State STATE_SHUTDOWN = NO_STATE;
char *STATE_CMDLINE_DIR = NULL;
Command SHUTDOWN_COMMAND = NO_COMMAND;
bool NO_WTMP = false;
Array *UNITD_ENV_VARS = NULL;
bool LISTEN_SOCK_REQUEST = false;
Time *BOOT_START = NULL;
Time *BOOT_STOP = NULL;
Time *SHUTDOWN_START = NULL;
Time *SHUTDOWN_STOP = NULL;
Cleaner *CLEANER = NULL;
Notifier *NOTIFIER = NULL;
pthread_mutex_t NOTIFIER_MUTEX = PTHREAD_MUTEX_INITIALIZER;
bool USER_INSTANCE = false;
char *UNITS_USER_LOCAL_PATH = NULL;
char *UNITS_USER_ENAB_PATH = NULL;
char *UNITD_USER_CONF_PATH = NULL;
char *UNITD_USER_TIMER_DATA_PATH = NULL;
char *UNITD_USER_LOG_PATH = NULL;
State STATE_USER = USER;
char *STATE_USER_DIR = NULL;
char *SOCKET_USER_PATH = NULL;
pthread_mutex_t START_MUTEX = PTHREAD_MUTEX_INITIALIZER;

static void __attribute__((noreturn)) usage(bool fail)
{
    // clang-format off
    fprintf(stdout,
        "Usage: unitd [OPTION] \n\n"
        WHITE_UNDERLINE_COLOR"OPTIONS\n"DEFAULT_COLOR
        "-v, --version      Show the version\n"
        "-d, --debug        Enable the debug\n"
        "-h, --help         Show usage\n\n"
    );
    exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
    // clang-format off
}

int main(int argc, char **argv) {

    int c = 0, rv = 0, userId = 0;
    UnitdData *unitdData = NULL;
    bool showEmergencyShell = false, hasError = false, version = false;
    const char *shortopts = "hdv";
    const struct option longopts[] = {
        { "help", no_argument, NULL, 'h' },
        { "debug", optional_argument, NULL, 'd' },
        { "version", optional_argument, NULL, 'v' },
        { 0, 0, 0, 0 }
    };

    assert(OS_NAME);
    assert(UNITS_PATH);
    assert(UNITS_USER_PATH);
    assert(UNITS_ENAB_PATH);
    assert(UNITD_LOG_PATH);
    assert(UNITD_DATA_PATH);
    assert(UNITD_CONF_PATH);

    /* Get options */
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
            case 'h':
                usage(false);
                break;
            case 'd':
                DEBUG = true;
                break;
            case 'v':
                version = true;
                break;
            default:
                usage(true);
        }
    }
    if (version) {
        showVersion();
        exit(EXIT_SUCCESS);
    }
    /* Check user id.
     * If UNITD_TEST macro is defined then we want the root user.
    */
    userId = getuid();
    assert(userId >= 0);
#ifdef UNITD_TEST
    if (userId != 0) {
        logErrorStr(CONSOLE, "Please, run this program as administrator (UNITD_TEST=true).\n");
        showEmergencyShell = hasError = true;
        goto out;
    }
#endif
    /* Boot start */
    BOOT_START = timeNew(NULL);
    /* Set PID */
    UNITD_PID = setsid();
    if (UNITD_PID != 1) {
#ifndef UNITD_TEST
        USER_INSTANCE = true;
#endif
        UNITD_PID = getpid();
    }
    /* Unmask the file mode */
    umask(0);
    if (!USER_INSTANCE) {
        /* Change the current working directory to root */
        if ((rv = chdir("/")) == -1) {
            logError(ALL, "src/bin/unitd/main.c", "main", errno,
                          strerror(errno), "Chdir (system instance) returned -1 exit code");
            showEmergencyShell = hasError = true;
            goto out;
        }
        /* Parsing /proc/cmdline file */
        if ((rv = parseProcCmdLine()) == 1) {
            showEmergencyShell = hasError = true;
            goto out;
        }
        /* We check if a state is defined on the command line */
        if (STATE_CMDLINE != NO_STATE) {
            STATE_CMDLINE_DIR = stringNew(STATE_DATA_ITEMS[STATE_CMDLINE].desc);
            stringAppendStr(&STATE_CMDLINE_DIR, ".state");
        }
        /* Welcome msg */
        logInfo(CONSOLE, "%sWelcome to %s!%s\n", WHITE_COLOR, OS_NAME, DEFAULT_COLOR);
        /* Detecting virtualization environment */
        Array *envVars = arrayNew(objectRelease);
        addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
        /* Must be null terminated */
        arrayAdd(envVars, NULL);
        rv = execUScript(&envVars, "virtualization");
        arrayRelease(&envVars);
        if (rv == 0 || rv == 1) {
            if (rv == 1)
                addEnvVar(&UNITD_ENV_VARS, "VIRTUALIZATION", "1");
            /* Adding the macro's path as environment variables */
            addEnvVar(&UNITD_ENV_VARS, "PATH", PATH_ENV_VAR);
            addEnvVar(&UNITD_ENV_VARS, "UNITS_PATH", UNITS_PATH);
            addEnvVar(&UNITD_ENV_VARS, "UNITS_USER_PATH", UNITS_USER_PATH);
            addEnvVar(&UNITD_ENV_VARS, "UNITS_ENAB_PATH", UNITS_ENAB_PATH);
            addEnvVar(&UNITD_ENV_VARS, "UNITD_DATA_PATH", UNITD_DATA_PATH);
            addEnvVar(&UNITD_ENV_VARS, "UNITD_CONF_PATH", UNITD_CONF_PATH);
            addEnvVar(&UNITD_ENV_VARS, "UNITD_TIMER_DATA_PATH", UNITD_TIMER_DATA_PATH);
            addEnvVar(&UNITD_ENV_VARS, "OUR_UTMP_FILE", OUR_UTMP_FILE);
            addEnvVar(&UNITD_ENV_VARS, "OUR_WTMP_FILE", OUR_WTMP_FILE);
            /* UNITD_ENV_VARS Array must be null terminated */
            arrayAdd(UNITD_ENV_VARS, NULL);
        }
        else {
            logError(ALL, "src/bin/unitd/main.c", "main", rv,
                          strerror(rv), "An error has occurred in virtualization.sh");
            showEmergencyShell = hasError = true;
            goto out;
        }
    }
    else {
        struct passwd *userInfo = NULL;
        /* Set user data */
        if ((rv = setUserData(userId, &userInfo)) != 0) {
            hasError = true;
            goto out;
        }
        /* Get userId as string */
        char userIdStr[20] = {0};
        sprintf(userIdStr, "%d", userId);
        /* Check the user directories are there and the instance is not already running for this user */
        if ((rv = unitdUserCheck(userIdStr, userInfo->pw_name)) != 0) {
            hasError = true;
            goto out;
        }
        /* For the user instance we never show the emrgency shell and we put whatever error into log which
         * can be already opened in this case (disk already mounted) */
        assert(!UNITD_LOG_FILE);
        unitdOpenLog("w");
    }
    /* Starting from an heap pointer */
    unitdData = calloc(1, sizeof(UnitdData));
    assert(unitdData);
    UNITD_DATA = unitdData;
    /* Set sigaction */
    if ((rv = setSigAction()) != 0) {
        if (!USER_INSTANCE)
            showEmergencyShell = true;
        hasError = true;
        goto out;
    }
    /* Init */
    if (!USER_INSTANCE)
        rv = unitdInit(&unitdData, false);
    else
        rv = unitdUserInit(&unitdData, false);
    /* Release all */
    unitdEnd(&unitdData);
    if (SHUTDOWN_START) {
        /* Print Shutdown time */
        SHUTDOWN_STOP = timeNew(NULL);
        char *diff = stringGetDiffTime(SHUTDOWN_STOP, SHUTDOWN_START);
        char *msg = getMsg(-1, UNITS_MESSAGES_ITEMS[TIME_MSG].desc, "Shutdown", diff);
        logInfo(CONSOLE | UNITD_BOOT_LOG, "%s%s%s\n", WHITE_COLOR, msg, DEFAULT_COLOR);
        objectRelease(&diff);
        objectRelease(&msg);
        timeRelease(&SHUTDOWN_START);
        timeRelease(&SHUTDOWN_STOP);
    }
    if (!USER_INSTANCE) {
        /* The system is going down */
#ifndef UNITD_TEST
        sync();
        switch (SHUTDOWN_COMMAND) {
            case REBOOT_COMMAND:
                logSuccess(CONSOLE, "System reboot ...");
                reboot(RB_AUTOBOOT);
                break;
            case POWEROFF_COMMAND:
                logSuccess(CONSOLE, "System power off ...");
                reboot(RB_POWER_OFF);
                break;
            case HALT_COMMAND:
                logSuccess(CONSOLE, "System halt ...");
                reboot(RB_HALT_SYSTEM);
                break;
            case KEXEC_COMMAND:
                logSuccess(CONSOLE, "System reboot with kexec ...");
                reboot(RB_KEXEC);
                break;
            default:
                break;
        }
#endif
    }

    out:
        if (showEmergencyShell)
            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        if (hasError)
            unitdEnd(&unitdData);
        if (!USER_INSTANCE) {
#ifndef UNITD_TEST
            logInfo(CONSOLE, "System reboot ...\n");
            sync();
            reboot(RB_AUTOBOOT);
#endif
        }
        else {
            syslog(LOG_DAEMON | LOG_INFO, "Unitd instance for %d userId exited with %d (%s) exit code.\n",
                                          userId, rv, strerror(rv));
            unitdCloseLog();
            assert(!UNITD_LOG_FILE);
        }

        return rv;
}
