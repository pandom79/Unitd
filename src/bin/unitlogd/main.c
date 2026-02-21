/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd/unitlogd_impl.h"

const char *DEV_LOG_NAME = "/dev/log";
const char *DEV_KMSG_NAME = "/proc/kmsg";
int SELF_PIPE[2];
int UNITLOGD_PID = 0;
bool DEBUG = false;
FILE *UNITLOGD_INDEX_FILE = NULL;
FILE *UNITLOGD_LOG_FILE = NULL;
FILE *UNITLOGD_KMSG_FILE = NULL;
char *BOOT_ID_STR = NULL;
bool UNITLOGD_EXIT = false;

static void usage()
{
    // clang-format off
    fprintf(stdout,
            "Usage: unitlogd [OPTIONS] \n\n"
            WHITE_UNDERLINE_COLOR"OPTIONS\n"DEFAULT_COLOR
            "-l, --log          Log the system messages\n"
            "-k, --kernel       Enable the kernel messages forward\n"
            "-d, --debug        Enable the debug\n"
            "-v, --version      Show the version\n"
            "-h, --help         Show usage\n\n"
    );
    // clang-format on
}

static void addSocketThread(Array **socketThreads, const char *devName)
{
    assert(*socketThreads);
    assert(devName);
    SocketThread *socketThread = socketThreadNew();
    socketThread->devName = stringNew(devName);
    if (stringEquals(devName, DEV_LOG_NAME))
        socketThread->sockType = UNIX;
    else
        socketThread->sockType = FORWARDER;
    arrayAdd(*socketThreads, socketThread);
}

static int createResources()
{
    int rv = 0;

    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "UNITLOGD_PATH", UNITLOGD_PATH);
    addEnvVar(&envVars, "UNITLOGD_LOG_PATH", UNITLOGD_LOG_PATH);
    addEnvVar(&envVars, "UNITLOGD_INDEX_PATH", UNITLOGD_INDEX_PATH);
    addEnvVar(&envVars, "UNITLOGD_LOCK_PATH", UNITLOGD_LOCK_PATH);
    arrayAdd(envVars, NULL);
    rv = execUlScript(&envVars, "create");
    arrayRelease(&envVars);

    return rv;
}

int main(int argc, char **argv)
{
    int c = 0, rv = 0, input = 0;
    const char *shortopts = "lkhdv";
    bool log = false, kernel = false;
    Array *socketThreads = arrayNew(socketThreadRelease);
    const struct option longopts[] = {
        { "log", no_argument, NULL, 'l' },         { "kernel", no_argument, NULL, 'k' },
        { "debug", optional_argument, NULL, 'd' }, { "version", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },        { 0, 0, 0, 0 }
    };

    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
        case 'l':
            log = true;
            break;
        case 'k':
            kernel = true;
            break;
        case 'd':
            DEBUG = true;
            break;
        case 'v':
            showVersion();
            goto out;
        case 'h':
            usage();
            goto out;
        default:
            usage();
            rv = 1;
            goto out;
        }
    }

    assertMacros();

    if (getuid() != 0) {
        rv = 1;
        logErrorStr(CONSOLE, "Please, run this program as administrator.\n");
        goto out;
    } else if (!log && !kernel) {
        rv = 1;
        logErrorStr(CONSOLE, "Nothing to do!\n");
        logInfo(CONSOLE, "Please, provide at least one of the following options: "
                         "\nlog\nkernel\n");
        goto out;
    }
    UNITLOGD_PID = getpid();
    if (log) {
        if ((rv = createResources()) != 0)
            goto out;
        if (DEBUG) {
            logInfo(CONSOLE, "Unitlogd starting as pid %d\n", UNITLOGD_PID);
            logInfo(CONSOLE, "Unitlogd path = %s\n", UNITLOGD_PATH);
            logInfo(CONSOLE, "Unitlogd data path = %s\n", UNITLOGD_DATA_PATH);
            logInfo(CONSOLE, "Unitlogd log path = %s\n", UNITLOGD_LOG_PATH);
            logInfo(CONSOLE, "Unitlogd index path = %s\n", UNITLOGD_INDEX_PATH);
            logInfo(CONSOLE, "Unitlogd lock path = %s\n", UNITLOGD_LOCK_PATH);
            logInfo(CONSOLE, "Unitlogd kmesg path = %s\n", UNITLOGD_KMSG_PATH);
            logInfo(CONSOLE, "Debug = %s\n", DEBUG ? "True" : "False");
        }
    }
    if ((rv = setUnitlogdSigAction()) != 0)
        goto out;
    if ((rv = pipe(SELF_PIPE)) != 0) {
        logError(CONSOLE | SYSTEM, "src/bin/unitlogd/main.c", "main", errno, strerror(errno),
                 "Pipe func returned %d", rv);
        goto out;
    }
    if (log)
        addSocketThread(&socketThreads, DEV_LOG_NAME);
    if (kernel)
        addSocketThread(&socketThreads, DEV_KMSG_NAME);
    /**
     * The index management is useful only if we have to log the messages (log option).
     * No need that for the forwarders.
     *
    */
    if (log && (rv = unitlogdInit()) != 0)
        goto out;
    if ((rv = startSockets(socketThreads)) != 0)
        goto out;
    while (true) {
        if ((rv = uRead(SELF_PIPE[0], &input, sizeof(int))) == -1) {
            logError(CONSOLE, "src/bin/unitlogd/main.c", "main", errno, strerror(errno),
                     "Unable to read from the self pipe. Rv = %d.", rv);
            goto out;
        } else
            rv = 0;
        if (input == THREAD_EXIT)
            break;
    }
    /*************** SHUTDOWN ********************/
    rv = stopSockets(socketThreads);
    if (log && (rv = unitlogdShutdown()) != 0)
        goto out;

out:
    close(SELF_PIPE[0]);
    close(SELF_PIPE[1]);
    objectRelease(&BOOT_ID_STR);
    arrayRelease(&socketThreads);
    if (DEBUG)
        logInfo(CONSOLE, "Unitlogd exited with rv = %d.\n", rv);

    return rv;
}
