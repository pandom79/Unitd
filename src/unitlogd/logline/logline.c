/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitlogd_impl.h"

static Code PRIORITYNAMES[] = {
  { "alert", LOG_ALERT },
  { "crit", LOG_CRIT },
  { "debug", LOG_DEBUG },
  { "emerg", LOG_EMERG },
  { "err", LOG_ERR },
  { "error", LOG_ERR }, /* DEPRECATED */
  { "info", LOG_INFO },
  { "none", INTERNAL_NOPRI }, /* INTERNAL */
  { "notice", LOG_NOTICE },
  { "panic", LOG_EMERG }, /* DEPRECATED */
  { "warn", LOG_WARNING }, /* DEPRECATED */
  { "warning", LOG_WARNING },
  { NULL, -1 }
};

static Code FACILITYNAMES[] = {
  { "auth", LOG_AUTH },
  { "authpriv", LOG_AUTHPRIV },
  { "cron", LOG_CRON },
  { "daemon", LOG_DAEMON },
  { "ftp", LOG_FTP },
  { "kern", LOG_KERN },
  { "lpr", LOG_LPR },
  { "mail", LOG_MAIL },
  { "mark", INTERNAL_MARK }, /* INTERNAL */
  { "news", LOG_NEWS },
  { "security", LOG_AUTH }, /* DEPRECATED */
  { "syslog", LOG_SYSLOG },
  { "user", LOG_USER },
  { "uucp", LOG_UUCP },
  { "local0", LOG_LOCAL0 },
  { "local1", LOG_LOCAL1 },
  { "local2", LOG_LOCAL2 },
  { "local3", LOG_LOCAL3 },
  { "local4", LOG_LOCAL4 },
  { "local5", LOG_LOCAL5 },
  { "local6", LOG_LOCAL6 },
  { "local7", LOG_LOCAL7 },
  { NULL, -1 }
};

LogLine*
logLineNew()
{
    LogLine *logLine = calloc(1, sizeof(LogLine));
    assert(logLine);

    return logLine;
}

void
logLineRelease(LogLine **logLine)
{
    if (*logLine) {
        objectRelease(&(*logLine)->pri);
        objectRelease(&(*logLine)->fac);
        objectRelease(&(*logLine)->hostName);
        objectRelease(&(*logLine)->timeStamp);
        objectRelease(logLine);
    }
}

static int
setFacAndPri(char *buffer, LogLine **logLine)
{
    int rv = 0, startIdx, endIdx, value = 0;
    char *valueStr = NULL;
    Code *fac, *pri;

    assert(*logLine);

    startIdx = stringIndexOfChr(buffer, '<');
    endIdx = stringIndexOfChr(buffer, '>');
    if (startIdx != -1 && endIdx != -1) {
        valueStr = stringSub(buffer, startIdx + 1, endIdx - 1);
        value = atoi(valueStr);
        for (fac = FACILITYNAMES; fac->name && !(fac->val == LOG_FAC(value) << 3); fac++)
        ;
        for (pri = PRIORITYNAMES; pri->name && !(pri->val == LOG_PRI(value)); pri++)
        ;
        if (fac && fac->name)
            (*logLine)->fac = stringNew(fac->name);
        if (pri && pri->name)
            (*logLine)->pri = stringNew(pri->name);
    }

    objectRelease(&valueStr);
    return rv;
}

static void
setHostName(LogLine **logLine)
{
    char hostName[1000] = {0};

    assert(*logLine);

    gethostname(hostName, 1000);
    assert(hostName);
    (*logLine)->hostName = stringNew(hostName);
}

static void
writeLogLine(char *buffer, LogLine **logLine)
{
    char *line, *other;
    bool error, warn, kernel;
    int idx = 19;

    assert(*logLine);

    line = other = NULL;
    error = warn = kernel = false;

    /* Evaluating the priority */
    if (stringStartsWithStr((*logLine)->pri, "err"))
        error = true;
    else if (stringStartsWithStr((*logLine)->pri, "warn") || stringStartsWithStr((*logLine)->pri, "crit") ||
             stringStartsWithStr((*logLine)->pri, "alert") || stringStartsWithStr((*logLine)->pri, "emerg"))
        warn = true;

    /* Building the log line */
    if (error)
        line = stringNew(RED_COLOR);
    else if (warn)
        line = stringNew(YELLOW_COLOR);

    if (!line)
        line = stringNew((*logLine)->timeStamp);
    else
        stringConcat(&line, (*logLine)->timeStamp);

    stringConcat(&line, " ");
    stringConcat(&line, (*logLine)->hostName);

    /* Evaluating Kernel facility */
    if (stringStartsWithStr((*logLine)->fac, "kern")) {
        kernel = true;
        stringConcat(&line, " kernel:");
        idx = stringIndexOfChr(buffer, ']') + 1;
    }

    /* Buffer remaining */
    other = stringSub(buffer, idx, strlen(buffer) - 1);
    assert(other);
    stringLtrim(other, NULL);
    stringConcat(&line, " ");
    stringConcat(&line, other);

    /* Reset color */
    if (error || warn)
        stringConcat(&line, DEFAULT_COLOR);

    /* Adding new line */
    if (!stringEndsWithStr(buffer, "\n"))
        stringConcat(&line, NEW_LINE);

    if (UNITLOGD_DEBUG)
        logInfo(CONSOLE, "Log line: \n%s\n", line);

    /* Open file and write the log line */
    unitlogdOpenLog("a");
    assert(UNITLOGD_LOG_FILE);
    logEntry(&UNITLOGD_LOG_FILE, line);

    /* Release resources */
    unitlogdCloseLog();
    assert(!UNITLOGD_LOG_FILE);
    objectRelease(&other);
    objectRelease(&line);
}

int
processLine(char *buffer)
{
    int rv, value;
    LogLine *logLine = logLineNew();

    rv = value = 0;

    assert(buffer);

    /* Set facility and priority */
    setFacAndPri(buffer, &logLine);
    /* Set host name */
    setHostName(&logLine);
    /* Set time stamp */
    logLine->timeStamp = stringGetTimeStamp(NULL, false, "%d %b %Y %H:%M:%S");

    if (UNITLOGD_DEBUG) {
        logInfo(CONSOLE, "\nFacility = %s\n", logLine->fac);
        logInfo(CONSOLE, "Priority = %s\n", logLine->pri);
        logInfo(CONSOLE, "hostName = %s\n", logLine->hostName);
        logInfo(CONSOLE, "Time stamp = %s\n", logLine->timeStamp);
    }

    /* Write the line into log */
    writeLogLine(buffer, &logLine);

    /* Release resources */
    logLineRelease(&logLine);

    return rv;
}
