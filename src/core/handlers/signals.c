/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

bool UNITD_DEBUG;
UnitdData *UNITD_DATA;

int signalsHandler(int signo, siginfo_t *info, void *context UNUSED)
{
    int rv = 0, status, infoCode, *finalStatus, *exitCode, output;
    const char *unitName = NULL;
    Unit *unit = NULL;
    pid_t infoPid = 0;
    ProcessData *pData = NULL;
    PStateData *pStateData = NULL;
    Pipe *unitPipe = NULL;

    if (SHUTDOWN_COMMAND == NO_COMMAND) {
        /* If unitd (PID=1) receives a SIGINT (ctrl+alt+del) or SIGTERM signal then
         * we start the reboot phase
        */
        if (signo == SIGINT || signo == SIGTERM) {
            /* Set default shutdown operation. */
            SHUTDOWN_COMMAND = REBOOT_COMMAND;
            /* If we already are listening then we can invoke unitdShutdown function
             * simulating the "unitctl" command without force
            */
            if (LISTEN_SOCK_REQUEST)
                rv = unitdShutdown(SHUTDOWN_COMMAND, false, false, true);
            goto out;
        }
        /* Get siginfo data */
        infoCode = info->si_code;
        infoPid = info->si_pid;
        /* Get the unit */
        unit = getUnitByPid(UNITD_DATA->units, infoPid);
        if (unit && !unit->isStopping) {
            unitName = unit->name;
            /* Get process Data */
            pData = unit->processData;
            pStateData = pData->pStateData;
            finalStatus = pData->finalStatus;
            exitCode = pData->exitCode;
            unitPipe = unit->pipe;
            if (infoCode == CLD_EXITED || infoCode == CLD_KILLED)
                uWaitPid(infoPid, &status, 0);
            if (unit->type == ONESHOT) {
                if (infoCode == CLD_EXITED)
                    *pData->exitCode = info->si_status;
            } else { //DAEMON
                switch (infoCode) {
                case CLD_EXITED:
                    *exitCode = info->si_status;
                    *finalStatus = FINAL_STATUS_FAILURE;
                    *pStateData = PSTATE_DATA_ITEMS[EXITED];
                    setStopAndDuration(&pData);
                    if (UNITD_DEBUG) {
                        logInfo(SYSTEM,
                                "The process %s with pid %d is exited with the following values: "
                                "Exit code = %d, status = %s, finalStatus = %d, type = %s, "
                                "dateTimeStart = %s, dateTimeStop = %s, duration = %s\n",
                                unitName, infoPid, *exitCode, pStateData->desc, *finalStatus,
                                PTYPE_DATA_ITEMS[unit->type].desc, pData->dateTimeStartStr,
                                pData->dateTimeStopStr, pData->duration);
                    }
                    if (unitPipe) {
                        output = CLD_EXITED;
                        if (uWrite(unitPipe->fds[1], &output, sizeof(int)) == -1) {
                            logError(SYSTEM, "src/core/handlers/signal_handler.c", "signalsHandler",
                                     errno, strerror(errno),
                                     "Unable to write into pipe for the %s unit (exit case)",
                                     unitName);
                        }
                    }
                    break;
                case CLD_KILLED:
                    *exitCode = -1;
                    *finalStatus = FINAL_STATUS_FAILURE;
                    *pStateData = PSTATE_DATA_ITEMS[KILLED];
                    *pData->signalNum = info->si_status;
                    setStopAndDuration(&pData);
                    if (UNITD_DEBUG) {
                        logInfo(
                            SYSTEM,
                            "The process %s with pid %d is terminated with the following values: "
                            "Exit code = %d, signal = %d, status = %s, finalStatus = %d, type = %s, "
                            "dateTimeStart = %s, dateTimeStop = %s, duration = %s\n",
                            unitName, infoPid, *exitCode, *pData->signalNum, pStateData->desc,
                            *finalStatus, PTYPE_DATA_ITEMS[unit->type].desc,
                            pData->dateTimeStartStr, pData->dateTimeStopStr, pData->duration);
                    }
                    if (unitPipe) {
                        output = CLD_KILLED;
                        if (uWrite(unitPipe->fds[1], &output, sizeof(int)) == -1) {
                            logError(SYSTEM, "src/core/handlers/signal_handler.c", "signalsHandler",
                                     errno, strerror(errno),
                                     "Unable to write into pipe for the %s unit (kill case)",
                                     unitName);
                        }
                    }
                    break;
                case CLD_STOPPED:
                    /* We don't want to allow SIGSTOP, SIGTSTP or something else.
                     * The only way to stop a process is "unitctl stop" command.
                     * We don't create a history for that because this is not a real "restart".
                     * Additionally, according restart policy, if 'restartNum' achieves 'restartMax' then
                     * this pid will remain stopped and we don't want that.
                     * At most, we set a sigcont signal. In this way the user will see it in the unit status
                     * and will understand that it has been stopped.
                     * That will can also be confirmed by system log.
                     */
                    if (kill(infoPid, SIGCONT) == -1) {
                        logError(SYSTEM, "src/core/handlers/signals.c", "signalsHandler", errno,
                                 strerror(errno), "Unable to send SIGCONT to %d pid!", infoPid);
                    } else
                        logWarning(
                            SYSTEM,
                            "'%s' with '%d' pid has received a SIGSTOP! Sending a SIGCONT signal to it ...",
                            unitName, infoPid);
                    break;
                case CLD_CONTINUED:
                    logWarning(SYSTEM, "'%s' with '%d' pid has received a SIGCONT signal!",
                               unitName, infoPid);
                    *pData->signalNum = SIGCONT;
                    break;
                }
            }
        } else if (!unit && infoCode == CLD_EXITED) {
            /* Try to get the unit by failure pid */
            if ((unit = getUnitByFailurePid(UNITD_DATA->units, infoPid)))
                *unit->failureExitCode = info->si_status;
        }
    }

out:
    return rv;
}
