/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../core/unitd_impl.h"

static void __attribute__((noreturn))
usage(bool fail)
{
    fprintf(stdout,
        "Usage: megazined [OPTION] \n\n"

        WHITE_UNDERLINE_COLOR"OPTIONS\n"DEFAULT_COLOR
        "-d, --debug        Enable the debug\n"
        "-h, --help         Show usage\n\n"
    );
    exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv) {

    int c, rv;
    const char *shortopts = "hd";
    const struct option longopts[] = {
        { "help", no_argument, NULL, 'h' },
        { "debug", optional_argument, NULL, 'd' },
        { 0, 0, 0, 0 }
    };

    c = rv = 0;

    assert(OS_NAME);
    assert(UNITS_PATH);
    assert(UNITS_USER_PATH);
    assert(UNITS_ENAB_PATH);
    assert(UNITD_LOG_PATH);
    assert(UNITD_DATA_PATH);
    assert(UNITD_CONF_PATH);


    return rv;
}
