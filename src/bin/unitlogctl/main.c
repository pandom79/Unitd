/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd/unitlogd_impl.h"

bool UNITLOGCTL_DEBUG = false;

static void
showUsage()
{
    fprintf(stdout,
        "Usage: unitlogctl [COMMAND] [OPTIONS] ... \n\n"

        WHITE_UNDERLINE_COLOR"COMMAND\n"DEFAULT_COLOR
        "show-log           Show the log\n"
        "list-boots         List the boots\n"
        "show-boot          Show the boot\n"
        "index-repair       Repair the index\n"

        WHITE_UNDERLINE_COLOR"\nOPTIONS\n"DEFAULT_COLOR
        "-f, --follow       Follow the log\n"
        "-p, --pager        Enable the pager\n"
        "-d, --debug        Enable the debug\n"
        "-h, --help         Show usage\n\n"
    );
}

int main(int argc, char **argv) {

    int c, rv, userId = -1;
    const char *shortopts = "fphd", *commandName = NULL, *arg = NULL;
    const struct option longopts[] = {
        { "follow", optional_argument, NULL, 'f' },
        { "pager", optional_argument, NULL, 'p' },
        { "help", no_argument, NULL, 'h' },
        { "debug", optional_argument, NULL, 'd' },
        { 0, 0, 0, 0 }
    };
    UlCommand ulCommand = NO_UL_COMMAND;
    bool pager, follow;
    c = rv = 0;
    pager = follow = false;

    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
            case 'f':
                follow = true;
                break;
            case 'p':
                pager = true;
                break;
            case 'd':
                UNITLOGCTL_DEBUG = true;
                break;
            case 'h':
                showUsage();
                rv = 0;
                goto out;
            default:
                showUsage();
                rv = 1;
                goto out;
        }
    }

    /* Assert the macros */
    assertMacros();

    /* Get the command */
    if ((commandName = argv[optind])) {
        ulCommand = getUlCommand(commandName);
        if (ulCommand == NO_UL_COMMAND) {
            showUsage();
            rv = 1;
            goto out;
        }
    }

    /* Show-log is the default command */
    if (ulCommand == NO_UL_COMMAND)
        ulCommand = SHOW_LOG;

    /* Check administrator */
    userId = getuid();
    if (userId != 0) {
        if (!getSkipCheckAdmin(ulCommand)) {
            rv = checkAdministrator(argv);
            goto out;
        }
    }

    /* Command handling */
    switch (ulCommand) {
        case SHOW_LOG:
            if (argc > 5 || (argc > 3 && !UNITLOGCTL_DEBUG && !pager && !follow)) {
                showUsage();
                rv = 1;
                goto out;
            }
            rv = showLog(pager, follow);
            break;
        case LIST_BOOTS:
            if (argc > 3 || (argc > 2 && !UNITLOGCTL_DEBUG)) {
                showUsage();
                rv = 1;
                goto out;
            }
            rv = showBootsList();
            break;
        case SHOW_BOOT:
            if (argc < 3 || argc > 6 || (argc > 3 && !UNITLOGCTL_DEBUG && !pager && !follow)) {
                showUsage();
                rv = 1;
                goto out;
            }
            arg = argv[argc - 1];
            rv = showBoot(pager, follow, arg);
            break;
        case INDEX_REPAIR:
            if (argc < 2 || argc > 3 || (argc > 2 && !UNITLOGCTL_DEBUG)) {
                showUsage();
                rv = 1;
                goto out;
            }
            rv = indexRepair();
            break;
        default:
            break;
    }

    out:

        return rv;
}
