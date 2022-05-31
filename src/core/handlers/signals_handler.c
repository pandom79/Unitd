/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

bool UNITD_DEBUG;
UnitdData *UNITD_DATA;

int
signalsHandler(int signo UNUSED, siginfo_t *info, void *context UNUSED)
{
    int rv, status, infoCode, *finalStatus, *exitCode, output;
    const char *unitName = NULL;
    Unit *unit = NULL;
    pid_t infoPid = 0;
    ProcessData *pData = NULL;
    PStateData *pStateData = NULL;
    Pipe *unitPipe = NULL;

    rv = 0;

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

        /* Get the unit */
        unit = getUnitByPid(UNITD_DATA->units, info->si_pid);
        if (unit && unit->type == DAEMON && !unit->isStopping) {
            unitName = unit->name;
            /* Get siginfo data */
            infoCode = info->si_code;
            infoPid = info->si_pid;
            /* Get process Data */
            pData = unit->processData;
            pStateData = pData->pStateData;
            finalStatus = pData->finalStatus;
            exitCode = pData->exitCode;
            unitPipe = unit->pipe;

            if (infoCode == CLD_EXITED || infoCode == CLD_KILLED) {
//                waitForPid(infoPid, &status, false);
//                waitForPid(-1, &status, true);
                waitpid(infoPid, &status, 0);
                /* Reap all pending child processes */
                reapPendingChild();
            }

            switch (infoCode) {
                case CLD_EXITED:
                    *exitCode = info->si_status;
                    *finalStatus = FINAL_STATUS_FAILURE;
                    *pStateData = PSTATE_DATA_ITEMS[EXITED];
                    timeSetCurrent(&pData->timeStop);
                    stringSetTimeStamp(&pData->dateTimeStopStr, pData->timeStop, true);
                    stringSetDiffTime(&pData->duration, pData->timeStop, pData->timeStart);
                    if (UNITD_DEBUG) {
                        syslog(LOG_DAEMON | LOG_DEBUG,
                               "The process %s with pid %d is exited with the following values: "
                               "Exit code = %d, status = %s, finalStatus = %d, type = %s, "
                               "dateTimeStart = %s, dateTimeStop = %s, duration = %s\n",
                               unitName, infoPid, *exitCode, pStateData->desc, *finalStatus,
                               PTYPE_DATA_ITEMS[unit->type].desc, pData->dateTimeStartStr,
                               pData->dateTimeStopStr, pData->duration);
                    }
                    if (unitPipe) {
                        output = CLD_EXITED;
                        if (write(unitPipe->fds[1], &output, sizeof(int)) == -1) {
                            unitdLogError(LOG_UNITD_CONSOLE, "src/core/handlers/signal_handler.c", "signalsHandler",
                                          errno, strerror(errno), "Unable to write into pipe for the %s unit (exit case)",
                                          unitName);
                        }
                    }
                    break;
                case CLD_KILLED:
                    *exitCode = -1;
                    *finalStatus = FINAL_STATUS_FAILURE;
                    *pStateData = PSTATE_DATA_ITEMS[KILLED];
                    *pData->signalNum = info->si_status;
                    timeSetCurrent(&pData->timeStop);
                    stringSetTimeStamp(&pData->dateTimeStopStr, pData->timeStop, true);
                    stringSetDiffTime(&pData->duration, pData->timeStop, pData->timeStart);
                    if (UNITD_DEBUG) {
                        syslog(LOG_DAEMON | LOG_DEBUG,
                               "The process %s with pid %d is terminated with the following values: "
                               "Exit code = %d, signal = %d, status = %s, finalStatus = %d, type = %s"
                               "dateTimeStart = %s, dateTimeStop = %s, duration = %s\n",
                               unitName, infoPid, *exitCode, *pData->signalNum, pStateData->desc,
                               *finalStatus, PTYPE_DATA_ITEMS[unit->type].desc, pData->dateTimeStartStr,
                               pData->dateTimeStopStr, pData->duration);
                    }
                    if (unitPipe) {
                        output = CLD_KILLED;
                        if (write(unitPipe->fds[1], &output, sizeof(int)) == -1) {
                            unitdLogError(LOG_UNITD_CONSOLE, "src/core/handlers/signal_handler.c", "signalsHandler",
                                          errno, strerror(errno), "Unable to write into pipe for the %s unit (kill case)",
                                          unitName);
                        }
                    }
                    break;
                case CLD_STOPPED:
                    *exitCode = -1;
                    *finalStatus = FINAL_STATUS_FAILURE;
                    *pStateData = PSTATE_DATA_ITEMS[STOPPED];
                    *pData->signalNum = info->si_status;
                    timeSetCurrent(&pData->timeStop);
                    stringSetTimeStamp(&pData->dateTimeStopStr, pData->timeStop, true);
                    stringSetDiffTime(&pData->duration, pData->timeStop, pData->timeStart);
                    if (UNITD_DEBUG)
                        syslog(LOG_DAEMON | LOG_DEBUG,
                               "The process %s with pid %d is stopped with the following values: "
                               "Exit code = %d, signal = %d, status = %s, finalStatus = %d, type = %s"
                               "dateTimeStart = %s, dateTimeStop = %s, duration = %s\n",
                               unitName, infoPid, *exitCode, *pData->signalNum, pStateData->desc,
                               *finalStatus, PTYPE_DATA_ITEMS[unit->type].desc, pData->dateTimeStartStr,
                               pData->dateTimeStopStr, pData->duration);
                    break;
                case CLD_CONTINUED:
                    *exitCode = -1;
                    *finalStatus = FINAL_STATUS_SUCCESS;
                    *pStateData = PSTATE_DATA_ITEMS[RUNNING];
                    *pData->signalNum = info->si_status;
                    objectRelease(&pData->dateTimeStopStr);
                    objectRelease(&pData->duration);
                    timeRelease(&pData->timeStop);
                    if (UNITD_DEBUG)
                        syslog(LOG_DAEMON | LOG_DEBUG,
                               "The process %s with pid %d is continued with the following values: "
                               "Exit code = %d, signal = %d, status = %s, finalStatus = %d, type = %s"
                               "dateTimeStart = %s\n",
                               unitName, infoPid, *exitCode, *pData->signalNum, pStateData->desc,
                               *finalStatus, PTYPE_DATA_ITEMS[unit->type].desc, pData->dateTimeStartStr);
                    break;
            }
        }
    }

    out:
        return rv;
}
