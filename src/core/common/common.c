/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

Array *UNITD_ENV_VARS;

int
msleep(long msec)
{
   struct timespec ts;
   int res;

   if (msec < 0) {
       errno = EINVAL;
       return -1;
   }
   ts.tv_sec = msec / 1000;
   ts.tv_nsec = (msec % 1000) * 1000000;
   do {
       res = nanosleep(&ts, &ts);
   } while (res && errno == EINTR);

   return res;
}

/*
0 = Valid symlink
1 = Symlink missing
2 = Not a symlink
*/
int
readSymLink(const char *symLink, char **wherePoints)
{
    struct stat sb;
    ssize_t nbytes, bufsiz;
    int rv = 0;

    assert(symLink);
    if ((rv = lstat(symLink, &sb)) == -1) {
        rv = 1;
        goto out;
    }

    /* Set buffer size */
    bufsiz = sb.st_size + 1;
    if (sb.st_size == 0)
        bufsiz = PATH_MAX;

    /* Read the symlink */
    *wherePoints = calloc(bufsiz, sizeof(char));
    assert(*wherePoints);
    nbytes = readlink(symLink, *wherePoints, bufsiz);
    if (nbytes == -1)
        rv = 2;

    out:
        return rv;
}

void
addEnvVar(const char *envVarName, const char *envVarValue)
{
    char *envVar  = NULL;

    assert(envVarName);
    assert(envVarValue);

    if (!UNITD_ENV_VARS)
        UNITD_ENV_VARS = arrayNew(objectRelease);

    envVar = stringNew(envVarName);
    stringAppendChr(&envVar, '=');
    stringAppendStr(&envVar, envVarValue);
    arrayAdd(UNITD_ENV_VARS, envVar);
}

State
getStateByStr(char *stateArg)
{
    State ret = NO_STATE;
    int startIdx, endIdx;
    char *stateStr = NULL;

    startIdx = endIdx = -1;

    startIdx = stringLastIndexOfChr(stateArg, '/');
    endIdx = stringLastIndexOfStr(stateArg, ".state");

    if (startIdx != -1 && endIdx != -1)
        stateStr = stringSub(stateArg, startIdx + 1, endIdx - 1);
    else if (startIdx == -1 && endIdx != -1)
        stateStr = stringSub(stateArg, 0, endIdx - 1);
    else
        stateStr = stringNew(stateArg);

    for (int i = 0; i < STATE_DATA_ITEMS_LEN; i++) {
        if (strcmp(stateStr, STATE_DATA_ITEMS[i].desc) == 0) {
            ret = i;
            break;
        }
    }

    objectRelease(&stateStr);
    return ret;
}

int
getDefaultStateStr(char **destDefStateSyml)
{
    int rv = 0;
    char *defStateSymlPath = NULL;

    /* Building path */
    defStateSymlPath = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&defStateSymlPath, '/');
    stringAppendStr(&defStateSymlPath, DEF_STATE_SYML_NAME);
    if ((rv = readSymLink(defStateSymlPath, destDefStateSyml)) != 0) {
        /* These errors would have to never happen */
        if (rv == 1) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/common/common.c", "getDefaultStateStr", EPERM,
                          strerror(EPERM), "The '%s' symlink is missing", defStateSymlPath);
        }
        else if (rv == 2) {
            unitdLogError(LOG_UNITD_CONSOLE, "src/core/common/common.c", "getDefaultStateStr", EPERM,
                          strerror(EPERM), "The '%s' doesn't look like a symlink", defStateSymlPath);
        }
    }

    objectRelease(&defStateSymlPath);
    return rv;
}

int setNewDefaultStateSyml(State newDefaultState)
{
    int rv = 0;
    Array *scriptParams = arrayNew(objectRelease);
    char *command, *from, *to;

    command = to = from = NULL;

    /* Building from */
    from = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&from, '/');
    stringAppendStr(&from, STATE_DATA_ITEMS[newDefaultState].desc);
    stringAppendStr(&from, ".state");
    /* Building to */
    to = stringNew(UNITS_ENAB_PATH);
    stringAppendChr(&to, '/');
    stringAppendStr(&to, DEF_STATE_SYML_NAME);
    /* Building command */
    command = stringNew(UNITD_DATA_PATH);
    stringAppendStr(&command, "/scripts/symlink-handle.sh");
    arrayAdd(scriptParams, command); //0
    arrayAdd(scriptParams, stringNew(SYML_ADD_OP)); //1
    arrayAdd(scriptParams, from); //2
    arrayAdd(scriptParams, to); //3
    /* Must be null terminated */
    arrayAdd(scriptParams, NULL); //4
    /* Execute the script */
    rv = execScript(UNITD_DATA_PATH, "/scripts/symlink-handle.sh", scriptParams->arr);
    arrayRelease(&scriptParams);
    return rv;
}

void
arrayPrint(int options, Array **array, bool hasStrings)
{
    int len = (*array ? (*array)->size : 0);
    if (hasStrings) {
        for (int i = 0; i < len; i++)
            unitdLogInfo(options, "%s\n", (char *)(*array)->arr[i]);
    }
    else {
        for (int i = 0; i < len; i++)
            unitdLogInfo(options, "%p\n", (*array)->arr[i]);
    }
}
