/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITD_H
#define UNITD_H

#include "unitd_config.h"
#include "unitd_types.h"

/* Free a SockMessageOut struct */
void
sockMessageOutRelease(SockMessageOut **sockMessageOut);

/* Perform a reboot, halt or poweroff the system */
int
unitdShutdown(Command command, bool force, bool noWtmp);

/* List all the units.
 * The function will populate a sockMessageOut->unitsDisplay array.
 * Each element of this array is an unit struct.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
getUnitList(SockMessageOut **sockMessageOut);

/* Get the unit data.
 * The "unit name" argument can also contain the ".unit" suffix.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
getUnitStatus(SockMessageOut **sockMessageOut, const char *unitName);

/* Stop the unit
 * The "unit name" argument can also contain the ".unit" suffix.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
stopUnit(SockMessageOut **sockMessageOut, const char *unitName);

/* Start/Restart the unit.
 * The "unit name" argument can also contain the ".unit" suffix.
 * To resolve the eventual conflicts, invoke it with "force" = true.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
startUnit(SockMessageOut **sockMessageOut, const char *unitName, bool force, bool restart);

/* Disable the unit.
 * The "unit name" argument can also contain the ".unit" suffix.
 * You can also stop this unit invoking it with "run" = true.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
disableUnit(SockMessageOut **sockMessageOut, const char *unitName, bool run);

/* Enable the unit.
 * The "unit name" argument can also contain the ".unit" suffix.
 * You can also start this unit invoking it with "run" = true and "force" = true
 * to resolve eventual conflicts.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
enableUnit(SockMessageOut **sockMessageOut, const char *unitName, bool force, bool run);

/* Get the dependencies, conflicts or unit wanted states according the boolean parameters values.
 * The "unit name" argument can also contain the ".unit" suffix.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
getUnitData(SockMessageOut **sockMessageOut, const char *unitName,
            bool requires, bool conflicts, bool states);

/* Get the default state.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
getDefaultState(SockMessageOut **sockMessageOut);

/* Set the default state.
 * The "state" argument can also contain the ".state" suffix.
 * SockMessageOut struct must be freed via the sockMessageOutRelease function.
*/
int
setDefaultState(SockMessageOut **sockMessageOut, const char *state);

#endif // UNITD_H
