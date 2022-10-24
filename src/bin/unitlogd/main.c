/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd/unitlogd_impl.h"

const char *DEV_LOG_NAME = "/dev/log";
const char *DEV_KMSG_NAME = "/proc/kmsg";
const char *DMESG_LOG_PATH = "/var/log/dmesg.log";
int SELF_PIPE[2];
int UNITLOGD_PID = 0;
bool UNITLOGD_DEBUG = false;
FILE *UNITLOGD_INDEX_FILE = NULL;
FILE *UNITLOGD_LOG_FILE = NULL;
FILE *UNITLOGD_BOOT_LOG_FILE = NULL;
char *BOOT_ID_STR = NULL;

static void __attribute__((noreturn))
usage(bool fail)
{
    fprintf(stdout,
            "Usage: unitlogd [OPTIONS] \n\n"

            WHITE_UNDERLINE_COLOR"OPTIONS\n"DEFAULT_COLOR
            "-l, --log          Log the system messages\n"
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
    if (strcmp(devName, DEV_LOG_NAME) == 0)
        socketThread->sockType = UNIX;
    else
        socketThread->sockType = FORWARDER;
    arrayAdd(*socketThreads, socketThread);
}

static int
createResources()
{
    int rv = 0;
    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "UNITLOGD_PATH", UNITLOGD_PATH);
    addEnvVar(&envVars, "UNITLOGD_LOG_PATH", UNITLOGD_LOG_PATH);
    addEnvVar(&envVars, "UNITLOGD_INDEX_PATH", UNITLOGD_INDEX_PATH);
    addEnvVar(&envVars, "UNITLOGD_BOOT_LOG_PATH", UNITLOGD_BOOT_LOG_PATH);
    addEnvVar(&envVars, "UNITLOGD_LOCK_PATH", UNITLOGD_LOCK_PATH);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);
    /* Exec script */
    rv = execUlScript(&envVars, "create");
    arrayRelease(&envVars);
    return rv;
}

int main(int argc, char **argv) {

    int c, rv, input;
    const char *shortopts = "lkhd";
    bool log, kernel;
    Array *socketThreads = arrayNew(socketThreadRelease);
    const struct option longopts[] = {
        { "log", no_argument, NULL, 'l' },
        { "kernel", no_argument, NULL, 'k' },
        { "debug", optional_argument, NULL, 'd' },
        { "help", no_argument, NULL, 'h' },
        { 0, 0, 0, 0 }
    };
    c = rv = input = 0;
    log = kernel = false;

    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
            case 'l':
                log = true;
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
    else if (!log && !kernel) {
        rv = 1;
        logErrorStr(CONSOLE, "Nothing to do!\n");
        logInfo(CONSOLE, "Please, provide at least one of the following options: "
                         "\nlog\nkernel\n");
        goto out;
    }

    /* Assert the macros */
    assertMacros();

    /* Set pid */
    UNITLOGD_PID = getpid();

    if (log) {
        /* Create dir and files */
        if ((rv = createResources()) != 0)
            goto out;

        /* Open the boot log */
        if (unitlogdOpenBootLog("w")) {
            rv = 1;
            goto out;
        }
        assert(UNITLOGD_BOOT_LOG_FILE);

        if (UNITLOGD_DEBUG) {
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd starting as pid %d\n", UNITLOGD_PID);
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd path = %s\n", UNITLOGD_PATH);
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd data path = %s\n", UNITLOGD_DATA_PATH);
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd boot log path = %s\n", UNITLOGD_BOOT_LOG_PATH);
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd log path = %s\n", UNITLOGD_LOG_PATH);
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd index path = %s\n", UNITLOGD_INDEX_PATH);
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd lock path = %s\n", UNITLOGD_LOCK_PATH);
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Debug = %s\n", UNITLOGD_DEBUG ? "True" : "False");
        }
    }
    /* Set sigaction */
    if ((rv = setUnitlogdSigAction()) != 0)
        goto out;

    /* Self pipe */
    if ((rv = pipe(SELF_PIPE)) != 0) {
        logError(CONSOLE | SYSTEM, "src/bin/unitlogd/main.c", "main", errno, strerror(errno),
                 "Pipe func returned %d", rv);
        goto out;
    }

    /* Set the number of threads */
    if (log)
        addSocketThread(&socketThreads, DEV_LOG_NAME);
    if (kernel)
        addSocketThread(&socketThreads, DEV_KMSG_NAME);

    /* The index management is useful only if we have to log the messages (log option).
     * No need that for the forwarders.
    */
    if (log && (rv = unitlogdInit()) != 0)
        goto out;

    /* Start sockets */
    if ((rv = startSockets(socketThreads)) != 0)
        goto out;

    /* Close boot log */
    if (log) {
        unitlogdCloseBootLog();
        assert(!UNITLOGD_BOOT_LOG_FILE);
    }

    while (true) {
        /* Unitlogd is blocked here waiting for a signal */
        if ((rv = read(SELF_PIPE[0], &input, sizeof(int))) == -1) {
            logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/bin/unitlogd/main.c", "main", errno,
                     strerror(errno), "Unable to read from the self pipe. Rv = %d.", rv);
            goto out;
        }
        else
            rv = 0;

        if (input == THREAD_EXIT)
            break;
    }

    /*************** SHUTDOWN ********************/

    /* Open the boot log */
    if (log) {
        if (unitlogdOpenBootLog("a")) {
            rv = 1;
            goto out;
        }
        assert(UNITLOGD_BOOT_LOG_FILE);
    }

    /* Stop sockets */
    rv = stopSockets(socketThreads);

    if (log && (rv = unitlogdShutdown()) != 0)
        goto out;

    out:
        /* Release resources */
        close(SELF_PIPE[0]);
        close(SELF_PIPE[1]);
        objectRelease(&BOOT_ID_STR);
        arrayRelease(&socketThreads);
        if (UNITLOGD_DEBUG)
            logInfo(CONSOLE | UNITLOGD_BOOT_LOG, "Unitlogd exited with rv = %d.\n", rv);
        if (log) {
            unitlogdCloseBootLog();
            assert(!UNITLOGD_BOOT_LOG_FILE);
        }
        return rv;
}
