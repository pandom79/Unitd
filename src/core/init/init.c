/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

/* Options */
OptionData OPTIONS_DATA[] = {
    { FORCE_OPT, "force" },       { RESTART_OPT, "restart" },     { RUN_OPT, "run" },
    { REQUIRES_OPT, "requires" }, { CONFLICTS_OPT, "conflicts" }, { STATES_OPT, "states" },
    { NO_WTMP_OPT, "no-wtmp" },   { ANALYZE_OPT, "analyze" },     { RE_ENABLE_OPT, "re-enable" },
    { RESET_OPT, "reset" }
};
int OPTIONS_LEN = 10;

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
    { CREATE_COMMAND, "create" },
    { LIST_ENABLED_COMMAND, "list-enabled" },
    { LIST_DISABLED_COMMAND, "list-disabled" },
    { LIST_STARTED_COMMAND, "list-started" },
    { LIST_DEAD_COMMAND, "list-dead" },
    { LIST_FAILED_COMMAND, "list-failed" },
    { LIST_RESTARTABLE_COMMAND, "list-restartable" },
    { LIST_RESTARTED_COMMAND, "list-restarted" },
    { LIST_TIMERS_COMMAND, "list-timers" },
    { LIST_UPATH_COMMAND, "list-paths" },
};
int COMMANDS_LEN = 30;

/* List Filter */
const ListFilterData LIST_FILTER_DATA[] = {
    { ENABLED_FILTER, "enable" },      { DISABLED_FILTER, "disable" },
    { STARTED_FILTER, "started" },     { DEAD_FILTER, "dead" },
    { FAILED_FILTER, "failed" },       { RESTARTABLE_FILTER, "restartable" },
    { RESTARTED_FILTER, "restarted" }, { TIMERS_FILTER, "timers" },
    { UPATH_FILTER, "upath" }
};
int LIST_FILTER_LEN = 9;

/* Unitd errors */
const UnitdErrorsData UNITD_ERRORS_ITEMS[] = { { UNITD_GENERIC_ERR, "An error has occurred!" },
                                               { UNITD_SOCKBUF_ERR,
                                                 "Unable to receive the data!" } };
/* Unitd messages */
const UnitdMessagesData UNITD_MESSAGES_ITEMS[] = {
    { UNITD_SYSTEM_LOG_MSG, "Please, check the system log for details." },
    { UNITD_SOCKBUF_MSG, "Have been requested '%lu' bytes but only '%lu' are available.\n"
                         "Please, increase the socket buffer size." }
};

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
pthread_mutex_t START_MUTEX;
pthread_mutex_t NOTIFIER_MUTEX;

static void addBootUnits(Array **bootUnits, Array **units)
{
    if (!(*bootUnits))
        *bootUnits = arrayNew(unitRelease);
    int len = (*units ? (*units)->size : 0);
    for (int i = 0; i < len; i++)
        arrayAdd(*bootUnits, unitNew(arrayGet(*units, i), PARSE_SOCK_RESPONSE_UNITLIST));
}

int unitdInit(UnitdData **unitdData, bool isAggregate)
{
    int rv = 0;
    char *initStateDir = NULL, *destDefStateSyml = NULL, *finalStateDir = NULL,
         *shutDownStateStr = NULL;
    Array **initUnits = NULL, **finalUnits = NULL, **units = NULL, **shutDownUnits = NULL,
          **bootUnits = NULL;

    assert(*unitdData);

    initUnits = &(*unitdData)->initUnits;
    units = &(*unitdData)->units;
    shutDownUnits = &(*unitdData)->shutDownUnits;
    finalUnits = &(*unitdData)->finalUnits;
    bootUnits = &(*unitdData)->bootUnits;
    if (UNITD_DEBUG) {
        logInfo(CONSOLE, "%s starting as pid %d\n", PROJECT_NAME, UNITD_PID);
        logInfo(CONSOLE, "Units path = %s\n", UNITS_PATH);
        logInfo(CONSOLE, "Units enab path = %s\n", UNITS_ENAB_PATH);
        logInfo(CONSOLE, "Unitd data path = %s\n", UNITD_DATA_PATH);
        logInfo(CONSOLE, "Unitd Log path = %s\n", UNITD_LOG_PATH);
        logInfo(CONSOLE, "Debug = %s\n", UNITD_DEBUG ? "True" : "False");
    }
    /* For each terminated state, we check if "SHUTDOWN_COMMAND" is set by signal handler.
     * If so (Ctrl+Alt+Del pressed), we start the reboot phase
    */
    //*************************** INIT STATE *********************************
    /* Parsing and starting the initialization units.
     * For the initialization state, we always aggregate the errors to allow
     * to fix them in an one shot
    */
#ifndef UNITD_TEST
    initStateDir = stringNew(STATE_DATA_ITEMS[INIT].desc);
    stringAppendStr(&initStateDir, ".state/units");
    if ((rv = loadUnits(initUnits, UNITD_DATA_PATH, initStateDir, INIT, true, NULL, PARSE_UNIT,
                        true)) != 0) {
        /* Show unit configuration error and emergency shell */
        logError(ALL, "src/core/init/init.c", "unitdInit", rv,
                 "An error has occurred in loadUnits for init.state", NULL);
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto out;
    }
    if ((rv = startProcesses(initUnits, NULL)) != 0) {
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto out;
    }
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;
#endif
    assert(!UNITD_BOOT_LOG_FILE);
    unitdOpenLog("w");
    /* Start the cleaner */
    startCleaner();
    /* Start the notifiers */
    startNotifier(NULL);
    //******************* DEFAULT OR CMDLINE STATE ************************
    /* Set the default state variable.
     * Actually, the following errors should never happen because the unitd-check initialization
     * unit which makes this job has been already called.
     */
    if (getDefaultStateStr(&destDefStateSyml) != 0) {
        /* If we are here then the symlink is not valid or missed */
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        /* Set the default shutdown command */
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto shutdown;
    }
    STATE_DEFAULT = getStateByStr(destDefStateSyml);
    if (STATE_DEFAULT == NO_STATE) {
        /* If we are here then the symlink points to a bad destination */
        logErrorStr(CONSOLE, "The default state symlink points to a bad destination (%s)\n",
                    destDefStateSyml);
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        /* Set the default shutdown command */
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto shutdown;
    }
    assert(STATE_DEFAULT != NO_STATE);
    if (UNITD_DEBUG)
        logInfo(UNITD_BOOT_LOG, "The default.state symlink points to %s\n",
                STATE_DATA_ITEMS[STATE_DEFAULT].desc);
    /* Parsing the units for the cmdline or default state */
    if (STATE_CMDLINE_DIR) {
        if (UNITD_DEBUG)
            logInfo(UNITD_BOOT_LOG, "The state of the cmdline is %s\n",
                    STATE_DATA_ITEMS[STATE_CMDLINE].desc);
        rv = loadUnits(units, UNITS_ENAB_PATH, STATE_CMDLINE_DIR, STATE_CMDLINE, isAggregate, NULL,
                       PARSE_UNIT, true);
    } else {
        rv = loadUnits(units, UNITS_ENAB_PATH, DEF_STATE_SYML_NAME, STATE_DEFAULT, isAggregate,
                       NULL, PARSE_UNIT, true);
    }
    /* Zero units are not allowed in this state (default/cmdline) */
    if (rv == GLOB_NOMATCH) {
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        /* Set the default shutdown command */
        SHUTDOWN_COMMAND = REBOOT_COMMAND;
        goto shutdown;
    }
    /* We put the pipes to listen before to start processes to be ready for restart. */
    listenPipes(units, NULL);
    startProcesses(units, NULL);
    unitdCloseLog();
    /* Create the boot units array */
    addBootUnits(bootUnits, units);
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;
    /* Unitd is blocked here listening the client requests */
    listenSocketRequest();

shutdown:
    /* Shutdown start */
    SHUTDOWN_START = timeNew(NULL);
    /* Open the log in append mode if it is closed */
    if (!UNITD_BOOT_LOG_FILE)
        unitdOpenLog("a");
    /* Stop the cleaner */
    stopCleaner();
    /* Stop the notifiers */
    stopNotifier(NULL);
    //******************* POWEROFF (HALT) / REBOOT STATE **********************
    logInfo(CONSOLE | UNITD_BOOT_LOG, "%sSystem is going down ...%s\n", WHITE_COLOR, DEFAULT_COLOR);
    if (SHUTDOWN_COMMAND == HALT_COMMAND) {
        shutDownStateStr = stringNew(COMMANDS_DATA[POWEROFF_COMMAND].name);
        stringAppendStr(&shutDownStateStr, ".state");
    } else {
        shutDownStateStr = stringNew(COMMANDS_DATA[SHUTDOWN_COMMAND].name);
        stringAppendStr(&shutDownStateStr, ".state");
    }
    STATE_SHUTDOWN = getStateByStr(shutDownStateStr);
    loadUnits(shutDownUnits, UNITS_ENAB_PATH, shutDownStateStr, STATE_SHUTDOWN, isAggregate, NULL,
              PARSE_UNIT, true);
    startProcesses(shutDownUnits, NULL);
    //********************* STOPPING UNITS **********************************
    closePipes(units, NULL);
    stopProcesses(units, NULL);
    stopProcesses(shutDownUnits, NULL);
    stopProcesses(initUnits, NULL);
    unitdCloseLog();
    /* Write a wtmp 'shutdown' record */
    if (!NO_WTMP) {
        rv = writeWtmp(false);
    }
    //********************* FINAL STATE ************************************
    /* Parsing and starting the finalization units.
     * For the finalization state, we always aggregate the errors to allow
     * to fix them in an one shot.
     */
#ifndef UNITD_TEST
    finalStateDir = stringNew(STATE_DATA_ITEMS[FINAL].desc);
    stringAppendStr(&finalStateDir, ".state/units");
    if ((rv = loadUnits(finalUnits, UNITD_DATA_PATH, finalStateDir, FINAL, true, NULL, PARSE_UNIT,
                        true)) != 0) {
        /* Show unit configuration error and emergency shell */
        logError(ALL, "src/core/init/init.c", "unitdInit", rv,
                 "An error has occurred in loadUnits for final.state", NULL);
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
        goto out;
    }
    if ((rv = startProcesses(finalUnits, NULL)) != 0)
        execScript(UNITD_DATA_PATH, "/scripts/emergency-shell.sh", NULL, NULL);
#endif

out:
    assert(!UNITD_BOOT_LOG_FILE);
    objectRelease(&initStateDir);
    objectRelease(&destDefStateSyml);
    objectRelease(&shutDownStateStr);
    objectRelease(&finalStateDir);
    return rv;
}

int unitdUserInit(UnitdData **unitdData, bool isAggregate)
{
    int rv = 0;
    Array **units = NULL, **bootUnits = NULL;

    assert(*unitdData);

    units = &(*unitdData)->units;
    if (!(*units))
        *units = arrayNew(unitRelease);
    bootUnits = &(*unitdData)->bootUnits;
    if (UNITD_DEBUG) {
        logInfo(UNITD_BOOT_LOG, "%s starting as pid %d\n", PROJECT_USER_NAME, UNITD_PID);
        logInfo(UNITD_BOOT_LOG, "Units user path = %s\n", UNITS_USER_PATH);
        logInfo(UNITD_BOOT_LOG, "Units user local path = %s\n", UNITS_USER_LOCAL_PATH);
        logInfo(UNITD_BOOT_LOG, "Units user conf path = %s\n", UNITD_USER_CONF_PATH);
        logInfo(UNITD_BOOT_LOG, "Unitd user log path = %s\n", UNITD_USER_LOG_PATH);
        logInfo(UNITD_BOOT_LOG, "Units user enab path = %s\n", UNITS_USER_ENAB_PATH);
        logInfo(UNITD_BOOT_LOG, "socket user path = %s\n", SOCKET_USER_PATH);
        logInfo(UNITD_BOOT_LOG, "Debug = %s\n", UNITD_DEBUG ? "True" : "False");
    }
    /* Start the cleaner */
    startCleaner();
    /* Start the notifiers */
    startNotifier(NULL);
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;
    //******************* USER STATE ************************
    /* Parsing the units for the user state */
    STATE_USER_DIR = stringNew(STATE_DATA_ITEMS[USER].desc);
    stringAppendStr(&STATE_USER_DIR, ".state");
    rv = loadUnits(units, UNITS_USER_ENAB_PATH, STATE_USER_DIR, STATE_USER, isAggregate, NULL,
                   PARSE_UNIT, true);
    /* We put the pipes to listen before to start processes to be ready for restart. */
    listenPipes(units, NULL);
    startProcesses(units, NULL);
    unitdCloseLog();
    /* Create the boot units array */
    addBootUnits(bootUnits, units);
    if (SHUTDOWN_COMMAND == REBOOT_COMMAND)
        goto shutdown;
    /* Unitd is blocked here listening the client requests */
    listenSocketRequest();

shutdown:
    /* Shutdown start */
    SHUTDOWN_START = timeNew(NULL);
    /* Open the log in append mode if it is closed */
    if (!UNITD_BOOT_LOG_FILE)
        unitdOpenLog("a");
    /* Stop the cleaner */
    stopCleaner();
    /* Stop the notifiers */
    stopNotifier(NULL);
    //********************* STOPPING UNITS **********************************
    closePipes(units, NULL);
    stopProcesses(units, NULL);

    /* Release resources */
    objectRelease(&STATE_USER_DIR);
    return rv;
}

void unitdEnd(UnitdData **unitdData)
{
    int rv = 0;
    arrayRelease(&UNITD_ENV_VARS);
    objectRelease(&STATE_CMDLINE_DIR);
    timeRelease(&BOOT_START);
    timeRelease(&BOOT_STOP);
    userDataRelease();
    notifierRelease(&NOTIFIER);
    cleanerRelease(&CLEANER);
    if ((rv = pthread_mutex_destroy(&START_MUTEX)) != 0)
        logError(CONSOLE | SYSTEM, "src/core/init/init.c", "unitdEnd", rv, strerror(rv),
                 "Unable to destroy the start mutex");
    if ((rv = pthread_mutex_destroy(&NOTIFIER_MUTEX)) != 0)
        logError(CONSOLE | SYSTEM, "src/core/init/init.c", "unitdEnd", rv, strerror(rv),
                 "Unable to destroy the notifier mutex");
    if (*unitdData) {
        arrayRelease(&(*unitdData)->bootUnits);
        arrayRelease(&(*unitdData)->initUnits);
        arrayRelease(&(*unitdData)->units);
        arrayRelease(&(*unitdData)->shutDownUnits);
        arrayRelease(&(*unitdData)->finalUnits);
        objectRelease(unitdData);
    }
}
