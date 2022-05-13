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
bool UNITD_DEBUG = false;
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
bool ENABLE_RESTART = false;

int
parseProcCmdLine()
{
    FILE *fp = NULL;
    int rv = 0;
    char *line = NULL;
    const char *stateSingle = "single";
    size_t len = 0;

    assert(PROC_CMDLINE_PATH);
    if ((fp = fopen(PROC_CMDLINE_PATH, "r")) == NULL) {
        rv = 1;
        unitdLogError(LOG_UNITD_ALL, "src/bin/unitd/main.c", "readCmdline", errno,
                      strerror(errno), "Unable to open %s", PROC_CMDLINE_PATH, NULL);
        return rv;
    }

    while (getline(&line, &len, fp) != -1) {
        if (stringContainsStr(line, PROC_CMDLINE_DEBUG_ARG))
            UNITD_DEBUG = true;
        /* If the command line contains the "single" string the we set the state to "SINGLE_USER" */
        if (stringContainsStr(line, stateSingle)) {
            STATE_CMDLINE = SINGLE_USER;
            goto out;
        }
        /* We exclude INIT, SINGLE (already handled and it has the precedence), REBOOT,
         * POWEROFF and FINAL STATE */
        for (State state = MULTI_USER; state <= GRAPHICAL; state++) {
            if (stringContainsStr(line, STATE_DATA_ITEMS[state].desc)) {
                STATE_CMDLINE = state;
                goto out;
            }
        }
    }

    out:
        objectRelease(&line);
        fclose(fp);
        fp = NULL;
        return rv;
}

int main() {

    int rv = 0;
    UnitdData *unitdData = NULL;
    bool hasError = false;

    assert(OS_NAME);
    assert(UNITS_PATH);
    assert(UNITS_ENAB_PATH);
    assert(UNITD_LOG_PATH);
    assert(UNITD_DATA_PATH);
    assert(UNITD_CONF_PATH);

    if (getuid() != 0) {
        unitdLogErrorStr(LOG_UNITD_CONSOLE, "Please, run this program as administrator.\n");
        exit(EXIT_FAILURE);
    }

    if ((rv = parseProcCmdLine()) == 1) {
        hasError = true;
        goto out;
    }

    /* We check if a state is defined on the command line */
    if (STATE_CMDLINE != NO_STATE) {
        STATE_CMDLINE_DIR = stringNew(STATE_DATA_ITEMS[STATE_CMDLINE].desc);
        stringAppendStr(&STATE_CMDLINE_DIR, ".state");
    }

    /* Welcome msg */
    unitdLogInfo(LOG_UNITD_CONSOLE, "Welcome to %s!\n", OS_NAME);

    // Unmask the file mode
    umask(0);

    // Change the current working directory to root
    chdir("/");

    /* Set PID */
    UNITD_PID = setsid();
    //FIXME local test
//    assert(UNITD_PID == 1);

    /* Detecting virtualization environment */
    rv = execScript(UNITD_DATA_PATH, "/scripts/virtualization.sh", NULL, NULL);
    if (rv == 0 || rv == 1) {
        if (rv == 1)
            addEnvVar("VIRTUALIZATION", "1");
        /* Adding the macro's path as environment variables */
        addEnvVar("UNITS_PATH", UNITS_PATH);
        addEnvVar("UNITS_ENAB_PATH", UNITS_ENAB_PATH);
        addEnvVar("UNITD_DATA_PATH", UNITD_DATA_PATH);
        addEnvVar("UNITD_CONF_PATH", UNITD_CONF_PATH);
        addEnvVar("OUR_UTMP_FILE", OUR_UTMP_FILE);
        addEnvVar("OUR_WTMP_FILE", OUR_WTMP_FILE);
        /* UNITD_ENV_VARS Array must be null terminated */
        arrayAdd(UNITD_ENV_VARS, NULL);

        /* Starting by a heap pointer */
        unitdData = calloc(1, sizeof(UnitdData));
        assert(unitdData);
        UNITD_DATA = unitdData;

        /* Init */
        unitdInit(&unitdData, false);

        /* Release all */
        unitdEnd(&unitdData);

        /* The system is going down */
//FIXME test
//        sync();
//        if (SHUTDOWN_COMMAND == NO_COMMAND)
//            SHUTDOWN_COMMAND = REBOOT_COMMAND;
//        switch (SHUTDOWN_COMMAND) {
//            case REBOOT_COMMAND:
//                unitdLogInfo(LOG_UNITD_CONSOLE, "Reboot the system ...");
//                reboot(RB_AUTOBOOT);
//                break;
//            case POWEROFF_COMMAND:
//                unitdLogInfo(LOG_UNITD_CONSOLE, "Power off the system ...");
//                reboot(RB_POWER_OFF);
//                break;
//            case HALT_COMMAND:
//                unitdLogInfo(LOG_UNITD_CONSOLE, "Halt the system ...");
//                reboot(RB_HALT_SYSTEM);
//                break;
//            case KEXEC_COMMAND:
//                unitdLogInfo(LOG_UNITD_CONSOLE, "Reboot the system with kexec ...");
//                if (isKexecLoaded())
//                    reboot(RB_KEXEC);
//                else {
//                    unitdLogWarning(LOG_UNITD_CONSOLE, "\nWarning : kexec is not loaded!");
//                    reboot(RB_AUTOBOOT);
//                }
//                break;
//            default:
//                break;
//        }
        /* Not reached */
    }
    else {
        unitdLogError(LOG_UNITD_ALL, "src/bin/unitd/main.c", "main", rv,
                      "An error has occurred in virtualization.sh", NULL);
        hasError = true;
    }

    /* Not reached when we reboot/poweroff the system */
    out:
        objectRelease(&STATE_CMDLINE_DIR);
        if (hasError) {
            /* Show emergency shell */
            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        }
        return 0;
}

