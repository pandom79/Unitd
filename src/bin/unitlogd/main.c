/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../core/unitlogd_impl.h"

static void __attribute__((noreturn))
usage(bool fail)
{
    fprintf(stdout,
            "Usage: unitlogd [OPTION] \n\n"

            WHITE_UNDERLINE_COLOR"OPTIONS\n"DEFAULT_COLOR
            "-c, --console      Enable console messages forward\n"
            "-k, --kernel       Enable kernel messages forward\n"
            "-d, --debug        Enable the debug\n"
            "-h, --help         Show usage\n\n"
    );
    exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv) {

    int c, rv;
    const char *shortopts = "hdck";
    const struct option longopts[] = {
        { "console", no_argument, NULL, 'c' },
        { "kernel", no_argument, NULL, 'k' },
        { "help", no_argument, NULL, 'h' },
        { "debug", optional_argument, NULL, 'd' },
        { 0, 0, 0, 0 }
    };

    c = rv = 0;

    assertMacroPaths();

    return rv;
}
