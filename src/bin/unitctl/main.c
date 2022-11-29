/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../core/unitd_impl.h"

bool UNITCTL_DEBUG = false;
bool USER_INSTANCE;
char *UNITS_USER_LOCAL_PATH;
char *UNITS_USER_ENAB_PATH;
char *UNITD_USER_CONF_PATH;
char *UNITD_USER_TIMER_DATA_PATH;
char *UNITD_USER_LOG_PATH;
char *SOCKET_USER_PATH;

static bool
getSkipCheckAdmin(Command command)
{
    switch (command) {
        case STATUS_COMMAND:
        case LIST_REQUIRES_COMMAND:
        case LIST_CONFLICTS_COMMAND:
        case LIST_STATES_COMMAND:
        case CAT_COMMAND:
        case LIST_COMMAND:
        case LIST_ENABLED_COMMAND:
        case LIST_DISABLED_COMMAND:
        case LIST_STARTED_COMMAND:
        case LIST_DEAD_COMMAND:
        case LIST_FAILED_COMMAND:
        case LIST_RESTARTABLE_COMMAND:
        case LIST_RESTARTED_COMMAND:
        case LIST_TIMERS_COMMAND:
        case ANALYZE_COMMAND:
        case GET_DEFAULT_STATE_COMMAND:
            return true;
        default:
            return false;
    }
}

static void
showUsage()
{
    fprintf(stdout,
            "Usage for %s instance: unitctl [COMMAND] [OPTIONS] ... \n\n",
            !USER_INSTANCE ? "system" : "user");

    /* Commands */
    fprintf(stdout,
            WHITE_UNDERLINE_COLOR"COMMAND\n"DEFAULT_COLOR
            "enable             Enable the unit\n"
            "re-enable          Re-enable the unit\n"
            "disable            Disable the unit\n"
            "restart            Restart the unit\n"
            "start              Start the unit\n"
            "stop               Stop the unit\n"
            "status             Get the unit status\n"
            "list-requires      List the unit dependencies\n"
            "list-conflicts     List the unit conflicts\n"
            "list-states        List the unit wanted states\n"
            "cat                Show the unit content\n"
            "edit               Edit the unit content\n"
            "create             Create the unit\n"
            "list               List the units\n"
            "list-enabled       List the enabled units\n"
            "list-disabled      List the disabled units\n"
            "list-started       List the started units\n"
            "list-dead          List the dead units\n"
            "list-failed        List the failed units\n"
            "list-restartable   List the restartable units\n"
            "list-restarted     List the restarted units\n"
            "list-timers        List the timers\n"
    );
    fprintf(stdout,
            "analyze            Analyze the %s boot process\n",
            !USER_INSTANCE ? "system" : "user instance"
    );
    fprintf(stdout, !USER_INSTANCE ?
            "poweroff           Shutdown and power off the system\n" :
            "poweroff           Shutdown the user instance and exit\n"
    );
    if (!USER_INSTANCE) {
        fprintf(stdout,
            "reboot             Shutdown and reboot the system\n"
            "halt               Shutdown and halt the system\n"
            "kexec              Shutdown and reboot the system with kexec\n"
            "get-default        Get the default state\n"
            "set-default        Set the default state\n"
        );
    }

    /* Options */
    fprintf(stdout,
            WHITE_UNDERLINE_COLOR"\nOPTIONS\n"DEFAULT_COLOR
            "-e, --reset        Reset the timer\n"
            "-r, --run          Run the operation\n"
            "-f, --force        Force the operation\n"
            "-d, --debug        Enable the debug\n"
            "-h, --help         Show usage\n"
    );
    if (!USER_INSTANCE) {
        fprintf(stdout,
            "-n, --no-wtmp      Don't write a wtmp record\n"
            "-o, --only-wtmp    Only write a wtmp/utmp reboot record and exit\n"
            "-w, --no-wall      Don't write a message to all users\n"
            "-u, --user         Connect to user unitd instance\n"
        );
    }
    printf("\n");
}

int main(int argc, char **argv) {
    int c, rv, userId;
    bool force, run, noWtmp, onlyWtmp, noWall, skipCheckAdmin, usage, reset;
    const char *shortopts = "hrfdnowue";
    Command command = NO_COMMAND;
    const char *commandName, *arg;
    SockMessageOut *sockMessageOut = NULL;
    const struct option longopts[] = {
        { "help", no_argument, NULL, 'h' },
        { "run", optional_argument, NULL, 'r' },
        { "no-wtmp", optional_argument, NULL, 'n' },
        { "only-wtmp", optional_argument, NULL, 'o' },
        { "no-wall", optional_argument, NULL, 'w' },
        { "force", optional_argument, NULL, 'f' },
        { "debug", optional_argument, NULL, 'd' },
        { "user", optional_argument, NULL, 'u' },
        { "reset", optional_argument, NULL, 'e' },
        { 0, 0, 0, 0 }
    };

    c = rv = userId = 0;
    commandName = arg = NULL;
    force = run = noWtmp = onlyWtmp = noWall = skipCheckAdmin = usage = reset = false;

    /* Get options */
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
            case 'h':
                usage = true;
                break;
            case 'r':
                run = true;
                break;
            case 'f':
                force = true;
                break;
            case 'n':
                noWtmp = true;
                break;
            case 'o':
                onlyWtmp = true;
                break;
            case 'w':
                noWall = true;
                break;
            case 'd':
                UNITCTL_DEBUG = true;
                break;
            case 'u':
                USER_INSTANCE = true;
                break;
            case 'e':
                reset = true;
                break;
            default:
                usage = true;
                rv = 1;
                break;
        }
    }

    if (usage) {
        showUsage();
        goto out;
    }

    /* Get the command */
    if ((commandName = argv[optind])) {
        command = getCommand(commandName);
        if (command == NO_COMMAND) {
            showUsage();
            rv = 1;
            goto out;
        }
    }

    /* Check instance */
    userId = getuid();
    assert(userId >= 0);
    if (!USER_INSTANCE) {
        /* The interaction commands require the administrator check.
         * For the consultation commands instead, it is not required.
        */
        if (command == NO_COMMAND && !onlyWtmp) //LIST_COMMAND is a default command
            skipCheckAdmin = true;
        else
            skipCheckAdmin = getSkipCheckAdmin(command);

        if (userId != 0 && !skipCheckAdmin) {
            rv = checkAdministrator(argv);
            goto out;
        }
    }
    else {
        /* Set user data */
        struct passwd *userInfo = NULL;
        if ((rv = setUserData(userId, &userInfo)) != 0)
            goto out;
    }

    /* Command handling */
    switch (command) {
        case NO_COMMAND:
            if (argc > 4 ||
               (argc > 1 && !UNITCTL_DEBUG && !onlyWtmp && !USER_INSTANCE)) {
                showUsage();
                rv = 1;
                goto out;
            }
            else {
                if (onlyWtmp) {
                    if (!USER_INSTANCE) {
                        /* Write a wtmp/utmp 'reboot' record and exit */
                        rv = writeWtmp(true);
                    }
                    else {
                        showUsage();
                        rv = 1;
                        goto out;
                    }
                }
                else
                    /* List command as default */
                    rv = showUnitList(&sockMessageOut, NO_FILTER);
            }
            break;
        case REBOOT_COMMAND:
        case POWEROFF_COMMAND:
        case HALT_COMMAND:
        case KEXEC_COMMAND:
            if (!USER_INSTANCE) {
                if (argc > 6 ||
                   (argc > 2 && !force && !UNITCTL_DEBUG && !noWtmp && !noWall)) {
                    showUsage();
                    rv = 1;
                    goto out;
                }
                if (command == KEXEC_COMMAND && !isKexecLoaded()) {
                    rv = 1;
                    logErrorStr(CONSOLE, "Kexec is not loaded!\n");
                    logInfo(CONSOLE, "Please, run 'unitctl reboot' to reboot the system.\n");
                    goto out;
                }
            }
            else {
                /* We only use POWEROFF_COMMAND to shutdown an unitd user instance.
                 * We don't allow force, noWtmp and noWall option.
                */
                if (argc > 4 || command != POWEROFF_COMMAND ||
                   (argc > 2 && (force || noWtmp || noWall))) {
                    showUsage();
                    rv = 1;
                    goto out;
                }
            }
            rv = unitdShutdown(command, force, noWtmp, noWall);
            break;
        case LIST_COMMAND:
        case LIST_ENABLED_COMMAND:
        case LIST_DISABLED_COMMAND:
        case LIST_STARTED_COMMAND:
        case LIST_DEAD_COMMAND:
        case LIST_FAILED_COMMAND:
        case LIST_RESTARTABLE_COMMAND:
        case LIST_RESTARTED_COMMAND:
        case LIST_TIMERS_COMMAND:
        case ANALYZE_COMMAND:
        case GET_DEFAULT_STATE_COMMAND:
            if (argc > 4 ||
               (argc > 2 && !UNITCTL_DEBUG && !USER_INSTANCE) ||
               (command == GET_DEFAULT_STATE_COMMAND && USER_INSTANCE)) {
                showUsage();
                rv = 1;
                goto out;
            }
            switch (command) {
                case GET_DEFAULT_STATE_COMMAND:
                    rv = showData(command, &sockMessageOut, NULL, false, false, false, false, false);
                    break;
                case ANALYZE_COMMAND:
                    rv = showBootAnalyze(&sockMessageOut);
                    break;
                case LIST_TIMERS_COMMAND:
                    rv = showTimersList(&sockMessageOut, TIMERS_FILTER);
                    break;
                default:
                    rv = showUnitList(&sockMessageOut, getListFilterByCommand(command));
                    break;
            }
            break;
        case STATUS_COMMAND:
        case STOP_COMMAND:
        case LIST_REQUIRES_COMMAND:
        case LIST_CONFLICTS_COMMAND:
        case LIST_STATES_COMMAND:
        case SET_DEFAULT_STATE_COMMAND:
            if (argc < 3 || argc > 5 ||
               (argc > 3 && !UNITCTL_DEBUG && !USER_INSTANCE) ||
               (command == SET_DEFAULT_STATE_COMMAND && USER_INSTANCE)) {
                showUsage();
                rv = 1;
                goto out;
            }
            arg = argv[argc - 1];
            if (command == STATUS_COMMAND)
                rv = showUnitStatus(&sockMessageOut, arg);
            else
                rv = showData(command, &sockMessageOut, arg, false, false, false, false, false);
            break;
        case START_COMMAND:
        case RESTART_COMMAND:
            if (argc < 3 || argc > 7 ||
               (argc > 3 && !force && !UNITCTL_DEBUG && !USER_INSTANCE && !reset)) {
                showUsage();
                rv = 1;
                goto out;
            }
            arg = argv[argc - 1];
            rv = showData(command, &sockMessageOut, arg, force,
                          command == START_COMMAND ? false : true,
                          false, false, reset);
            break;
        case DISABLE_COMMAND:
            if (argc < 3 || argc > 6 ||
               (argc > 3 && !run && !UNITCTL_DEBUG && !USER_INSTANCE)) {
                showUsage();
                rv = 1;
                goto out;
            }
            arg = argv[argc - 1];
            rv = showData(command, &sockMessageOut, arg, false, false, run, false, false);
            break;
        case RE_ENABLE_COMMAND:
        case ENABLE_COMMAND:
            if (argc < 3 || argc > 7 ||
               (argc > 3 && !run && !force && !UNITCTL_DEBUG && !USER_INSTANCE)) {
                showUsage();
                rv = 1;
                goto out;
            }
            arg = argv[argc - 1];
            rv = showData(command, &sockMessageOut, arg, force, false, run,
                          command == ENABLE_COMMAND ? false : true, false);
            break;
        case CAT_COMMAND:
        case EDIT_COMMAND:
        case CREATE_COMMAND:
            if (argc < 3 || argc > 5 ||
               (argc > 3 && !UNITCTL_DEBUG && !USER_INSTANCE)) {
                showUsage();
                rv = 1;
                goto out;
            }
            arg = argv[argc -1];
            if (command == CREATE_COMMAND) {
                if ((rv = createUnit(arg)) != 0)
                    goto out;
                else
                    /* Edit the unit */
                    command = EDIT_COMMAND;
            }
            rv = catEditUnit(command, arg);
            break;
        }

    out:
        /* Release resources */
        userDataRelease();

        return rv;
}
