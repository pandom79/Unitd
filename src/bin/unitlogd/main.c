/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../../core/unitd_impl.h"

int SELF_PIPE[2];
int UNITLOGD_PID = 0;
bool UNITLOGD_DEBUG = false;
FILE *UNITLOGD_INDEX_FILE = NULL;
FILE *UNITLOGD_LOG_FILE = NULL;
FILE *UNITLOGD_BOOT_LOG_FILE = NULL;

static void __attribute__((noreturn))
usage(bool fail)
{
    fprintf(stdout,
            "Usage: unitlogd [OPTIONS] \n\n"

            WHITE_UNDERLINE_COLOR"OPTIONS\n"DEFAULT_COLOR
            "-l, --log          Log the system messages\n"
            "-c, --console      Enable the console messages forward\n"
            "-k, --kernel       Enable the kernel messages forward\n"
            "-d, --debug        Enable the debug\n"
            "-h, --help         Show usage\n\n"
    );
    exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
addSocketThread(Array **socketThreads, const char *devName)
{
    assert(*socketThreads);
    assert(devName);
    SocketThread *socketThread = socketThreadNew();
    socketThread->devName = stringNew(devName);
    arrayAdd(*socketThreads, socketThread);
}

int main(int argc, char **argv) {

    int c, rv, input;
    const char *shortopts = "lckhd";
    bool log, console, kernel;
    Array *socketThreads = arrayNew(socketThreadRelease);
    const struct option longopts[] = {
        { "log", no_argument, NULL, 'l' },
        { "console", no_argument, NULL, 'c' },
        { "kernel", no_argument, NULL, 'k' },
        { "debug", optional_argument, NULL, 'd' },
        { "help", no_argument, NULL, 'h' },
        { 0, 0, 0, 0 }
    };
    c = rv = input = 0;
    log = console = kernel = false;

    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
            case 'l':
                log = true;
                break;
            case 'c':
                console = true;
                break;
            case 'k':
                kernel = true;
                break;
            case 'd':
                UNITLOGD_DEBUG = true;
                break;
            case 'h':
                usage(false);
                break;
            default:
                usage(true);
                break;
        }
    }

    /* Check user id and options */
    if (getuid() != 0) {
        rv = 1;
        logErrorStr(CONSOLE, "Please, run this program as administrator.\n");
        goto out;
    }
    else if (!log && !console && !kernel) {
        rv = 1;
        logErrorStr(CONSOLE, "Nothing to do!\n");
        logInfo(CONSOLE, "Please, provide at least one of the following options: "
                                        "\nlog\nconsole\nkernel\n");
        goto out;
    }

    /* Assert the macros */
    assertMacros();

    /* Set pid */
    UNITLOGD_PID = getpid();

    if (UNITLOGD_DEBUG) {
        logInfo(CONSOLE, "Unitlogd starting as pid %d\n", UNITLOGD_PID);
        logInfo(CONSOLE, "Unitlogd path = %s\n", UNITLOGD_PATH);
        logInfo(CONSOLE, "Unitlogd log path = %s\n", UNITLOGD_LOG_PATH);
        logInfo(CONSOLE, "Unitlogd index path = %s\n", UNITLOGD_INDEX_PATH);
        logInfo(CONSOLE, "Unitlogd boot log path = %s\n", UNITLOGD_BOOT_LOG_PATH);
        logInfo(CONSOLE, "Debug = %s\n", UNITLOGD_DEBUG ? "True" : "False");
    }

    /* We run "init" script only if we have to log the messages.
     * No need that for the forwarders.
    */
    if (log && (rv = unitlogdInit()) != 0)
        goto out;

    /* Set sigaction */
    if ((rv = setUnitlogdSigAction()) != 0)
        goto out;

    /* Self pipe */
    if ((rv = pipe(SELF_PIPE)) != 0) {
        logError(CONSOLE, "src/bin/unitlogd/main.c", "main", errno,
                      strerror(errno), "Pipe func has returned %d", rv);
        goto out;
    }

    /* Calculate the number of threads */
    if (log)
        addSocketThread(&socketThreads, "/dev/log");
    if (console)
        addSocketThread(&socketThreads, "/dev/console");
    if (kernel)
        addSocketThread(&socketThreads, "/dev/kmsg");

    /* Start sockets */
    if ((rv = startSockets(socketThreads)) != 0)
        goto out;

    while (true) {
        /* Unitlogd is blocked here waiting for a signal */
        if ((rv = read(SELF_PIPE[0], &input, sizeof(int))) == -1) {
            logError(CONSOLE, "src/bin/unitlogd/main.c", "main", errno,
                          strerror(errno), "Unable to read from the self pipe. Rv = %d.", rv);
            goto out;
        }
        else
            rv = 0;

        if (input == THREAD_EXIT)
            break;
    }

    /* Stop sockets */
    rv = stopSockets(socketThreads);

    out:
        close(SELF_PIPE[0]);
        close(SELF_PIPE[1]);
        arrayRelease(&socketThreads);

        if (UNITLOGD_DEBUG)
            logInfo(CONSOLE, "Unitlogd exited with rv = %d.\n", rv);

        return rv;
}
