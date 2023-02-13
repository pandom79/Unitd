/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

void*
startProcess(void *arg)
{
    UnitThreadData *unitThreadData = NULL;
    Unit *unit, *unitDep, *unitConflict;
    Array *units, *requires, *conflicts, *wantedBy, *pDataHistory;
    int lenDeps, lenConflicts, statusThread, *finalStatus, *finalStatusDep, *rvThread, rv;
    ProcessData *pData, *pDataDep, *pDataConflict;
    pthread_mutex_t *unitMutex, *unitDepMutex;
    char **cmdline = NULL;
    const char *command, *unitName, *unitNameDep, *unitNameconflict, *desc;
    PState *pStateConflict = NULL;

    lenDeps = lenConflicts = rv = statusThread = 0;
    rvThread = NULL;
    unit = unitDep = unitConflict = NULL;
    pData = pDataDep = pDataConflict = NULL;
    unitMutex = unitDepMutex = NULL;
    command = unitName = unitNameDep = unitNameconflict = NULL;
    units = requires = conflicts = wantedBy = pDataHistory = NULL;
    assert(arg);
    unitThreadData = (UnitThreadData *)arg;
    units = unitThreadData->units;
    /* Get the unit data */
    unit = unitThreadData->unit;
    desc = unit->desc;
    unitName = unit->name;
    pData = unit->processData;
    unitMutex = unit->mutex;
    finalStatus = pData->finalStatus;
    pDataHistory = unit->processDataHistory;

    /* Time start */
    timeRelease(&pData->timeStart);
    pData->timeStart = timeNew(NULL);
    /* Timestamp start */
    objectRelease(&pData->dateTimeStartStr);
    pData->dateTimeStartStr = stringGetTimeStamp(pData->timeStart, false, "%d-%m-%Y %H:%M:%S");

    /* Lock the mutex */
    if ((rv = pthread_mutex_lock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        logError(ALL, "src/core/processes/process.c", "startProcess",
                      rv, strerror(rv), "Unable to acquire the lock of the mutex for the %s unit",
                      unitName);
        goto out;
    }

    if (UNITD_DEBUG)
        logWarning(UNITD_BOOT_LOG, "\n[*] STARTING '%s' ... \n", unitName);

    /* If the errors already exist then exit */
    if (unit->errors->size > 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        if (UNITD_DEBUG)
            logErrorStr(UNITD_BOOT_LOG, "'%s' has some configuration errors. Exit!\n",
                            unitName);
        goto out;
    }

    /* Check the conflicts.
     * That never should happen.
     * Someone could enable the unit by hand rather than to run unitctl command.
     * For this reason, we check that anyway.
    */
    conflicts = unit->conflicts;
    lenConflicts = (conflicts ? conflicts->size : 0);
    for (int i = 0; i < lenConflicts; i++) {
        unitNameconflict = arrayGet(conflicts, i);
        unitConflict = getUnitByName(units, unitNameconflict);
        if (unitConflict) {
            pDataConflict = unitConflict->processData;
            pStateConflict = &pDataConflict->pStateData->pState;
            if (*pStateConflict != DEAD ||
               (*pStateConflict == DEAD && *pDataConflict->finalStatus != FINAL_STATUS_SUCCESS)) {
                arrayAdd(unit->errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_EXEC_ERROR].desc,
                                              unitName, unitNameconflict));
                *finalStatus = FINAL_STATUS_FAILURE;
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "'%s' has a conflict with '%s'. Exit!\n",
                                 unitName, unitNameconflict);
                goto out;
            }
        }
    }

    /* Get the dependencies */
    requires = unit->requires;
    lenDeps = (requires ? requires->size : 0);
    if (UNITD_DEBUG)
        logInfo(UNITD_BOOT_LOG, "'%s' has to wait for %d dependencies!\n", unitName, lenDeps);
    for (int i = 0; i < lenDeps; i++) {
        /* Get the dependency as string */
        unitNameDep = arrayGet(requires, i);
        /* Get the dependency as Unit type. */
        unitDep = getUnitByName(units, unitNameDep);
        if (!unitDep || (unitDep && unitDep->errors->size > 0)) {
            /* Set the unsatisfied dependency error in the current unit */
            arrayAdd(unit->errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc,
                                            unitNameDep, unitName));
            *finalStatus = FINAL_STATUS_FAILURE;
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "The dependency '%s' for '%s' doesn't exist "
                                             "or has some errors. Exit!\n",
                             unitNameDep, unitName);
            goto out;
        }
        else {

            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "The dependency name for '%s' = '%s' have no errors and is enabled. Go on!\n",
                             unitName, unitNameDep);
            /* Get the dependency's Process Data */
            pDataDep = unitDep->processData;
            finalStatusDep = pDataDep->finalStatus;

            /* If the dependency is not started yet */
            if (*finalStatusDep == FINAL_STATUS_READY) {
                /* Lock the dependency mutex */
                unitDepMutex = unitDep->mutex;
                if ((rv = pthread_mutex_lock(unitDepMutex)) != 0) {
                    *finalStatusDep = FINAL_STATUS_FAILURE;
                    logError(ALL, "src/core/processes/process.c", "startProcess",
                                  rv, strerror(rv), "Unable to lock the mutex for the %s dependency",
                                  unitNameDep);
                }
                else {
                    while (*finalStatusDep == FINAL_STATUS_READY) {
                        if (UNITD_DEBUG)
                            logInfo(UNITD_BOOT_LOG, "The dependency '%s' for '%s' is not started yet. "
                                                         "Waiting for the broadcast signal...!\n",
                                         unitNameDep, unitName);
                        /* Waiting for the broadcast signal ... */
                        if ((rv = pthread_cond_wait(unitDep->cv, unitDepMutex)) != 0) {
                            *finalStatusDep = FINAL_STATUS_FAILURE;
                            logError(ALL, "src/core/processes/process.c", "startProcess",
                                          rv, strerror(rv), "Unable to wait for condition variable for the %s dependency",
                                          unitNameDep);
                            break;
                        }
                        if (UNITD_DEBUG)
                            logInfo(UNITD_BOOT_LOG,
                                         "The dependency '%s' for '%s' sent the broadcast signal! Final status = %d\n",
                                         unitNameDep, unitName, *finalStatusDep);
                    }
                    /* Unlock the dependency mutex */
                    if ((rv = pthread_mutex_unlock(unitDepMutex)) != 0) {
                        *finalStatusDep = FINAL_STATUS_FAILURE;
                        logError(ALL, "src/core/processes/process.c", "startProcess",
                                      rv, strerror(rv), "Unable to unlock the mutex for the %s dependency",
                                      unitNameDep);
                    }
                }//unitDep mutex locked
            }
            else
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG,
                                 "The dependency '%s' for '%s' is already started! Final status = %d\n",
                                 unitNameDep, unitName, *finalStatusDep);

            if (*finalStatusDep != FINAL_STATUS_SUCCESS) {
                /* Set the unsatisfied dependency error in the current unit */
                arrayAdd(unit->errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc,
                                                unitNameDep, unitName));
                /* If the dependency is failed then the unit state is dead */
                *finalStatus = FINAL_STATUS_FAILURE;
                *pData->pStateData = PSTATE_DATA_ITEMS[DEAD];

                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "The dependency '%s' for '%s' went in ABEND!\n",
                                 unitNameDep, unitName);
                goto out;
            }
        }//dependency exist

    }// for dependencies

    switch (unit->type) {
        case DAEMON:
        case ONESHOT:
            /* If there are no configuration and dependency errors, we execute the command */
            /* Cmdline split */
            command = unit->runCmd;
            cmdline = cmdlineSplit(command);
            assert(cmdline);
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Executing '%s' command for '%s' ...!\n",
                        command, unitName);

            /* Execute the command */
            statusThread = execProcess(cmdline[0], cmdline, &unit);
            if (statusThread != EXIT_SUCCESS && statusThread != -1) {
                /* For the initialization and finalization units,
                 * we show the error and the emergency shell in the console
                 */
                wantedBy = unit->wantedBy;
                if (arrayContainsStr(wantedBy, STATE_DATA_ITEMS[INIT].desc) ||
                    arrayContainsStr(wantedBy, STATE_DATA_ITEMS[FINAL].desc)) {
                    logError(ALL, "src/core/processes/process.c", "startProcess", statusThread,
                                  strerror(statusThread), "'%s' command for '%s' returned %d exit code\n",
                                  command, unitName, statusThread);
                }
            }
            /* Release cmdline */
            cmdlineRelease(cmdline);
            break;
        case TIMER:
            statusThread = startTimerUnit(unit);
            break;
        case UPATH:
            statusThread = startNotifier(unit);
            break;
        default:
            break;
    }

    out:
    /* Set the final status of the unit */
    switch (unit->type) {
        case DAEMON:
            if (*pData->pid != -1 && *pData->exitCode == -1 &&
                pData->pStateData->pState == RUNNING)
                *finalStatus = FINAL_STATUS_SUCCESS;
            else
                *finalStatus = FINAL_STATUS_FAILURE;
            break;
        case ONESHOT:
            if (*pData->pid != -1 && *pData->exitCode == EXIT_SUCCESS &&
                pData->pStateData->pState == EXITED)
                *finalStatus = FINAL_STATUS_SUCCESS;
            else
                *finalStatus = FINAL_STATUS_FAILURE;
            break;
        case TIMER:
            if (statusThread == 0 && pData->pStateData->pState == RUNNING)
                *finalStatus = FINAL_STATUS_SUCCESS;
            else {
                pipeRelease(&unit->pipe);
                *finalStatus = FINAL_STATUS_FAILURE;
            }
            break;
        case UPATH:
            if (statusThread == 0 && pData->pStateData->pState == RUNNING)
                *finalStatus = FINAL_STATUS_SUCCESS;
            else
                *finalStatus = FINAL_STATUS_FAILURE;
            break;
        default:
            break;
    }

    /* Broadcast signal and unlock */
    if ((rv = pthread_cond_broadcast(unit->cv)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        logError(ALL, "src/core/processes/process.c", "startProcess",
                      rv, strerror(rv), "Unable to send the broadcast signal for the %s unit", unitName);
        kill(UNITD_PID, SIGTERM);
    }
    if (UNITD_DEBUG)
        logInfo(UNITD_BOOT_LOG, "'%s' sent the broadcast signal!\n", unitName);

    if ((rv = pthread_mutex_unlock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        logError(ALL, "src/core/processes/process.c", "startProcess",
                      rv, strerror(rv), "Unable to unlock the mutex for the %s unit", unitName);
        kill(UNITD_PID, SIGTERM);
    }
    /* show the result */
    if (unit->showResult) {
        if (*finalStatus == FINAL_STATUS_SUCCESS) {
            logInfo(CONSOLE | UNITD_BOOT_LOG, "[   %sOK%s   ] %s%s%s\n", GREEN_COLOR, DEFAULT_COLOR,
                         WHITE_COLOR, (desc ? desc : ""), DEFAULT_COLOR);
        }
        else
            logInfo(CONSOLE | UNITD_BOOT_LOG, "[ %sFAILED%s ] %s%s%s\n", RED_COLOR, DEFAULT_COLOR,
                         WHITE_COLOR, (desc ? desc : ""), DEFAULT_COLOR);
    }

    /* Return value. It will be freed by StartProcesses function */
    rvThread = calloc(1, sizeof(int));
    assert(rvThread);
    *rvThread = *finalStatus;
    return rvThread;
}

int
startProcesses(Array **units, Unit *singleUnit)
{
    int rv, result, numThreads, *rvThread;
    Unit *unit = NULL;
    const char *unitName = NULL;

    rv = result = numThreads = 0;

    if (!singleUnit)
        numThreads = (*units ? (*units)->size : 0);
    else
        numThreads = 1;

    if (numThreads > 0) {
        UnitThreadData unitsThreadsData[numThreads];
        if (UNITD_DEBUG)
            logWarning(UNITD_BOOT_LOG, "\n[*] CREATING %d THREADS (STARTING)\n", numThreads);
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            /* Get the unit */
            if (!singleUnit)
                unit = arrayGet(*units, i);
            else {
                unit = singleUnit;
                unit->showResult = false;
            }
            assert(unit);
            unitName = unit->name;
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Creating the '%s' thread\n", unitName);

            /* Set the unit thread data */
            unitThreadData->units = *units;
            unitThreadData->unit = unit;

            if ((rv = pthread_create(&unitThreadData->thread, NULL, startProcess, unitThreadData)) != 0) {
                logError(UNITD_BOOT_LOG, "src/core/processes/process.c", "startProcesses", rv,
                              strerror(rv), "Unable to create the thread for '%s'",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread created succesfully for '%s'\n", unitName);
            }
        }
        /* Waiting for all threads to terminate */
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            unit = unitThreadData->unit;
            unitName = unit->name;
            if ((rv = pthread_join(unitThreadData->thread, (void **)&rvThread)) != 0) {
                logError(UNITD_BOOT_LOG, "src/core/processes/process.c", "startProcesses", rv,
                              strerror(rv), "Unable to join the thread for '%s'",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread joined successfully for '%s'! Return code = %d\n",
                                 unitName, *rvThread);
            }
            if (*rvThread == 1)
                result = 1;
            objectRelease(&rvThread);
        }
    }

    return result;
}

Array*
getRunningUnits(Array **units)
{
    Array *runningUnits = arrayNew(NULL);
    int lenUnits = 0;
    Unit *unit = NULL;
    ProcessData *pData = NULL;
    PType unitType = NO_PROCESS_TYPE;

    lenUnits = (*units ? (*units)->size : 0);
    for (int i = 0; i < lenUnits; i++) {
        unit = arrayGet(*units, i);
        pData = unit->processData;
        unitType = unit->type;
        /* For the daemon and oneshot units we don't consider the restarting state because
         * closePipes() called before assure us the processes will have a state different by Restarting.
         * That's not true for the timers because closePipes() don't cosider them.
         * The timer which have a pipe will be stopped by stopProcess func which calls
         * closepipes() in single mode, therefore, its state can be Restarting as well.
        */
        if ((unitType == DAEMON && pData->pStateData->pState == RUNNING) ||
           ((unitType == TIMER || unitType == UPATH) &&
            (pData->pStateData->pState == RUNNING || pData->pStateData->pState == RESTARTING)))
            arrayAdd(runningUnits, unit);
    }

    return runningUnits;
}

void*
stopProcess(void *arg)
{
    UnitThreadData *unitThreadData = NULL;
    Unit *unit = NULL;
    const char *unitName = NULL;
    char **cmdline = NULL, *command = NULL;
    int statusThread, *finalStatus, rv;
    int *rvThread = NULL;
    ProcessData *pData = NULL;
    pthread_mutex_t *unitMutex = NULL;

    rv = 0;

    assert(arg);
    unitThreadData = (UnitThreadData *)arg;
    /* Get the unit data */
    unit = unitThreadData->unit;
    assert(unit);
    unitMutex = unit->mutex;
    pData = unit->processData;
    finalStatus = pData->finalStatus;
    unitName = unit->name;

    /* Lock the mutex */
    if ((rv = pthread_mutex_lock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        logError(CONSOLE, "src/core/processes/process.c", "stopProcess",
                      rv, strerror(rv), "Unable to acquire the lock of the mutex for the %s unit",
                      unitName);
        goto out;
    }

    switch (unit->type) {
        case DAEMON:
            command = unit->stopCmd;
            if (command) {
                /* Replace PID_CMD_VAR */
                if (stringContainsStr(command, PID_CMD_VAR)) {
                    char pidStr[30] = {0};
                    sprintf(pidStr, "%d", *pData->pid);
                    assert(strlen(pidStr) > 0);
                    stringReplaceStr(&command, PID_CMD_VAR, pidStr);
                }
                /* Split cmdline */
                cmdline = cmdlineSplit(command);
                assert(cmdline);
                /* Execute the command */
                statusThread = stopDaemon(cmdline[0], cmdline, &unit);
                cmdlineRelease(cmdline);
            }
            else
                statusThread = stopDaemon(NULL, NULL, &unit);
            break;
        case TIMER:
            /* When we stop the timer, we have to unlock the mutex because
             * the timer could require its lock in restart case.
            */
            if ((rv = pthread_mutex_unlock(unitMutex)) != 0) {
                *finalStatus = FINAL_STATUS_FAILURE;
                logError(CONSOLE, "src/core/processes/process.c", "stopProcess",
                              rv, strerror(rv), "Unable to unlock of the mutex for the %s unit (timer case)", unitName);
                goto out;
            }

            statusThread = closePipes(NULL, unit);

            if ((rv = pthread_mutex_lock(unitMutex)) != 0) {
                *finalStatus = FINAL_STATUS_FAILURE;
                logError(CONSOLE, "src/core/processes/process.c", "stopProcess",
                              rv, strerror(rv), "Unable to acquire the lock of the mutex for the %s unit (timer case)",
                              unitName);
                goto out;
            }
            *pData->pStateData = PSTATE_DATA_ITEMS[DEAD];
            setStopAndDuration(&pData);
            break;
        case UPATH:
            statusThread = stopNotifier(unit);
            *pData->pStateData = PSTATE_DATA_ITEMS[DEAD];
            setStopAndDuration(&pData);
            break;
        default:
            break;
    }

    /* Evaluating the final status */
    switch (unit->type) {
        case DAEMON:
            if (statusThread == -1 && *pData->exitCode == -1 && *pData->pid != -1 &&
                pData->pStateData->pState == DEAD)
                *finalStatus = FINAL_STATUS_SUCCESS;
            else
                *finalStatus = FINAL_STATUS_FAILURE;
            break;
        case TIMER:
        case UPATH:
            if (statusThread == 0 && pData->pStateData->pState == DEAD)
                *finalStatus = FINAL_STATUS_SUCCESS;
            else
                *finalStatus = FINAL_STATUS_FAILURE;
        default:
            break;
    }

    if ((rv = pthread_mutex_unlock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        logError(CONSOLE, "src/core/processes/process.c", "stopProcess",
                      rv, strerror(rv), "Unable to unlock of the mutex for the %s unit", unitName);
        goto out;
    }

    out:
        if (unit->showResult) {
            if (*finalStatus == FINAL_STATUS_SUCCESS) {
                logInfo(CONSOLE | UNITD_BOOT_LOG, "[   %sOK%s   ] %s%s%s\n", GREEN_COLOR, DEFAULT_COLOR,
                             WHITE_COLOR, unit->desc, DEFAULT_COLOR);
            }
            else
                logInfo(CONSOLE | UNITD_BOOT_LOG, "[ %sFAILED%s ] %s%s%s\n", RED_COLOR, DEFAULT_COLOR,
                             WHITE_COLOR, unit->desc, DEFAULT_COLOR);
        }


        /* Return value. It will be freed by StopProcesses function */
        rvThread = calloc(1, sizeof(int));
        assert(rvThread);
        *rvThread = *finalStatus;
        return rvThread;
}

int
stopProcesses(Array **units, Unit *unitArg)
{
    int rv, numThreads, *rvThread;
    Unit *unit = NULL;
    const char *unitName = NULL;
    Array *runningUnits = NULL;

    rv = numThreads = 0;

    /* We only get the running units */
    if (!unitArg)
        runningUnits = getRunningUnits(units);
    else {
        runningUnits = arrayNew(NULL);
        arrayAdd(runningUnits, unitArg);
    }
    numThreads = (runningUnits ? runningUnits->size : 0);
    if (numThreads > 0) {
        UnitThreadData unitsThreadsData[numThreads];
        if (UNITD_DEBUG)
            logWarning(UNITD_BOOT_LOG, "\n[*] CREATING %d THREADS (STOPPING) \n", numThreads);
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            /* Get the unit */
            unit = arrayGet(runningUnits, i);
            assert(unit);
            if (unitArg)
                unit->showResult = false;
            unitName = unit->name;
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Creating '%s' thread\n", unitName);

            /* Set the unit */
            unitThreadData->unit = unit;
            if ((rv = pthread_create(&unitThreadData->thread, NULL, stopProcess, unitThreadData)) != 0) {
                logError(UNITD_BOOT_LOG, "src/core/processes/process.c", "stopProcesses", rv,
                              strerror(rv), "Unable to create the thread for '%s'",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread created succesfully for '%s'\n", unitName);
            }
        }
        /* Waiting for all threads to terminate */
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            unit = unitThreadData->unit;
            unitName = unit->name;
            if ((rv = pthread_join(unitThreadData->thread, (void **)&rvThread)) != 0) {
                logError(UNITD_BOOT_LOG, "src/core/processes/process.c", "stopProcesses", rv,
                              strerror(rv), "Unable to join the thread for '%s'",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread joined successfully for '%s'! Return code = %d\n",
                                 unitName, *rvThread);
            }
            if (*rvThread == 1)
                rv = 1;
            objectRelease(&rvThread);
        }
    }

    /* We only release runningUnits array and runningUnits->arr member because the inside elements (units)
     * will be released by unitdEnd function
    */
    arrayRelease(&runningUnits);
    return rv;
}

void*
listenPipe(void *arg)
{
    Unit *unit = NULL;
    int rv, input, restartMax, *restartNum, rvMutex;
    bool restart = false;
    ProcessData **pData = NULL;
    Array *pDataHistory = NULL;
    Pipe *unitPipe = NULL;
    const char *unitName, *failureCmd;
    char **cmdline = NULL;

    rv = input = rvMutex = 0;
    unitName = failureCmd = NULL;

    /* Get the unit data*/
    unit = (Unit *)arg;
    assert(unit);

    unitName = unit->name;
    restart = unit->restart;
    restartMax = unit->restartMax;
    restartNum = &unit->restartNum;
    pData = &unit->processData;
    pDataHistory = unit->processDataHistory;
    failureCmd = unit->failureCmd;
    unitPipe = unit->pipe;
    assert(unitPipe);

    if ((rv = pthread_mutex_lock(unitPipe->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "listenPipe",
                      rv, strerror(rv), "Unable to lock the pipe mutex for the %s unit",
                      unitName);
        kill(UNITD_PID, SIGTERM);
    }

    if ((rv = pthread_mutex_unlock(unit->mutex)) != 0) {
        logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "listenPipe",
                      rv, strerror(rv), "Unable to lock the mutex for the %s unit",
                      unitName);
        kill(UNITD_PID, SIGTERM);
    }

    while (1) {
        if ((rv = uRead(unitPipe->fds[0], &input, sizeof(int))) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "listenPipe", errno,
                          strerror(errno), "Unable to read from pipe for '%s'",
                          unitName);
            kill(UNITD_PID, SIGTERM);
            goto out;
        }
        if (input == THREAD_EXIT)
            goto out;

        /* Before to run whatever, we wait for system is up.
         * We check if ctrl+alt+del is pressed as well.
        */
        while (!LISTEN_SOCK_REQUEST && SHUTDOWN_COMMAND == NO_COMMAND)
            msleep(50);
        if (SHUTDOWN_COMMAND != NO_COMMAND)
            goto out;

        /* Execute an eventual failure command */
        if (failureCmd) {
            cmdline = cmdlineSplit(failureCmd);
            assert(cmdline);
            logInfo(SYSTEM, "%s: executing '%s' failure command ...", unitName, failureCmd);
            rv = execFailure(cmdline[0], cmdline, &unit);
            cmdlineRelease(cmdline);
            if (rv == 0)
                logSuccess(SYSTEM, "%s: '%s' failure command executed successfully!",
                           unitName, failureCmd);
            else {
                logErrorStr(SYSTEM, "%s: '%s' failure command returned %d exit code!",
                            unitName, failureCmd, rv);
                arrayAdd(unit->errors, getMsg(-1, UNITD_ERRORS_ITEMS[UNITD_GENERIC_ERR].desc));
            }
        }

        if ((restartMax > 0 && *restartNum < restartMax) ||
            (restartMax == -1 && restart)) {
            /* We lock the mutex to allow unitStatusServer func to retrieve the completed data.
             * If the lock fails then it's not a problem. We will see the incompleted data but
             * a next unit status execution will show the completed data.
            */
            if ((rvMutex = pthread_mutex_lock(unit->mutex)) != 0) {
                logError(SYSTEM, "src/core/processes/process.c", "listenPipe",
                              rvMutex, strerror(rvMutex), "Unable to acquire the lock of the mutex for the %s unit",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            /* Before the restart the daemon, we add the current processData to
             * processDataHistory.
             * If restartNum achieved the 'SHOW_MAX_RESULTS' value, we increment it but we will always
             * remove the first element from the history because we only show
             * the last 'SHOW_MAX_RESULTS' results
            */
            if (*restartNum >= SHOW_MAX_RESULTS)
                arrayRemoveAt(pDataHistory, 0);
            /* Creating the history */
            arrayAdd(pDataHistory, processDataNew(*pData, PARSE_UNIT));
            /* Reset values for the current ProcessData */
            resetPDataForRestart(pData);
            /* Incrementing restartNum */
            (*restartNum)++;
            /* Unlock */
            if ((rvMutex = pthread_mutex_unlock(unit->mutex)) != 0) {
                logError(SYSTEM, "src/core/processes/process.c", "listenPipe",
                              rvMutex, strerror(rvMutex), "Unable to unlock the mutex for the %s unit",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            if (SHUTDOWN_COMMAND == NO_COMMAND) {
                if (UNITD_DEBUG)
                    logInfo(SYSTEM, "%s unit is restarting ....", unitName);
                /* Slowing it */
                msleep(1500);
                startProcesses(&UNITD_DATA->units, unit);
            }
        }
        else
            goto out;
    }

    out:
        /* Unlock pipe mutex */
        if ((rv = pthread_mutex_unlock(unitPipe->mutex)) != 0) {
            logError(CONSOLE, "src/core/processes/process.c", "listenPipe",
                          rv, strerror(rv), "Unable to unlock the pipe mutex for the %s unit",
                          unitName);
            kill(UNITD_PID, SIGTERM);
        }
        pthread_exit(0);
}

bool
hasPipe(Unit *unit)
{
    assert(unit);
    if (unit->restart || unit->restartMax > 0 || unit->failureCmd)
        return true;

    return false;
}

Array*
getRestartableUnits(Array **units)
{
    Array *restartableUnits = arrayNew(NULL);
    int lenUnits = 0;
    Unit *unit = NULL;

    lenUnits = (*units ? (*units)->size : 0);
    for (int i = 0; i < lenUnits; i++) {
        unit = arrayGet(*units, i);
        /* We exclude the timers which have a different restart concept. */
        if (unit->pipe && (unit->type != TIMER))
            arrayAdd(restartableUnits, unit);
    }
    return restartableUnits;
}

int
listenPipes(Array **units, Unit *unitArg)
{
    int rv, numThreads;
    Unit *unit = NULL;
    const char *unitName = NULL;
    Array *restartableUnits = NULL;

    rv = numThreads = 0;

    if (!unitArg)
        restartableUnits = getRestartableUnits(units);
    else {
        restartableUnits = arrayNew(NULL);
        arrayAdd(restartableUnits, unitArg);
    }

    numThreads = (restartableUnits ? restartableUnits->size : 0);
    if (numThreads > 0) {

        pthread_t threads[numThreads];
        pthread_attr_t attr[numThreads];

        if (UNITD_DEBUG)
            logWarning(UNITD_BOOT_LOG, "\n[*] CREATING %d DETACHED THREADS (listen pipe) \n", numThreads);

        for (int i = 0; i < numThreads; i++) {
            /* Get the unit */
            unit = arrayGet(restartableUnits, i);
            assert(unit);
            unitName = unit->name;
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Creating '%s' detached thread (listen pipe)\n", unitName);

            /* Set pthread attributes and start */
            if ((rv = pthread_attr_init(&attr[i])) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "listenPipes", errno,
                              strerror(errno), "pthread_attr_init returned bad exit code %d for %s",
                              rv, unitName);
                kill(UNITD_PID, SIGTERM);
            }

            if ((rv = pthread_attr_setdetachstate(&attr[i], PTHREAD_CREATE_DETACHED)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "listenPipes", errno,
                              strerror(errno), "pthread_attr_init returned bad exit code %d for %s",
                              rv, unitName);
                kill(UNITD_PID, SIGTERM);
            }

            /* It will be unlocked by listenPipeThread. */
            handleMutex(unit->mutex, true);

            if ((rv = pthread_create(&threads[i], &attr[i], listenPipe, unit)) != 0) {
                logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "listenPipes", rv,
                              strerror(rv), "Unable to create the detached thread for '%s' (listen pipe)",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Detached thread created succesfully for '%s' (listen pipe)\n", unitName);
            }

            /* We assure us that the pipes are listening. */
            handleMutex(unit->mutex, true);
            handleMutex(unit->mutex, false);
            pthread_attr_destroy(&attr[i]);
        }
    }
    /* We only release restartableUnits array and restartableUnits->arr member because the inside elements (units)
     * will be released by unitdEnd function
    */
    arrayRelease(&restartableUnits);
    return rv;
}

void*
closePipe(void *arg)
{
    Unit *unit = NULL;
    int rv, *rvThread, output;
    const char *unitName = NULL;
    Pipe *unitPipe = NULL;
    pthread_mutex_t *mutex = NULL;

    rv = 0;
    unit = (Unit *)arg;
    assert(unit);
    unitName = unit->name;

    if (unit->pipe) {
        unitPipe = unit->pipe;
        mutex = unitPipe->mutex;
        output = THREAD_EXIT;
        if ((rv = uWrite(unitPipe->fds[1], &output, sizeof(int))) == -1) {
            logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "closePipe",
                          errno, strerror(errno), "Unable to write into pipe for the %s unit", unitName);
            kill(UNITD_PID, SIGTERM);
        }
        if ((rv = pthread_mutex_lock(mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "closePipe",
                          rv, strerror(rv), "Unable to acquire the lock of the pipe mutex for the %s unit",
                          unitName);
            kill(UNITD_PID, SIGTERM);
        }
        if ((rv = pthread_mutex_unlock(mutex)) != 0) {
            logError(CONSOLE | SYSTEM, "src/core/processes/process.c", "closePipe",
                          rv, strerror(rv), "Unable to unlock the pipe mutex for the %s unit",
                          unitName);
            kill(UNITD_PID, SIGTERM);
        }
    }

    rvThread = calloc(1, sizeof(int));
    assert(rvThread);
    *rvThread = rv;
    return rvThread;
}

int
closePipes(Array **units, Unit *unitArg)
{
    int rv, numThreads, *rvThread;
    Unit *unit = NULL;
    const char *unitName = NULL;
    Array *restartableUnits = NULL;

    rv = numThreads = 0;

    if (!unitArg)
        restartableUnits = getRestartableUnits(units);
    else {
        restartableUnits = arrayNew(NULL);
        arrayAdd(restartableUnits, unitArg);
    }

    numThreads = (restartableUnits ? restartableUnits->size : 0);
    if (numThreads > 0) {
        pthread_t threads[numThreads];
        if (UNITD_DEBUG)
            logWarning(UNITD_BOOT_LOG, "\n[*] CREATING %d THREADS (close pipe)\n", numThreads);
        for (int i = 0; i < numThreads; i++) {
            /* Get the unit */
            unit = arrayGet(restartableUnits, i);
            assert(unit);
            unitName = unit->name;
            if (UNITD_DEBUG)
                logInfo(UNITD_BOOT_LOG, "Creating '%s' thread (close pipe)\n", unitName);

            if ((rv = pthread_create(&threads[i], NULL, closePipe, unit)) != 0) {
                logError(UNITD_BOOT_LOG, "src/core/processes/process.c", "closePipes", rv,
                              strerror(rv), "Unable to create the thread for '%s' (close pipe)",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread created succesfully for '%s' (close pipe)\n", unitName);
            }
        }
        /* Waiting for all threads to terminate */
        for (int i = 0; i < numThreads; i++) {
            /* Get the unit */
            unit = arrayGet(restartableUnits, i);
            assert(unit);
            unitName = unit->name;
            if ((rv = pthread_join(threads[i], (void **)&rvThread)) != 0) {
                logError(UNITD_BOOT_LOG, "src/core/processes/process.c", "closePipes", rv,
                              strerror(rv), "Unable to join the thread for '%s' (close pipe)",
                              unitName);
                kill(UNITD_PID, SIGTERM);
            }
            else {
                if (UNITD_DEBUG)
                    logInfo(UNITD_BOOT_LOG, "Thread joined successfully for '%s' (close pipe)! Return code = %d\n",
                                 unitName, *rvThread);
            }
            if (*rvThread == 1)
                rv = 1;
            objectRelease(&rvThread);
        }
    }
    /* We only release restartableUnits array and restartableUnits->arr member because the inside elements (units)
     * will be released by unitdEnd function
    */
    arrayRelease(&restartableUnits);
    return rv;
}
