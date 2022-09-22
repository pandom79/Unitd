/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

void
assertMacros()
{
    assert(UNITLOGD_PATH);
    assert(UNITLOGD_LOG_PATH);
    assert(UNITLOGD_INDEX_PATH);
    assert(UNITLOGD_BOOT_LOG_PATH);
}

int
unitlogdInit()
{
    int rv = 0;

    /* Env vars */
    Array *envVars = arrayNew(objectRelease);
    addEnvVar(&envVars, "PATH", PATH_ENV_VAR);
    addEnvVar(&envVars, "UNITLOGD_PATH", UNITLOGD_PATH);
    addEnvVar(&envVars, "UNITLOGD_LOG_PATH", UNITLOGD_LOG_PATH);
    addEnvVar(&envVars, "UNITLOGD_INDEX_PATH", UNITLOGD_INDEX_PATH);
    addEnvVar(&envVars, "UNITLOGD_BOOT_LOG_PATH", UNITLOGD_BOOT_LOG_PATH);
    /* Must be null terminated */
    arrayAdd(envVars, NULL);

    /* Building command */
    char *cmd = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&cmd, "/scripts/unitlogd.sh");

    /* Creating script params */
    Array *scriptParams = arrayNew(objectRelease);
    arrayAdd(scriptParams, cmd); //0
    arrayAdd(scriptParams, stringNew("init")); //1
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL);

    /* Execute the script */
    rv = execScript(UNITD_DATA_PATH, "/scripts/unitlogd.sh", scriptParams->arr, envVars->arr);
    if (rv != 0) {
        unitdLogError(LOG_UNITD_CONSOLE, "src/core/unitlogd/common.c",
                      "unitlogdInit", rv, strerror(rv), "ExecScript error");
    }

    arrayRelease(&envVars);
    arrayRelease(&scriptParams);
    return rv;

}
