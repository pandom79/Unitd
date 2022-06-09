/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../core/unitd_impl.h"

bool UNITCTL_DEBUG = false;

static void __attribute__((noreturn))
usage(bool fail)
{
    fprintf(stdout,
        "Usage: unitctl [COMMAND] [OPTIONS] ... \n\n"

        WHITE_UNDERLINE_COLOR"COMMAND\n"DEFAULT_COLOR
        "enable             Enable the unit\n"
        "disable            Disable the unit\n"
        "restart            Restart the unit\n"
        "start              Start the unit\n"
        "stop               Stop the unit\n"
        "status             Get the unit status\n"
        "list-requires      List all the unit dependencies\n"
        "list-conflicts     List all the unit conflicts\n"
        "list-states        List all the unit wanted states\n"
        "get-default        Get the default state\n"
        "set-default        Set the default state\n"
        "cat                Show the unit content\n"
        "edit               Edit the unit content\n"
        "list               List all the units\n"
        "analyze            Analyze the boot process"
        "reboot             Shutdown and reboot the system\n"
        "poweroff           Shutdown and power off the system\n"
        "halt               Shutdown and halt the system\n"
        "kexec              Shutdown and reboot the system with kexec\n\n"

        WHITE_UNDERLINE_COLOR"OPTIONS\n"DEFAULT_COLOR
        "-r, --run          Run the operation\n"
        "-f, --force        Force the operation\n"
        "-d, --debug        Enable the debug\n"
        "-n, --no-wtmp      Don't write a wtmp record\n"
        "-o, --only-wtmp    Only write a wtmp/utmp reboot record and exit\n"
        "-w, --no-wall      Don't write a message to all users\n"
        "-h, --help         Show usage\n\n"
    );
    exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int c, rv;
    bool force, run, noWtmp, onlyWtmp, noWall;
    const char *shortopts = "hrfdnow";
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
        { 0, 0, 0, 0 }
    };

    c = rv = 0;
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
            default:
                usage(true);
        }
    }

    if (getuid() != 0) {
        rv = checkAdministrator(argv);
        exit(rv);
    }

    /* Get the command */
    if ((commandName = argv[optind]))
        command = getCommand(commandName);

    /* Command handling */
    switch (command) {
        case NO_COMMAND:
            if ((argc >= 3 && UNITCTL_DEBUG && onlyWtmp) ||
                (argc >= 2 && (!UNITCTL_DEBUG || !onlyWtmp)))
                usage(true);
            else {
                if (onlyWtmp) {
                    /* Write a wtmp/utmp 'reboot' record and exit */
                    rv = writeWtmp(true);
                    if (rv != 0)
                        unitdLogErrorStr(LOG_UNITD_CONSOLE, "An error has occurred in writeWtmp!\n");
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
            if (argc > 2) {
                if (argc > 6 || (!force && !UNITCTL_DEBUG && !noWtmp && !noWall))
                    usage(true);
            }
            if (command == KEXEC_COMMAND && !isKexecLoaded()) {
                unitdLogErrorStr(LOG_UNITD_CONSOLE, "Kexec is not loaded!\n");
                unitdLogInfo(LOG_UNITD_CONSOLE, "Please, run 'unitctl reboot' to reboot the system.\n");
                return rv;
            }
            rv = unitdShutdown(command, force, noWtmp, noWall);
            break;
        case LIST_COMMAND:
        case GET_DEFAULT_STATE_COMMAND:
            if ((argc > 2 && !UNITCTL_DEBUG) || argc > 3)
                usage(true);
            if (command == LIST_COMMAND)
                rv = showUnitList(&sockMessageOut);
            else
                rv = showUnit(command, &sockMessageOut, NULL, false, false, false);
            break;
        case STATUS_COMMAND:
        case STOP_COMMAND:
        case LIST_REQUIRES_COMMAND:
        case LIST_CONFLICTS_COMMAND:
        case LIST_STATES_COMMAND:
        case SET_DEFAULT_STATE_COMMAND:
            if (argc == 2 || (argc > 3 && !UNITCTL_DEBUG))
                usage(true);
            if (argc > 3)
                arg = argv[3];
            else
                arg = argv[2];
            if (command == STATUS_COMMAND)
                rv = showUnitStatus(&sockMessageOut, arg);
            else
                rv = showUnit(command, &sockMessageOut, arg, false, false, false);
            break;
        case START_COMMAND:
        case RESTART_COMMAND:
            if (argc == 2 || (argc > 3 && !force && !UNITCTL_DEBUG))
                usage(true);
            arg = argv[argc - 1];
            if (command == START_COMMAND)
                rv = showUnit(command, &sockMessageOut, arg, force, false, false);
            else
                rv = showUnit(command, &sockMessageOut, arg, force, true, false);
            break;
        case DISABLE_COMMAND:
            if (argc == 2 || (argc > 3 && !run && !UNITCTL_DEBUG))
                usage(true);
            arg = argv[argc - 1];
            rv = showUnit(command, &sockMessageOut, arg, false, false, run);
            break;
        case ENABLE_COMMAND:
            if (argc == 2 || (argc > 3 && !run && !force && !UNITCTL_DEBUG))
                usage(true);
            arg = argv[argc - 1];
            rv = showUnit(command, &sockMessageOut, arg, force, false, run);
            break;
        case CAT_COMMAND: case EDIT_COMMAND:
            if (argc != 3)
                usage(true);
            else
                arg = argv[argc -1];
            rv = catEditUnit(command, arg);
            break;
        case ANALYZE_COMMAND:
            if (argc > 3 || (argc == 3 && !UNITCTL_DEBUG))
                usage(true);
            rv = showBootAnalyze(&sockMessageOut);
        }


    return rv;
}
