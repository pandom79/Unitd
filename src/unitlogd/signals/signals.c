/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../core/unitd_impl.h"

int SELF_PIPE[2];
bool UNITLOGD_EXIT;

void exitSignal(int signo, siginfo_t *info UNUSED, void *context UNUSED)
{
    int rv = 0, output = THREAD_EXIT;

    if (DEBUG)
        logInfo(CONSOLE, "Unitlogd received %d signal, exiting ....\n", signo);
    UNITLOGD_EXIT = true;
    if ((rv = uWrite(SELF_PIPE[1], &output, sizeof(int))) == -1) {
        logError(CONSOLE, "src/unitlogd/signals/signals.c", "exitSignal", errno, strerror(errno),
                 "Unable to write into self pipe. Rv = %d.", rv);
    }
}

int setUnitlogdSigAction()
{
    int rv = 0;
    struct sigaction act = { 0 };

    act.sa_flags = SA_SIGINFO | SA_RESTART;
    act.sa_sigaction = exitSignal;
    if (sigaction(SIGTERM, &act, NULL) == -1 || sigaction(SIGQUIT, &act, NULL) == -1 ||
        sigaction(SIGINT, &act, NULL) == -1) {
        rv = -1;
        logError(CONSOLE | UNITLOGD_BOOT_LOG, "src/unitlogd/signals/signals.c", "setSigAction",
                 errno, strerror(errno), "Sigaction returned %d exit code", rv);
    }

    return rv;
}
