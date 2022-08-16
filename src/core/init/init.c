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
    { STATES_OPT, "states"},
    { NO_WTMP_OPT, "no-wtmp"},
    { ANALYZE_OPT, "analyze"},
    { RE_ENABLE_OPT, "re-enable"}
};
int OPTIONS_LEN = 9;
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
    { CAT_COMMAND, "cat" },
    { EDIT_COMMAND, "edit" },
    { ANALYZE_COMMAND, "analyze" },
    { RE_ENABLE_COMMAND, "re-enable" },
};
int COMMANDS_LEN = 20;
State STATE_DEFAULT;
State STATE_NEW_DEFAULT;
State STATE_CMDLINE;
State STATE_SHUTDOWN;
char *STATE_CMDLINE_DIR;
Time *BOOT_START;
Time *SHUTDOWN_START;
bool NO_WTMP;
Array *UNITD_ENV_VARS;
pid_t UNITD_PID;
char *UNITD_USER_CONF_PATH;
State STATE_USER;
char *STATE_USER_DIR;

static void
addBootUnits(Array **bootUnits, Array **units) {
    if (!(*bootUnits))
        *bootUnits = arrayNew(unitRelease);
    int len = (*units ? (*units)->size : 0);
    for (int i = 0; i < len; i++)
        arrayAdd(*bootUnits, unitNew(arrayGet(*units, i), PARSE_SOCK_RESPONSE_UNITLIST));
}

int
unitdInit(UnitdData **unitdData, bool isAggregate)
{
    int rv, lenUnits;
    char *initStateDir, *destDefStateSyml, *finalStateDir;
    Array **initUnits, **finalUnits, **units, **shutDownUnits, **bootUnits;
    char *shutDownStateStr = NULL;

    rv = lenUnits = 0;
    initStateDir = destDefStateSyml = finalStateDir = NULL;
    initUnits = finalUnits = bootUnits = NULL;

    assert(*unitdData);
    initUnits = &(*unitdData)->initUnits;
    units = &(*unitdData)->units;
    shutDownUnits = &(*unitdData)->shutDownUnits;
    finalUnits = &(*unitdData)->finalUnits;
    bootUnits = &(*unitdData)->bootUnits;

    if (UNITD_DEBUG) {
        unitdLogInfo(LOG_UNITD_ALL, "%s starting as pid %d\n", PROJECT_NAME, UNITD_PID);
        unitdLogInfo(LOG_UNITD_ALL, "Units path = %s\n", UNITS_PATH);
        unitdLogInfo(LOG_UNITD_ALL, "Units enab path = %s\n", UNITS_ENAB_PATH);
        unitdLogInfo(LOG_UNITD_ALL, "Unitd data path = %s\n", UNITD_DATA_PATH);
        unitdLogInfo(LOG_UNITD_ALL, "Unitd Log path = %s\n", UNITD_LOG_PATH);
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
#ifndef UNITD_TEST
    initStateDir = stringNew(STATE_DATA_ITEMS[INIT].desc);
    stringAppendStr(&initStateDir, ".state/units");
    if ((rv = loadUnits(initUnits, UNITD_DATA_PATH, initStateDir,
                        INIT, true, NULL, PARSE_UNIT, true)) != 0) {
        /* Show unit configuration error and emergency shell */
        unitdLogError(LOG_UNITD_ALL, "src/core/init/init.c", "unitdInit", rv,
                      "An error has occurred in loadUnits for init.state", NULL);
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        goto out;
    }
    if ((rv = startProcesses(initUnits, NULL)) != 0) {
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        goto out;
    }
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;
#endif

    assert(!UNITD_LOG_FILE);
    unitdOpenLog("w");
    /* Start the cleaner */
    startCleaner();
    /* Start the notifiers */
    startNotifiers();
    //******************* DEFAULT OR CMDLINE STATE ************************
    /* Set the default state variable.
     * Actually, the following errors should never happen because the unitd-check initialization
     * unit which makes this job has been already called.
     */
    if (getDefaultStateStr(&destDefStateSyml) != 0) {
        /* If we are here then the symlink is not valid or missing */
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
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
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        unitdCloseLog();
        /* Set the default shutdown command */
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto shutdown;
    }
    assert(STATE_DEFAULT != NO_STATE);
    if (UNITD_DEBUG)
        unitdLogInfo(LOG_UNITD_BOOT, "The default.state symlink points to %s\n",
                     STATE_DATA_ITEMS[STATE_DEFAULT].desc);

    /* Parsing the units for the cmdline or default state */
    if (STATE_CMDLINE_DIR) {
        if (UNITD_DEBUG)
            unitdLogInfo(LOG_UNITD_BOOT, "The state of the cmdline is %s\n",
                         STATE_DATA_ITEMS[STATE_CMDLINE].desc);
        rv = loadUnits(units, UNITS_ENAB_PATH, STATE_CMDLINE_DIR,
                       STATE_CMDLINE, isAggregate, NULL, PARSE_UNIT, true);
    }
    else {
        rv = loadUnits(units, UNITS_ENAB_PATH, DEF_STATE_SYML_NAME,
                       STATE_DEFAULT, isAggregate, NULL, PARSE_UNIT, true);
    }
    /* Zero units are not allowed in this state (default/cmdline) */
    if (rv == GLOB_NOMATCH) {
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        unitdCloseLog();
        /* Set the default shutdown command */
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto shutdown;
    }
    /* we open eventual pipes and start the processes */
    openPipes(units, NULL);
    startProcesses(units, NULL);
    ENABLE_RESTART = true;
    unitdCloseLog();
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;

    /* Create the boot units array */
    addBootUnits(bootUnits, units);
    /* Unitd is blocked here listening the client requests */
    listenSocketRequest();

    shutdown:
        /* Shutdown start */
        timeSetCurrent(&SHUTDOWN_START);
        assert(!UNITD_LOG_FILE);
        unitdOpenLog("a");
        /* Stop the cleaner */
        stopCleaner();
        /* Stop the notifiers */
        stopNotifiers();
        //******************* POWEROFF (HALT) / REBOOT STATE **********************
        unitdLogInfo(LOG_UNITD_ALL, "The system is going down ...\n");
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
        unitdCloseLog();
        /* Write a wtmp 'shutdown' record */
        if (!NO_WTMP && writeWtmp(false) != 0)
            unitdLogErrorStr(LOG_UNITD_CONSOLE, "An error has occurred in writeWtmp!\n");

        //********************* FINAL STATE ************************************
        /* Parsing and starting the finalization units
         * For the finalization state, we always aggregate the errors to allow
         * to fix all the errors in a one shot
        */
#ifndef UNITD_TEST
        finalStateDir = stringNew(STATE_DATA_ITEMS[FINAL].desc);
        stringAppendStr(&finalStateDir, ".state/units");
        if ((rv = loadUnits(finalUnits, UNITD_DATA_PATH, finalStateDir,
                            FINAL, true, NULL, PARSE_UNIT, true)) != 0) {
            /* Show unit configuration error and emergency shell */
            unitdLogError(LOG_UNITD_ALL, "src/core/init/init.c", "unitdInit", rv,
                          "An error has occurred in loadUnits for final.state", NULL);
            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
            goto out;
        }
        if ((rv = startProcesses(finalUnits, NULL)) != 0)
            execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
#endif

    out:
        assert(!UNITD_LOG_FILE);
        objectRelease(&initStateDir);
        objectRelease(&destDefStateSyml);
        objectRelease(&shutDownStateStr);
        objectRelease(&finalStateDir);
        return rv;
}

int
unitdUserInit(UnitdData **unitdData, bool isAggregate)
{
    int rv = 0;
    Array **units, **bootUnits;

    units = bootUnits = NULL;

    assert(*unitdData);
    units = &(*unitdData)->units;
    if (!(*units))
        *units = arrayNew(unitRelease);
    bootUnits = &(*unitdData)->bootUnits;

    if (UNITD_DEBUG) {
        unitdLogInfo(LOG_UNITD_BOOT, "%s starting as pid %d\n", PROJECT_USER_NAME, UNITD_PID);
        unitdLogInfo(LOG_UNITD_BOOT, "Units user path = %s\n", UNITS_USER_PATH);
        unitdLogInfo(LOG_UNITD_BOOT, "Units user local path = %s\n", UNITS_USER_LOCAL_PATH);
        unitdLogInfo(LOG_UNITD_BOOT, "Units user conf path = %s\n", UNITD_USER_CONF_PATH);
        unitdLogInfo(LOG_UNITD_BOOT, "Unitd user log path = %s\n", UNITD_USER_LOG_PATH);
        unitdLogInfo(LOG_UNITD_BOOT, "Units user enab path = %s\n", UNITS_USER_ENAB_PATH);
        unitdLogInfo(LOG_UNITD_BOOT, "Debug = %s\n", UNITD_DEBUG ? "True" : "False");
    }

    /* Start the cleaner */
    startCleaner();
    /* Start the notifiers */
    startNotifiers();
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;

    //******************* USER STATE ************************
    /* Parsing the units for the user state */
    STATE_USER_DIR = stringNew(STATE_DATA_ITEMS[USER].desc);
    stringAppendStr(&STATE_USER_DIR, ".state");
    rv = loadUnits(units, UNITS_USER_ENAB_PATH, STATE_USER_DIR,
                   STATE_USER, isAggregate, NULL, PARSE_UNIT, true);
    /* we open eventual pipes and start the processes */
    openPipes(units, NULL);
    startProcesses(units, NULL);
    ENABLE_RESTART = true;
    unitdCloseLog();
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;

    /* Create the boot units array */
    addBootUnits(bootUnits, units);
    /* Unitd is blocked here listening the client requests */
    listenSocketRequest();

    shutdown:
        /* Shutdown start */
        timeSetCurrent(&SHUTDOWN_START);
        unitdCloseLog();
        assert(!UNITD_LOG_FILE);
        unitdOpenLog("a");
        /* Stop the cleaner */
        stopCleaner();
        /* Stop the notifiers */
        stopNotifiers();
        //********************* STOPPING UNITS **********************************
        closePipes(units, NULL);
        stopProcesses(units, NULL);

        objectRelease(&STATE_USER_DIR);
        return rv;
}

void
unitdEnd(UnitdData **unitdData)
{
    arrayRelease(&UNITD_ENV_VARS);
    objectRelease(&STATE_CMDLINE_DIR);
    timeRelease(&BOOT_START);
    timeRelease(&BOOT_STOP);
    objectRelease(&UNITS_USER_LOCAL_PATH);
    objectRelease(&UNITD_USER_CONF_PATH);
    objectRelease(&UNITD_USER_LOG_PATH);
    objectRelease(&UNITS_USER_ENAB_PATH);
    objectRelease(&SOCKET_USER_PATH);
    if (*unitdData) {
        arrayRelease(&(*unitdData)->bootUnits);
        arrayRelease(&(*unitdData)->initUnits);
        arrayRelease(&(*unitdData)->units);
        arrayRelease(&(*unitdData)->shutDownUnits);
        arrayRelease(&(*unitdData)->finalUnits);
        objectRelease(unitdData);
    }
}
