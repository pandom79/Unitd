/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define RED_COLOR "\033[1;31m"
#define YELLOW_COLOR "\033[1;33m"
#define GREEN_COLOR "\033[1;32m"
#define LIGHT_WHITE_COLOR "\033[0;37m"
#define WHITE_COLOR "\033[1;37m"
#define GREY_COLOR "\033[30;1m"
#define DEFAULT_COLOR "\033[0m"
#define WHITE_UNDERLINE_COLOR "\033[37;4m"

#define CONSOLE             0x1
#define UNITD_BOOT_LOG      0x2
#define SYSTEM              0x4
#define UNITLOGD_BOOT_LOG   0x8
#define UNITLOGD_LOG        0x10
#define UNITLOGD_INDEX      0x20
#define ALL                 0x3F

extern FILE *UNITD_BOOT_LOG_FILE;

void logInfo(int options, const char *format, ...);
void logWarning(int options, const char *format, ...);
void logError(int options, const char *transUnit, const char *funcName, int returnValue,
                   const char *errDesc, const char *format, ...);
void logErrorStr(int options, const char *format, ...);
void logSuccess(int options, const char *format, ...);
