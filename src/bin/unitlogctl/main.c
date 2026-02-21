/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd/unitlogd_impl.h"

bool DEBUG = false;

static void showUsage()
{
    // clang-format off
    fprintf(stdout,
        "Usage: unitlogctl [COMMAND] [OPTIONS] ... \n\n"
        WHITE_UNDERLINE_COLOR"COMMAND\n"DEFAULT_COLOR
        "show-log           Show the log\n"
        "show-size          Show the log size\n"
        "list-boots         List the boots\n"
        "show-boot          Show the boot\n"
        "show-current       Show the current boot\n"
        "index-repair       Repair the index\n"
        "vacuum             Remove the log lines of the specified \n"
        "                   boot index or boot index range\n"
        WHITE_UNDERLINE_COLOR"\nOPTIONS\n"DEFAULT_COLOR
        "-f, --follow       Follow the log\n"
        "-p, --pager        Enable the pager\n"
        "-d, --debug        Enable the debug\n"
        "-v, --version      Show the version\n"
        "-h, --help         Show usage\n\n"
    );
    // clang-format on
}

int main(int argc, char **argv)
{
    int c = 0, rv = 0, userId = -1;
    const char *shortopts = "fphdv", *commandName = NULL, *arg = NULL;
    const struct option longopts[] = {
        { "follow", optional_argument, NULL, 'f' },  { "pager", optional_argument, NULL, 'p' },
        { "help", no_argument, NULL, 'h' },          { "debug", optional_argument, NULL, 'd' },
        { "version", optional_argument, NULL, 'v' }, { 0, 0, 0, 0 }
    };
    UlCommand ulCommand = NO_UL_COMMAND;
    bool pager = false, follow = false;

    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
        case 'f':
            follow = true;
            break;
        case 'p':
            pager = true;
            break;
        case 'd':
            DEBUG = true;
            break;
        case 'h':
            showUsage();
            rv = 0;
            goto out;
        case 'v':
            showVersion();
            rv = 0;
            goto out;
        default:
            showUsage();
            rv = 1;
            goto out;
        }
    }

    assertMacros();
    if ((commandName = argv[optind])) {
        ulCommand = getUlCommand(commandName);
        if (ulCommand == NO_UL_COMMAND) {
            showUsage();
            rv = 1;
            goto out;
        }
    }
    if (ulCommand == NO_UL_COMMAND)
        ulCommand = SHOW_LOG;
    userId = getuid();
    if (userId != 0) {
        if (!getSkipCheckAdmin(ulCommand)) {
            rv = checkAdministrator(argv);
            goto out;
        }
    }
    switch (ulCommand) {
    case SHOW_LOG:
        if (argc > 5 || (argc > 3 && !DEBUG && !pager && !follow)) {
            showUsage();
            rv = 1;
            goto out;
        }
        rv = showLog(pager, follow);
        break;
    case LIST_BOOTS:
        if (argc > 3 || (argc > 2 && !DEBUG)) {
            showUsage();
            rv = 1;
            goto out;
        }
        rv = showBootsList();
        break;
    case SHOW_BOOT:
        if (argc < 3 || argc > 6 || (argc > 3 && !DEBUG && !pager && !follow)) {
            showUsage();
            rv = 1;
            goto out;
        }
        arg = argv[argc - 1];
        rv = showBoot(pager, follow, arg);
        break;
    case INDEX_REPAIR:
        if (argc < 2 || argc > 3 || (argc > 2 && !DEBUG)) {
            showUsage();
            rv = 1;
            goto out;
        }
        rv = indexRepair();
        if (rv == 0)
            logSuccess(CONSOLE, "Index file repaired successfully.\n");
        break;
    case VACUUM:
        if (argc < 3 || argc > 4 || (argc > 3 && !DEBUG)) {
            showUsage();
            rv = 1;
            goto out;
        }
        arg = argv[argc - 1];
        rv = vacuum(arg);
        break;
    case SHOW_SIZE:
        if (argc < 2 || argc > 3 || (argc == 3 && !DEBUG)) {
            showUsage();
            rv = 1;
            goto out;
        }
        printLogSizeInfo(-1, -1, getFileSize(UNITLOGD_LOG_PATH));
        break;
    case SHOW_CURRENT:
        if (argc < 2 || argc > 5 || (argc > 2 && !DEBUG && !pager && !follow)) {
            showUsage();
            rv = 1;
            goto out;
        }
        rv = showCurrentBoot(pager, follow);
        break;
    default:
        break;
    }

out:

    return rv;
}
