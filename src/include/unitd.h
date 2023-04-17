/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITD_H
#define UNITD_H

/**
 * @brief Unitd init system library
 * @file unitd.h
 * @author Domenico Panella <pandom79@gmail.com>
 * @section License
 *
 * Copyright (C) 2021,<br>
 * This program is free software.<br>
 * You can redistribute it and/or modify it under the terms of the GNU General Public License version 3.<br>
 * See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
 *
*/

#include "unitd_config.h"
#include "unitd_types.h"
#include "unitlogd/unitlogd.h"

/**
 * Free a SockMessageOut struct
 * @param[in] sockMessageOut
 */
void sockMessageOutRelease(SockMessageOut **sockMessageOut);

/**
 * Reboot, poweroff or halt the system.<br>
 * For the system instance, the available commands are:
 * <ul>
 *  <li>REBOOT_COMMAND</li>
 *  <li>HALT_COMMAND</li>
 *  <li>POWEROFF_COMMAND</li>
 *  <li>KEXEC_COMMAND</li>
 * </ul>
 * For the user instance, the available commands are:
 * <ul>
 *  <li>POWEROFF_COMMAND</li>
 * </ul>
 * @param[in] command   One of the above commands
 * @param[in] force     Force this operation
 * @param[in] noWtmp    Write or not a wtmp record
 * @param[in] noWall    Send or not a message to all users
 * @return integer
 */
int unitdShutdown(Command command, bool force, bool noWtmp, bool noWall);

/**
 * List all the units or analyze the boot process.<br>
 * The function will populate a sockMessageOut->unitsDisplay array.<br>
 * Each element of this array is an unit struct.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease() function.
 * @param sockMessageOut
 * @param bootAnalyze
 * @param listFilter
 * @return integer
 */
int getUnitList(SockMessageOut **sockMessageOut, bool bootAnalyze, ListFilter listFilter);

/**
 * Get the unit status.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease() function.
 * @param sockMessageOut
 * @param unitName
 * @return integer
 */
int getUnitStatus(SockMessageOut **sockMessageOut, const char *unitName);

/**
 * Stop the unit.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease() function.
 * @param sockMessageOut
 * @param unitName
 * @return integer
 */
int stopUnit(SockMessageOut **sockMessageOut, const char *unitName);

/**
 * Start/Restart the unit.<br>
 * To resolve eventual conflicts, invoke it with "force" = true.<br>
 * The reset option is used by timer units to reset the count starting by current time.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease() function.
 * @param sockMessageOut
 * @param unitName
 * @param force
 * @param restart
 * @param reset
 * @return integer
 */
int startUnit(SockMessageOut **sockMessageOut, const char *unitName, bool force, bool restart, bool reset);

/**
 * Disable the unit.<br>
 * You can also stop this unit invoking it with "run" = true.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease() function.
 * @param sockMessageOut
 * @param unitName
 * @param run
 * @return integer
 */
int disableUnit(SockMessageOut **sockMessageOut, const char *unitName, bool run);

/**
 * Enable/Re-enable the unit.<br>
 * You can also start the unit invoking it with "run" = true and "force" = true
 * to resolve eventual conflicts.<br>
 * The reset option only works for timer unit only if "run" option is set as well.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease() function.
 * @param sockMessageOut
 * @param unitName
 * @param force
 * @param run
 * @param reEnable
 * @param reset
 * @return integer
 */
int enableUnit(SockMessageOut **sockMessageOut, const char *unitName, bool force,
               bool run, bool reEnable, bool reset);

/**
 * Get the dependencies, conflicts or unit wanted states according the boolean parameters values.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
 * @param sockMessageOut
 * @param unitName
 * @param requires
 * @param conflicts
 * @param states
 * @return integer
 */
int getUnitData(SockMessageOut **sockMessageOut, const char *unitName,
                bool requires, bool conflicts, bool states);

/**
 * Get the default state.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease() function.
 * @param sockMessageOut
 * @return integer
 */
int getDefaultState(SockMessageOut **sockMessageOut);

/**
 * Set the default state.<br>
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
 * @param sockMessageOut
 * @param state
 * @return integer
 */
int setDefaultState(SockMessageOut **sockMessageOut, const char *state);

#endif // UNITD_H
