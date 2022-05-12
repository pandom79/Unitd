/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

/* Options */
OptionData OPTIONS_DATA[] = {
    { FORCE_OPT, "force"},
    { RESTART_OPT, "restart"},
    { RUN_OPT, "run"},
    { REQUIRES_OPT, "requires"},
    { CONFLICTS_OPT, "conflicts"},
    { STATES_OPT, "states"}
};
int OPTIONS_LEN = 6;
/* Commands */
CommandData COMMANDS_DATA[] = {
    { REBOOT_COMMAND, "reboot" },
    { POWEROFF_COMMAND, "poweroff" },
    { HALT_COMMAND, "halt" },
    { LIST_COMMAND, "list" },
    { STATUS_COMMAND, "status" },
    { STOP_COMMAND, "stop" },
    { START_COMMAND, "start" },
    { RESTART_COMMAND, "restart" },
    { DISABLE_COMMAND, "disable" },
    { ENABLE_COMMAND, "enable" },
    { LIST_REQUIRES_COMMAND, "list-requires" },
    { LIST_CONFLICTS_COMMAND, "list-conflicts" },
    { LIST_STATES_COMMAND, "list-states" },
    { GET_DEFAULT_STATE_COMMAND, "get-default" },
    { SET_DEFAULT_STATE_COMMAND, "set-default" },
    { KEXEC_COMMAND, "kexec" },
};
int COMMANDS_LEN = 16;

State STATE_DEFAULT;
State STATE_NEW_DEFAULT;
State STATE_CMDLINE;
State STATE_SHUTDOWN;
char *STATE_CMDLINE_DIR;

int
unitdInit(UnitdData **unitdData, bool isAggregate)
{
    int rv, lenUnits;
    struct sigaction act = { 0 };
    char *initStateDir, *destDefStateSyml, *finalStateDir;
    Array **initUnits, **finalUnits, **units, **shutDownUnits;
    char *shutDownStateStr = NULL;

    rv = lenUnits = 0;
    initStateDir = destDefStateSyml = finalStateDir = NULL;
    initUnits = finalUnits = NULL;

    assert(*unitdData);
    initUnits = &(*unitdData)->initUnits;
    units = &(*unitdData)->units;
    shutDownUnits = &(*unitdData)->shutDownUnits;
    finalUnits = &(*unitdData)->finalUnits;

    /* Enable ctrl-alt-del signal (SIGINT) */
    reboot(RB_DISABLE_CAD);

    /* Set the values for the signals handler */
    /* We use SA_RESTART flag to avoid 'Interrupted system call' error by socket */
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    act.sa_sigaction = signalsHandler;
    if (sigaction(SIGTERM, &act, NULL) == -1 ||
        sigaction(SIGINT, &act, NULL)  == -1 ||
        sigaction(SIGCHLD, &act, NULL) == -1) {
        rv = -1;
        unitdLogError(LOG_UNITD_ALL, "src/core/init/init.c", "unitdInit", errno, strerror(errno),
                      "Sigaction has returned -1 exit code");
        goto out;
    }

    if (UNITD_DEBUG) {
        unitdLogInfo(LOG_UNITD_ALL, "%s starting as pid %d....\n", PROJECT_NAME, getpid());
        unitdLogInfo(LOG_UNITD_ALL, "Units path = %s\n", UNITS_PATH);
        unitdLogInfo(LOG_UNITD_ALL, "Units enab path = %s\n", UNITS_ENAB_PATH);
        unitdLogInfo(LOG_UNITD_ALL, "Unitd data path = %s\n", UNITD_DATA_PATH);
        unitdLogInfo(LOG_UNITD_ALL, "Log path = %s\n", UNITD_LOG_PATH);
        unitdLogInfo(LOG_UNITD_ALL, "Debug = %s\n", UNITD_DEBUG ? "True" : "False");
    }

    /* For each terminated state, we check if "SHUTDOWN_COMMAND" is set by signal handler.
     * If so (Ctrl+Alt+Del pressed), we start the reboot phase
    */

    //*************************** INIT STATE *********************************
     /* Parsing and starting the initialization units
     * For the initialization state, we always aggregate the errors to allow
     * to fix all the errors in a one shot
    */
//FIXME local test
//    initStateDir = stringNew(STATE_DATA_ITEMS[INIT].desc);
//    stringAppendStr(&initStateDir, ".state/units");
//    if ((rv = loadUnits(initUnits, UNITD_DATA_PATH, initStateDir,
//                        INIT, true, NULL, PARSE_UNIT, true)) != 0) {
//        /* Show unit configuration error and emergency shell */
//        unitdLogError(LOG_UNITD_ALL, "src/core/init/init.c", "unitdInit", rv,
//                      "An error has occurred in loadUnits for init.state", NULL);
//        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);
//        goto out;
//    }
//    if ((rv = startProcesses(initUnits, NULL)) != 0) {
//        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);
//        goto out;
//    }
//    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
//        goto shutdown;

//FIXME
//here, we can release the oneshot initialization units. Keep them into memory is very useless.
//Optimize memory usage. The daemons will be released just before of the final state execution

    //******************* DEFAULT OR CMDLINE STATE ************************
    assert(!UNITD_LOG_FILE);
    unitdOpenLog("w");
    if (STATE_CMDLINE_DIR)
        rv = loadUnits(units, UNITS_ENAB_PATH, STATE_CMDLINE_DIR,
                       STATE_CMDLINE, isAggregate, NULL, PARSE_UNIT, true);
    else {
        /* Get the default state as string */
        if (getDefaultStateStr(&destDefStateSyml) != 0) {
            /* If we are here then the symlink is not valid or missing */
            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);
            unitdCloseLog();
            /* Set the default shutdown command */
            SHUTDOWN_COMMAND = REBOOT_COMMAND;
            goto shutdown;
        }
        STATE_DEFAULT = getStateByStr(destDefStateSyml);
        if (STATE_DEFAULT == NO_STATE) {
            /* If we are here then the symlink points to a bad destination */
            unitdLogErrorStr(LOG_UNITD_CONSOLE, "The default state symlink points to a bad destination (%s)\n",
                                                destDefStateSyml);
            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);
            unitdCloseLog();
            /* Set the default shutdown command */
            SHUTDOWN_COMMAND = REBOOT_COMMAND;
            goto shutdown;
        }
        if (UNITD_DEBUG)
            unitdLogInfo(LOG_UNITD_BOOT, "The default.state symlink points to %s",
                         STATE_DATA_ITEMS[STATE_DEFAULT].desc);
        rv = loadUnits(units, UNITS_ENAB_PATH, DEF_STATE_SYML_NAME,
                       STATE_DEFAULT, isAggregate, NULL, PARSE_UNIT, true);
    }
    /* Zero units are not allowed in this state */
    if (rv == GLOB_NOMATCH) {
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);
        unitdCloseLog();
        /* Set the default shutdown command */
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto shutdown;
    }
    assert((STATE_CMDLINE != NO_STATE && STATE_DEFAULT == NO_STATE) ||
           (STATE_CMDLINE == NO_STATE && STATE_DEFAULT != NO_STATE));
    /* we open eventual pipes and start the processes */
    openPipes(units, NULL);
    startProcesses(units, NULL);
    ENABLE_RESTART = true;
    assert(UNITD_LOG_FILE);
    unitdCloseLog();
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;

    /* Unitd is blocked here listening the client requests */
    listenSocketRequest();

    shutdown:
        //******************* POWEROFF (HALT) / REBOOT STATE **********************
        unitdLogInfo(LOG_UNITD_ALL, "The system is going down ...\n");
        assert(!UNITD_LOG_FILE);
        unitdOpenLog("a");
        if (SHUTDOWN_COMMAND == NO_COMMAND) SHUTDOWN_COMMAND = REBOOT_COMMAND;
        if (SHUTDOWN_COMMAND == HALT_COMMAND) {
            shutDownStateStr = stringNew(COMMANDS_DATA[POWEROFF_COMMAND].name);
            stringAppendStr(&shutDownStateStr, ".state");
        }
        else {
            shutDownStateStr = stringNew(COMMANDS_DATA[SHUTDOWN_COMMAND].name);
            stringAppendStr(&shutDownStateStr, ".state");
        }
        STATE_SHUTDOWN = getStateByStr(shutDownStateStr);
        loadUnits(shutDownUnits, UNITS_ENAB_PATH, shutDownStateStr,
                  STATE_SHUTDOWN, isAggregate, NULL, PARSE_UNIT, true);
        startProcesses(shutDownUnits, NULL);

        //********************* STOPPING UNITS **********************************
        closePipes(units, NULL);
        stopProcesses(units, NULL);
        stopProcesses(shutDownUnits, NULL);
        stopProcesses(initUnits, NULL);
        assert(UNITD_LOG_FILE);
        unitdCloseLog();
        /* Set the new default state */
        if (STATE_NEW_DEFAULT != NO_STATE) {
            rv = setNewDefaultStateSyml(STATE_NEW_DEFAULT);
            if (rv != 0) {
                unitdLogError(LOG_UNITD_CONSOLE, "src/core/init/init.c", "unitdInit", rv, strerror(rv),
                              "An error has occurred in setNewDefaultStateSyml func", NULL);
                execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);
            }
        }

        //********************* FINAL STATE ************************************
        /* Parsing and starting the finalization units
         * For the finalization state, we always aggregate the errors to allow
         * to fix all the errors in a one shot
        */
//FIXME local test
//        finalStateDir = stringNew(STATE_DATA_ITEMS[FINAL].desc);
//        stringAppendStr(&finalStateDir, ".state/units");
//        if ((rv = loadUnits(finalUnits, UNITD_DATA_PATH, finalStateDir,
//                            FINAL, true, NULL, PARSE_UNIT, true)) != 0) {
//            /* Show unit configuration error and emergency shell */
//            unitdLogError(LOG_UNITD_ALL, "src/core/init/init.c", "unitdInit", rv,
//                          "An error has occurred in loadUnits for final.state", NULL);
//            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);
//            goto out;
//        }
//        if ((rv = startProcesses(finalUnits, NULL)) != 0)
//            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL);

    out:
        assert(!UNITD_LOG_FILE);
        objectRelease(&initStateDir);
        objectRelease(&destDefStateSyml);
        objectRelease(&shutDownStateStr);
        objectRelease(&finalStateDir);
        return rv;
}

void
unitdEnd(UnitdData **unitdData)
{
    arrayRelease(&UNITD_ENV_VARS);
    objectRelease(&STATE_CMDLINE_DIR);
    if (*unitdData) {
        arrayRelease(&(*unitdData)->initUnits);
        arrayRelease(&(*unitdData)->units);
        arrayRelease(&(*unitdData)->shutDownUnits);
        arrayRelease(&(*unitdData)->finalUnits);
        objectRelease(unitdData);
    }
}
