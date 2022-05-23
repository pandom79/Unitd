/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

bool ENABLE_RESTART;

void*
startProcess(void *arg)
{
    UnitThreadData *unitThreadData = NULL;
    Unit *unit, *unitDep, *unitConflict;
    Array *units, *requires, *conflicts, *wantedBy, *pDataHistory;
    int lenDeps, lenConflicts, statusThread, *finalStatus, *finalStatusDep, *rvThread, rv;
    ProcessData *pData, *pDataDep;
    pthread_mutex_t *unitMutex, *unitDepMutex;
    char **cmdline = NULL;
    const char *command, *unitName, *unitNameDep, *unitNameconflict, *desc;

    lenDeps = lenConflicts = rv = 0;
    rvThread = NULL;
    unit = unitDep = unitConflict = NULL;
    pData = pDataDep = NULL;
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
    stringSetDateTime(&pData->dateTimeStartStr, true);

    /* Lock the mutex */
    if ((rv = pthread_mutex_lock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startProcess",
                      rv, strerror(rv), "Unable to acquire the lock of the mutex for the %s unit",
                      unitName);
        goto out;
    }

    if (UNITD_DEBUG)
        unitdLogWarning(LOG_UNITD_BOOT, "\n[*] STARTING the '%s' UNIT\n", unitName);

    /* If the errors already exist then exit */
    if (unit->errors->size > 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        if (UNITD_DEBUG)
            unitdLogErrorStr(LOG_UNITD_BOOT, "The '%s' unit has some configuration errors. Exit!\n",
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
            arrayAdd(unit->errors, getMsg(-1, UNITS_ERRORS_ITEMS[CONFLICT_EXEC_ERROR].desc,
                                            unitName, unitNameconflict));
            *finalStatus = FINAL_STATUS_FAILURE;
            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "The '%s' unit has a conflict with the '%s' unit. Exit!\n",
                             unitName, unitNameconflict);
            goto out;
        }
    }

    /* Get the dependencies */
    requires = unit->requires;
    lenDeps = (requires ? requires->size : 0);
    if (UNITD_DEBUG)
        unitdLogInfo(LOG_UNITD_BOOT, "The '%s' unit has to wait for %d dependencies!\n",
                     unitName, lenDeps);
    for (int i = 0; i < lenDeps; i++) {
        /* Get the dependency as string */
        unitNameDep = arrayGet(requires, i);
        /* Get the dependency as Unit type. */
        unitDep = getUnitByName(units, unitNameDep);
        /* Actually, when the unit is stopped, doesn't exist into memory.
         * In this way, we are optimizing memory usage and making easier the management
        */
        if (!unitDep || (unitDep && unitDep->errors->size > 0)) {
            /* Set the unsatisfied dependency error in the current unit */
            arrayAdd(unit->errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc,
                                            unitNameDep, unitName));
            *finalStatus = FINAL_STATUS_FAILURE;
            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "The dependency '%s' for the '%s' unit doesn't exist "
                                             "or has some errors. Exit!\n",
                             unitNameDep, unitName);
            goto out;
        }
        else {

            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "The dependency name for the '%s' unit = '%s' have no errors and is enabled. Go on!\n",
                             unitName, unitNameDep);
            /* Get the dependency's Process Data */
            pDataDep = unitDep->processData;
            /* When an unit restarts, its processData could be temporarily NULL (extreme case).
             * See listenPipeThread function.
             * Waiting for it.
            */
            while (!pDataDep)
                msleep(20);
            finalStatusDep = pDataDep->finalStatus;

            /* Lock the dependency mutex */
            unitDepMutex = unitDep->mutex;
            if ((rv = pthread_mutex_lock(unitDepMutex)) != 0) {
                *finalStatusDep = FINAL_STATUS_FAILURE;
                unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startProcess",
                              rv, strerror(rv), "Unable to lock the mutex for the %s dependency",
                              unitNameDep);
            }
            else {
                /* If the dependency is not started yet */
                while (*finalStatusDep == FINAL_STATUS_READY) {
                    if (UNITD_DEBUG)
                        unitdLogInfo(LOG_UNITD_BOOT, "The dependency '%s' for the '%s' unit is not started yet. "
                                                     "Waiting for the broadcast signal...!\n",
                                     unitNameDep, unitName);
                    /* Waiting for the broadcast signal ... */
                    if ((rv = pthread_cond_wait(unitDep->cv, unitDepMutex)) != 0) {
                        *finalStatusDep = FINAL_STATUS_FAILURE;
                        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startProcess",
                                      rv, strerror(rv), "Unable to wait for condition variable for the %s dependency",
                                      unitNameDep);
                        break;
                    }
                    if (UNITD_DEBUG)
                        unitdLogInfo(LOG_UNITD_BOOT,
                                     "The dependency '%s' for the '%s' unit sent the broadcast signal! Final status = %d\n",
                                     unitNameDep, unitName, *finalStatusDep);
                }
                /* Unlock the dependency mutex */
                if ((rv = pthread_mutex_unlock(unitDepMutex)) != 0) {
                    *finalStatusDep = FINAL_STATUS_FAILURE;
                    unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startProcess",
                                  rv, strerror(rv), "Unable to unlock the mutex for the %s dependency",
                                  unitNameDep);
                }
            }//unitDep mutex locked

            if (*finalStatusDep != FINAL_STATUS_SUCCESS) {
                /* Set the unsatisfied dependency error in the current unit */
                arrayAdd(unit->errors, getMsg(-1, UNITS_ERRORS_ITEMS[UNSATISFIED_DEP_ERR].desc,
                                                unitNameDep, unitName));
                /* If the dependency is failed then the unit state is died */
                *finalStatus = FINAL_STATUS_FAILURE;
                *pData->pStateData = PSTATE_DATA_ITEMS[DIED];

                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "The dependency '%s' for the '%s' unit went in ABEND!\n",
                                 unitNameDep, unitName);
                goto out;
            }
        }//dependency exist

    }// for dependencies

    /* If there are no configuration and dependency errors, we execute the command */
    /* Cmdline split */
    command = unit->runCmd;
    cmdline = cmdlineSplit(command);
    assert(cmdline);
    if (UNITD_DEBUG)
        unitdLogInfo(LOG_UNITD_BOOT, "Executing the '%s' command for the '%s' unit ...!\n",
                     command, unitName);

    /* Execute the command */
    statusThread = execProcess(cmdline[0], cmdline, &unit);
    if (statusThread != EXIT_SUCCESS && statusThread != -1) {
        /* For the initialization and finalization units,
         * we show the error and the emergency shell in the console */
        wantedBy = unit->wantedBy;
        if (arrayContainsStr(wantedBy, STATE_DATA_ITEMS[INIT].desc) ||
            arrayContainsStr(wantedBy, STATE_DATA_ITEMS[FINAL].desc)) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startProcess", statusThread,
                          strerror(statusThread), "The '%s' command for the '%s' unit has returned %d exit code\n",
                          command, unitName, statusThread);
        }
    }
    /* Release cmdline */
    cmdlineRelease(cmdline);

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
        default:
            break;
    }

    /* Broadcast signal and unlock */
    if ((rv = pthread_cond_broadcast(unit->cv)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startProcess",
                      rv, strerror(rv), "Unable to send the broadcast signal for the %s unit", unitName);
    }
    if (UNITD_DEBUG)
        unitdLogInfo(LOG_UNITD_BOOT, "The '%s' unit sent the broadcast signal!\n", unitName);

    if ((rv = pthread_mutex_unlock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "startProcess",
                      rv, strerror(rv), "Unable to unlock the mutex for the %s unit", unitName);
    }
    /* show the result */
    if (unit->showResult) {
        if (*finalStatus == FINAL_STATUS_SUCCESS) {
            unitdLogInfo(LOG_UNITD_ALL, "[   %sOK%s   ] %s%s%s\n", GREEN_COLOR, DEFAULT_COLOR,
                         WHITE_COLOR, (desc ? desc : ""), DEFAULT_COLOR);
        }
        else
            unitdLogInfo(LOG_UNITD_ALL, "[ %sFAILED%s ] %s%s%s\n", RED_COLOR, DEFAULT_COLOR,
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
    int rv, numThreads, *rvThread;
    Unit *unit = NULL;
    const char *unitName = NULL;

    rv = numThreads = 0;

    if (!singleUnit)
        numThreads = (*units ? (*units)->size : 0);
    else
        numThreads = 1;

    if (numThreads > 0) {
        UnitThreadData unitsThreadsData[numThreads];
        if (UNITD_DEBUG)
            unitdLogWarning(LOG_UNITD_BOOT, "\n[*] CREATING THE %d THREADS (STARTING)\n", numThreads);
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
                unitdLogInfo(LOG_UNITD_BOOT, "Creating the '%s' unit thread\n", unitName);

            /* Set the unit thread data */
            unitThreadData->units = *units;
            unitThreadData->unit = unit;
            if ((rv = pthread_create(&unitThreadData->thread, NULL, startProcess, unitThreadData)) != 0) {
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "startProcesses", errno,
                              strerror(errno), "Unable to create the thread for the '%s' unit",
                              unitName);
                break;
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread created succesfully for the '%s' unit\n", unitName);
            }
        }
        /* Waiting for all threads to terminate */
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            unit = unitThreadData->unit;
            unitName = unit->name;
            if (pthread_join(unitThreadData->thread, (void **)&rvThread) != EXIT_SUCCESS) {
                rv = 1;
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "startProcesses", rv,
                              strerror(rv), "Unable to join the thread for the '%s' unit",
                              unitName);
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread joined successfully for the '%s' unit! Return code = %d\n",
                                 unitName, *rvThread);
            }
            if (*rvThread == 1)
                rv = 1;
            objectRelease(&rvThread);
        }
    }

    return rv;
}

Array*
getDaemonUnits(Array **units)
{
    Array *daemonUnits = arrayNew(NULL);
    int lenUnits = 0;
    Unit *unit = NULL;

    lenUnits = (*units ? (*units)->size : 0);
    for (int i = 0; i < lenUnits; i++) {
        unit = arrayGet(*units, i);
        if (unit->type == DAEMON && *unit->processData->finalStatus == FINAL_STATUS_SUCCESS)
            arrayAdd(daemonUnits, unit);
    }

    return daemonUnits;
}

void*
stopProcess(void *arg)
{
    UnitThreadData *unitThreadData = NULL;
    Unit *unit = NULL;
    const char *command, *unitName;
    char **cmdline = NULL;
    int statusThread, *finalStatus, rv;
    int *rvThread = NULL;
    ProcessData *pData = NULL;
    pthread_mutex_t *unitMutex = NULL;

    command = unitName = NULL;
    rv = 0;

    assert(arg);
    unitThreadData = (UnitThreadData *)arg;
    /* Get the unit data */
    unit = unitThreadData->unit;
    assert(unit);
    assert(unit->type == DAEMON);
    unitMutex = unit->mutex;
    pData = unit->processData;
    finalStatus = pData->finalStatus;
    unitName = unit->name;

    /* Lock the mutex */
    if ((rv = pthread_mutex_lock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "stopProcess",
                      rv, strerror(rv), "Unable to acquire the lock of the mutex for the %s unit",
                      unitName);
        goto out;
    }

    command = unit->stopCmd;
    if (command) {
        cmdline = cmdlineSplit(command);
        assert(cmdline);
        /* Execute the command */
        statusThread = stopDaemon(cmdline[0], cmdline, &unit);
        cmdlineRelease(cmdline);
    }
    else
        statusThread = stopDaemon(NULL, NULL, &unit);


    /* Evaluating the final status */
    if (statusThread == -1 && *pData->exitCode == -1 && *pData->pid != -1 &&
        pData->pStateData->pState == DIED)
        *finalStatus = FINAL_STATUS_NOT_READY;
    else
        *finalStatus = FINAL_STATUS_FAILURE;

    if ((rv = pthread_mutex_unlock(unitMutex)) != 0) {
        *finalStatus = FINAL_STATUS_FAILURE;
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "stopProcess",
                      rv, strerror(rv), "Unable to unlock of the mutex for the %s unit", unitName);
        goto out;
    }

    out:
        if (unit->showResult) {
            if (*finalStatus == FINAL_STATUS_NOT_READY) {
                unitdLogInfo(LOG_UNITD_ALL, "[   %sOK%s   ] %s%s%s\n", GREEN_COLOR, DEFAULT_COLOR,
                             WHITE_COLOR, unit->desc, DEFAULT_COLOR);
            }
            else
                unitdLogInfo(LOG_UNITD_ALL, "[ %sFAILED%s ] %s%s%s\n", RED_COLOR, DEFAULT_COLOR,
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
    Array *daemonUnits = NULL;

    rv = numThreads = 0;

    /* We only get the daemon units */
    if (!unitArg)
        daemonUnits = getDaemonUnits(units);
    else {
        daemonUnits = arrayNew(NULL);
        arrayAdd(daemonUnits, unitArg);
    }
    numThreads = (daemonUnits ? daemonUnits->size : 0);
    if (numThreads > 0) {
        UnitThreadData unitsThreadsData[numThreads];
        if (UNITD_DEBUG)
            unitdLogWarning(LOG_UNITD_BOOT, "\n[*] CREATING THE %d THREADS (STOPPING) \n", numThreads);
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            /* Get the unit */
            unit = arrayGet(daemonUnits, i);
            assert(unit);
            if (unitArg)
                unit->showResult = false;
            unitName = unit->name;
            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "Creating the '%s' unit thread\n", unitName);

            /* Set the unit */
            unitThreadData->unit = unit;
            if ((rv = pthread_create(&unitThreadData->thread, NULL, stopProcess, unitThreadData)) != 0) {
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "stopProcesses", errno,
                              strerror(errno), "Unable to create the thread for the '%s' unit",
                              unitName);
                break;
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread created succesfully for the '%s' unit\n", unitName);
            }
        }
        /* Waiting for all threads to terminate */
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            unit = unitThreadData->unit;
            unitName = unit->name;
            if (pthread_join(unitThreadData->thread, (void **)&rvThread) != EXIT_SUCCESS) {
                rv = 1;
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "stopProcesses", rv,
                              strerror(rv), "Unable to join the thread for the '%s' unit",
                              unitName);
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread joined successfully for the '%s' unit! Return code = %d\n",
                                 unitName, *rvThread);
            }
            if (*rvThread == 1)
                rv = 1;
            objectRelease(&rvThread);
        }
    }

    /* We only release daemonUnits array and daemonUnits->arr member because the inside elements (units)
     * will be released by unitdEnd function
    */
    arrayRelease(&daemonUnits);
    return rv;
}

void*
listenPipeThread(void *arg)
{
    UnitThreadData *unitThreadData = NULL;
    Unit *unit = NULL;
    int rv, input, restartMax, *restartNum;
    bool restart = false;
    ProcessData **pData = NULL;
    Array *pDataHistory = NULL;
    Pipe *unitPipe = NULL;
    const char *unitName = NULL;

    rv = input = 0;

    assert(arg);
    unitThreadData = (UnitThreadData *)arg;
    /* Get the unit data*/
    unit = unitThreadData->unit;

    assert(unit);

    unitName = unit->name;
    restart = unit->restart;
    restartMax = unit->restartMax;
    restartNum = &unit->restartNum;
    pData = &unit->processData;
    pDataHistory = unit->processDataHistory;
    unitPipe = unit->pipe;
    assert(unitPipe);

   if ((rv = pthread_mutex_lock(unitPipe->mutex)) != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "listenPipeThread",
                      rv, strerror(rv), "Unable to acquire the lock of the pipe mutex for the %s unit",
                      unitName);
        goto out;
    }

    while (1) {
        if ((rv = read(unitPipe->fds[0], &input, sizeof(int))) == -1) {
            unitdLogError(LOG_UNITD_ALL, "src/core/processes/process.c", "listenPipeThread", errno,
                          strerror(errno), "Unable to read from pipe for the '%s' unit",
                          unitName);
            goto out;
        }
        if (input == PIPE_THREAD_EXIT)
            goto out;

        /* The restart is blocked until all threads are terminated. See init.c */
        while (!ENABLE_RESTART)
            msleep(750);

        if ((restartMax > 0 && *restartNum < restartMax) ||
            (restartMax == -1 && restart)) {
            /* Before the restart the daemon, we add the current processData to
             * processDataHistory.
             * If restartNum achieved the 'SHOW_MAX_RESULTS' value, we increment it but we will always
             * remove the first element from the history because we only show
             * the last 'SHOW_MAX_RESULTS' results
            */
            /* Slowing it */
            msleep(1500);
            if (*restartNum >= SHOW_MAX_RESULTS)
                arrayRemoveAt(pDataHistory, 0);
            /* Creating the history */
            arrayAdd(pDataHistory, processDataNew(*pData, PARSE_UNIT));
            /* Release and create a new Process Data */
            processDataRelease(pData);
            *pData = processDataNew(NULL, PARSE_UNIT);
            /* Incrementing restartNum */
            (*restartNum)++;
            *(*pData)->pStateData = PSTATE_DATA_ITEMS[RESTARTING];
            if (SHUTDOWN_COMMAND == NO_COMMAND) {
                if (UNITD_DEBUG)
                    syslog(LOG_DAEMON | LOG_DEBUG, "%s unit is restarting ....", unitName);
                startProcesses(&UNITD_DATA->units, unit);
            }
        }
        else
            goto out;
    }

    out:
        /* Unlock mutex pipe */
        if ((rv = pthread_mutex_unlock(unitPipe->mutex)) != 0) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "listenPipeThread",
                          rv, strerror(rv), "Unable to acquire the lock of the pipe mutex for the %s unit",
                          unitName);
        }
        objectRelease(&unitThreadData);
        pthread_exit(0);
}

void
listenPipe(Unit *unit)
{
    int rv = 0;
    UnitThreadData *unitThreadData = NULL;
    pthread_attr_t attr;
    const char *unitName = NULL;

    unitThreadData = calloc(1, sizeof(UnitThreadData));
    assert(unitThreadData);
    unitThreadData->unit = unit;
    unitName = unit->name;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if ((rv = pthread_create(&unitThreadData->thread, &attr, listenPipeThread, unitThreadData)) != 0) {
        syslog(LOG_DAEMON | LOG_ERR, "Unable to create the pipe thread for the '%s' unit",
               unitName);
    }
    else {
        if (UNITD_DEBUG)
            syslog(LOG_DAEMON | LOG_DEBUG, "Pipe Thread created succesfully for the %s unit\n", unitName);
    }
}

bool
hasPipe(Unit *unit)
{
    bool restart;
    int restartMax, restartNum;

    assert(unit);

    /* Create the pipe if we are in STATE_CMDLINE or STATE_DEFAULT and we have to restart */
    /* Get the properties related the restart feature */
    restart = unit->restart;
    restartMax = unit->restartMax;
    restartNum = unit->restartNum;
    if ((restart && restartMax == -1) || (restartMax > 0 && restartNum < restartMax))
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
        if (unit->errors->size == 0 && !unit->pipe && hasPipe(unit))
            arrayAdd(restartableUnits, unit);
    }
    return restartableUnits;
}

void*
openPipe(void *arg)
{
    UnitThreadData *unitThreadData = NULL;
    Unit *unit = NULL;
    int rv, *rvThread;

    rv = 0;

    assert(arg);
    unitThreadData = (UnitThreadData *)arg;
    unit = unitThreadData->unit;
    unit->pipe = pipeNew();
    unit->processDataHistory = arrayNew(processDataRelease);

    if ((rv = pipe(unit->pipe->fds)) != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "openPipe", errno,
                      strerror(errno), "Unable to run pipe for the '%s' unit\n",
                      unit->name);
        goto out;
    }

    listenPipe(unit);

    out:
        rvThread = calloc(1, sizeof(int));
        assert(rvThread);
        *rvThread = rv;
        return rvThread;
}

int
openPipes(Array **units, Unit *unitArg)
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
        UnitThreadData unitsThreadsData[numThreads];
        if (UNITD_DEBUG)
            unitdLogWarning(LOG_UNITD_BOOT, "\n[*] CREATING THE %d THREADS (open pipe) \n", numThreads);
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            /* Get the unit */
            unit = arrayGet(restartableUnits, i);
            assert(unit);
            unitName = unit->name;
            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "Creating the '%s' unit thread (open pipe)\n", unitName);

            /* Set the unit */
            unitThreadData->unit = unit;
            if ((rv = pthread_create(&unitThreadData->thread, NULL, openPipe, unitThreadData)) != 0) {
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "openPipes", errno,
                              strerror(errno), "Unable to create the thread for the '%s' unit (open pipe)",
                              unitName);
                break;
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread created succesfully for the '%s' unit (open pipe)\n", unitName);
            }
        }
        /* Waiting for all threads to terminate */
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            unit = unitThreadData->unit;
            unitName = unit->name;
            if (pthread_join(unitThreadData->thread, (void **)&rvThread) != EXIT_SUCCESS) {
                rv = 1;
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "openPipes", rv,
                              strerror(rv), "Unable to join the thread for the '%s' unit (open pipe)",
                              unitName);
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread joined successfully for the '%s' unit (open pipe)! Return code = %d\n",
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

Array*
getUnitsPipes(Array **units)
{
    Array *unitsPipes = arrayNew(NULL);
    int lenUnits = 0;
    Unit *unit = NULL;

    lenUnits = (*units ? (*units)->size : 0);
    for (int i = 0; i < lenUnits; i++) {
        unit = arrayGet(*units, i);
        if (unit->pipe)
            arrayAdd(unitsPipes, unit);
    }
    return unitsPipes;
}

void*
closePipe(void *arg)
{
    UnitThreadData *unitThreadData = NULL;
    Unit *unit = NULL;
    int rv, *rvThread, output;
    const char *unitName = NULL;
    Pipe *unitPipe = NULL;
    pthread_mutex_t *mutex = NULL;

    rv = 0;
    assert(arg);
    unitThreadData = (UnitThreadData *)arg;
    unit = unitThreadData->unit;
    unitName = unit->name;
    unitPipe = unit->pipe;
    mutex = unitPipe->mutex;

    output = PIPE_THREAD_EXIT;
    if ((rv = write(unitPipe->fds[1], &output, sizeof(int))) == -1) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "closePipe",
                      errno, strerror(errno), "Unable to write into pipe for the %s unit", unitName);
        goto out;
    }
    if ((rv = pthread_mutex_lock(mutex)) != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "closePipe",
                      rv, strerror(rv), "Unable to acquire the lock of the pipe mutex for the %s unit",
                      unitName);
        goto out;
    }
    /* Unlock the pipe mutex */
    if ((rv = pthread_mutex_unlock(mutex)) != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/processes/process.c", "closePipe",
                      rv, strerror(rv), "Unable to acquire the lock of the pipe mutex for the %s unit",
                      unitName);
    }

    out:
        pipeRelease(&unitPipe);
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
    Array *unitsPipes = NULL;

    rv = numThreads = 0;

    if (!unitArg)
        unitsPipes = getUnitsPipes(units);
    else {
        unitsPipes = arrayNew(NULL);
        arrayAdd(unitsPipes, unitArg);
    }

    numThreads = (unitsPipes ? unitsPipes->size : 0);
    if (numThreads > 0) {
        UnitThreadData unitsThreadsData[numThreads];
        if (UNITD_DEBUG)
            unitdLogWarning(LOG_UNITD_BOOT, "\n[*] CREATING THE %d THREADS (close pipe)\n", numThreads);
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            /* Get the unit */
            unit = arrayGet(unitsPipes, i);
            assert(unit);
            unitName = unit->name;
            if (UNITD_DEBUG)
                unitdLogInfo(LOG_UNITD_BOOT, "Creating the '%s' unit thread (close pipe)\n", unitName);

            /* Set the unit */
            unitThreadData->unit = unit;
            if ((rv = pthread_create(&unitThreadData->thread, NULL, closePipe, unitThreadData)) != 0) {
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "closePipes", errno,
                              strerror(errno), "Unable to create the thread for the '%s' unit (close pipe)",
                              unitName);
                break;
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread created succesfully for the '%s' unit (close pipe)\n", unitName);
            }
        }
        /* Waiting for all threads to terminate */
        for (int i = 0; i < numThreads; i++) {
            UnitThreadData *unitThreadData = &unitsThreadsData[i];
            unit = unitThreadData->unit;
            unitName = unit->name;
            if (pthread_join(unitThreadData->thread, (void **)&rvThread) != EXIT_SUCCESS) {
                rv = 1;
                unitdLogError(LOG_UNITD_BOOT, "src/core/processes/process.c", "closePipes", rv,
                              strerror(rv), "Unable to join the thread for the '%s' unit (close pipe)",
                              unitName);
            }
            else {
                if (UNITD_DEBUG)
                    unitdLogInfo(LOG_UNITD_BOOT, "Thread joined successfully for the '%s' unit (close pipe)! Return code = %d\n",
                                 unitName, *rvThread);
            }
            if (*rvThread == 1)
                rv = 1;
            objectRelease(&rvThread);
        }
    }
    /* We only release unitsPipes array and unitsPipes->arr member because the inside elements (units)
     * will be released by unitdEnd function
    */
    arrayRelease(&unitsPipes);
    return rv;
}
