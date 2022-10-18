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
        "Usage: unitlogctl [COMMAND] [OPTIONS] \n\n"

        WHITE_UNDERLINE_COLOR"COMMAND\n"DEFAULT_COLOR
        "list-boots         List the boots\n"

        WHITE_UNDERLINE_COLOR"\nOPTIONS\n"DEFAULT_COLOR
        "-d, --debug        Enable the debug\n"
        "-h, --help         Show usage\n\n"
    );
}

int main(int argc, char **argv) {

    int c, rv, userId = -1;
    const char *shortopts = "hd", *commandName = NULL;
    const struct option longopts[] = {
        { "help", no_argument, NULL, 'h' },
        { "debug", optional_argument, NULL, 'd' },
        { 0, 0, 0, 0 }
    };
    UlCommand ulCommand = NO_UL_COMMAND;
    c = rv = 0;

    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
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
        case LIST_BOOTS:
            rv = showBootsList();
            break;
        default:
            break;
    }

    out:

        return rv;
}
