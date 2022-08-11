/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../core/unitd_impl.h"

bool UNITCTL_DEBUG = false;
bool USER_INSTANCE;
char *SOCKET_USER_PATH;

static void __attribute__((noreturn))
usage(bool fail)
{
    if (USER_INSTANCE) {
        fprintf(stdout,
            "Usage for user instance: unitctl [COMMAND] [OPTIONS] ... \n\n"

            WHITE_UNDERLINE_COLOR"COMMAND\n"DEFAULT_COLOR
            "stop               Stop the unit\n"
            "status             Get the unit status\n"
            "list-requires      List the unit dependencies\n"
            "list-conflicts     List the unit conflicts\n"
            "list-states        List the unit wanted states\n"
            "list               List the units\n"
            "analyze            Analyze the user instance boot process\n"
            "poweroff           Shutdown the user instance and exit\n"

            WHITE_UNDERLINE_COLOR"\nOPTIONS\n"DEFAULT_COLOR
            "-d, --debug        Enable the debug\n"
            "-h, --help         Show usage\n\n"
        );
    }
    else {
        fprintf(stdout,
            "Usage for system instance: unitctl [COMMAND] [OPTIONS] ... \n\n"

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
            "get-default        Get the default state\n"
            "set-default        Set the default state\n"
            "cat                Show the unit content\n"
            "edit               Edit the unit content\n"
            "list               List the units\n"
            "analyze            Analyze the system boot process\n"
            "reboot             Shutdown and reboot the system\n"
            "poweroff           Shutdown and power off the system\n"
            "halt               Shutdown and halt the system\n"
            "kexec              Shutdown and reboot the system with kexec\n"

            WHITE_UNDERLINE_COLOR"\nOPTIONS\n"DEFAULT_COLOR
            "-r, --run          Run the operation\n"
            "-f, --force        Force the operation\n"
            "-d, --debug        Enable the debug\n"
            "-n, --no-wtmp      Don't write a wtmp record\n"
            "-o, --only-wtmp    Only write a wtmp/utmp reboot record and exit\n"
            "-w, --no-wall      Don't write a message to all users\n"
            "-u, --user         Connect to user unitd instance\n"
            "-h, --help         Show usage\n\n"
        );
    }

    /* Release resources */
    objectRelease(&SOCKET_USER_PATH);
    exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int c, rv, userId;
    bool force, run, noWtmp, onlyWtmp, noWall;
    const char *shortopts = "hrfdnowu";
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
        { 0, 0, 0, 0 }
    };

    c = rv = userId = 0;
    commandName = arg = NULL;
    force = run = noWtmp = onlyWtmp = noWall = false;

    /* Get options */
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
            case 'h':
                usage(false);
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
            default:
                usage(true);
        }
    }

    /* Get user id */
    userId = getuid();
    assert(userId >= 0);

    /* Check user instance and set user socket path */
    if (!USER_INSTANCE) {
        if (userId != 0) {
            rv = checkAdministrator(argv);
            goto out;
        }
    }
    else {
        if ((rv = setUserSocketPath(userId)) != 0)
            goto out;
        assert(SOCKET_USER_PATH);
    }

    /* Get the command */
    if ((commandName = argv[optind])) {
        command = getCommand(commandName);
        if (command == NO_COMMAND)
            usage(true);
    }

    /* Command handling */
    switch (command) {
        case NO_COMMAND:
            if (argc > 4 ||
               (argc > 1 && !UNITCTL_DEBUG && !onlyWtmp && !USER_INSTANCE))
                usage(true);
            else {
                if (onlyWtmp) {
                    if (!USER_INSTANCE) {
                        /* Write a wtmp/utmp 'reboot' record and exit */
                        rv = writeWtmp(true);
                        if (rv != 0)
                            unitdLogErrorStr(LOG_UNITD_CONSOLE, "An error has occurred in writeWtmp!\n");
                    }
                    else usage(true);
                }
                else
                    /* List command as default */
                    rv = showUnitList(&sockMessageOut);
            }
            break;
        case REBOOT_COMMAND:
        case POWEROFF_COMMAND:
        case HALT_COMMAND:
        case KEXEC_COMMAND:
            if (!USER_INSTANCE) {
                if (argc > 6 ||
                   (argc > 2 && !force && !UNITCTL_DEBUG && !noWtmp && !noWall))
                    usage(true);

                if (command == KEXEC_COMMAND && !isKexecLoaded()) {
                    rv = EPERM;
                    unitdLogErrorStr(LOG_UNITD_CONSOLE, "Kexec is not loaded!\n");
                    unitdLogInfo(LOG_UNITD_CONSOLE, "Please, run 'unitctl reboot' to reboot the system.\n");
                    goto out;
                }
            }
            else {
                /* We only use POWEROFF_COMMAND to shutdown an unitd user instance.
                 * We don't allow force, noWtmp and noWall option.
                */
                if (argc > 4 || command != POWEROFF_COMMAND ||
                   (argc > 2 && (force || noWtmp || noWall)))
                    usage(true);
            }
            rv = unitdShutdown(command, force, noWtmp, noWall);
            break;
        case LIST_COMMAND:
        case GET_DEFAULT_STATE_COMMAND:
            if (argc > 4 ||
               (argc > 2 && !UNITCTL_DEBUG && !USER_INSTANCE) ||
               (command == GET_DEFAULT_STATE_COMMAND && USER_INSTANCE))
                usage(true);
            if (command == LIST_COMMAND)
                rv = showUnitList(&sockMessageOut);
            else
                rv = showUnit(command, &sockMessageOut, NULL, false, false, false, false);
            break;
        case STATUS_COMMAND:
        case STOP_COMMAND:
        case LIST_REQUIRES_COMMAND:
        case LIST_CONFLICTS_COMMAND:
        case LIST_STATES_COMMAND:
        case SET_DEFAULT_STATE_COMMAND:
            if (argc < 3 || argc > 5 ||
               (argc > 3 && !UNITCTL_DEBUG && !USER_INSTANCE) ||
               (command == SET_DEFAULT_STATE_COMMAND && USER_INSTANCE))
                usage(true);
            arg = argv[argc - 1];
            if (command == STATUS_COMMAND)
                rv = showUnitStatus(&sockMessageOut, arg);
            else
                rv = showUnit(command, &sockMessageOut, arg, false, false, false, false);
            break;
        case START_COMMAND:
        case RESTART_COMMAND:
            if (argc < 3 || argc > 6 ||
               (argc > 3 && !force && !UNITCTL_DEBUG && !USER_INSTANCE))
                usage(true);
            arg = argv[argc - 1];
            rv = showUnit(command, &sockMessageOut, arg, force,
                          command == START_COMMAND ? false : true,
                          false, false);
            break;
        case DISABLE_COMMAND:
            if (argc == 2 || (argc > 3 && !run && !UNITCTL_DEBUG))
                usage(true);
            arg = argv[argc - 1];
            rv = showUnit(command, &sockMessageOut, arg, false, false, run, false);
            break;
        case RE_ENABLE_COMMAND:
        case ENABLE_COMMAND:
            if (argc == 2 || (argc > 3 && !run && !force && !UNITCTL_DEBUG))
                usage(true);
            arg = argv[argc - 1];
            if (command == ENABLE_COMMAND)
                rv = showUnit(command, &sockMessageOut, arg, force, false, run, false);
            else
                rv = showUnit(command, &sockMessageOut, arg, force, false, run, true);
            break;
        case CAT_COMMAND:
        case EDIT_COMMAND:
            if (argc != 3)
                usage(true);
            else
                arg = argv[argc -1];
            rv = catEditUnit(command, arg);
            break;
        case ANALYZE_COMMAND:
            if (argc > 4 || (argc == 4 && !UNITCTL_DEBUG && !USER_INSTANCE))
                usage(true);
            rv = showBootAnalyze(&sockMessageOut);
        }

    out:
        /* Release resources */
        objectRelease(&SOCKET_USER_PATH);

        return rv;
}
