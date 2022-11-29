/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITD_IMPL_H
#define UNITD_IMPL_H

#include "../include/unitd.h"
#include "handlers/signals.h"
#include "handlers/notifier.h"
#include "handlers/cleaner.h"
#include "parser/parser.h"
#include "socket/socket_client.h"
#include "socket/socket_common.h"
#include "socket/socket_server.h"
#include "socket/socket_request.h"
#include "socket/socket_response.h"
#include "common/common.h"
#include "init/init.h"
#include "processes/process.h"
#include "commands/commands.h"
#include "units/units.h"
#include "units/utimers/utimers.h"
#include "file/file.h"
#include "logger/logger.h"

#define PROJECT_NAME                "Unitd init system"
#define PROJECT_USER_NAME           "Unitd user instance"
#define DEF_STATE_SYML_NAME         "default.state"

#ifndef UNITD_TEST
#define PROC_CMDLINE_PATH           "/proc/cmdline"
#else
#define PROC_CMDLINE_PATH           "/home/domenico/Scrivania/cmdline.txt"
#endif

#define PROC_CMDLINE_UNITD_DEBUG    "unitd_debug=true"
#define PATH_ENV_VAR                "/usr/bin:/usr/sbin:/bin:/sbin"


extern pid_t UNITD_PID;
extern UnitdData *UNITD_DATA;
extern bool UNITD_DEBUG;
extern bool UNITCTL_DEBUG;
extern Command SHUTDOWN_COMMAND;
extern bool NO_WTMP;
extern Array *UNITD_ENV_VARS;
extern State STATE_DEFAULT;
extern State STATE_NEW_DEFAULT;
extern State STATE_CMDLINE;
extern State STATE_SHUTDOWN;
extern char *STATE_CMDLINE_DIR;
extern bool LISTEN_SOCK_REQUEST;
extern bool ENABLE_RESTART;
extern Time *BOOT_START;
extern Time *BOOT_STOP;
extern Time *SHUTDOWN_START;
extern Time *SHUTDOWN_STOP;
extern char *UNITS_USER_LOCAL_PATH;
extern char *UNITS_USER_ENAB_PATH;
extern char *UNITD_USER_CONF_PATH;
extern char *UNITD_USER_TIMER_DATA_PATH;
extern char *UNITD_USER_LOG_PATH;
extern bool USER_INSTANCE;
extern State STATE_USER;
extern char *STATE_USER_DIR;
extern char *SOCKET_USER_PATH;

/* Errors */
typedef enum {
    UNITD_GENERIC_ERR = 0,
} UnitdErrorsEnum;
typedef struct {
    UnitdErrorsEnum errorEnum;
    const char *desc;
} UnitdErrorsData;
extern const UnitdErrorsData UNITD_ERRORS_ITEMS[];

/* Messages */
typedef enum {
    UNITD_SYSTEM_LOG_MSG = 0,
} UnitdMessagesEnum;
typedef struct {
    UnitdMessagesEnum errorEnum;
    const char *desc;
} UnitdMessagesData;
extern const UnitdMessagesData UNITD_MESSAGES_ITEMS[];
/*********************************************************************************/

/* UNITCTL commands */
typedef enum {
    NO_OPTION = -1,
    FORCE_OPT = 0,
    RESTART_OPT = 1,
    RUN_OPT = 2,
    REQUIRES_OPT = 3,
    CONFLICTS_OPT = 4,
    STATES_OPT = 5,
    NO_WTMP_OPT = 6,
    ANALYZE_OPT = 7,
    RE_ENABLE_OPT = 8,
    RESET_OPT = 9
} Option;

typedef struct OptionData {
    Option option;
    const char *name;
} OptionData;
extern OptionData OPTIONS_DATA[];
extern int OPTIONS_LEN;

typedef struct CommandData {
    Command command;
    const char *name;
} CommandData;
extern CommandData COMMANDS_DATA[];
extern int COMMANDS_LEN;
/*********************************************************************************/

#endif // UNITD_IMPL_H
