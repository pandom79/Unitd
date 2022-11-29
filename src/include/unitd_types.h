/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITD_TYPES_H
#define UNITD_TYPES_H
#include <wrapper/wrapper.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <stdarg.h>
#include <pthread.h>
#include <glob.h>
#include <signal.h>
#include <syslog.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/reboot.h>
#include <utmp.h>
#include <sys/utsname.h>
#include <pwd.h>

#define UNUSED __attribute__((unused))

/* Process */
typedef enum {
    DEAD = 0,
    EXITED = 1,
    KILLED = 2,
    RUNNING = 3,
    RESTARTING = 4
} PState;

typedef struct PStateData {
    PState pState;
    const char *desc;
} PStateData;

static const struct PStateData PSTATE_DATA_ITEMS[] = {
    { DEAD, "Dead" },
    { EXITED, "Exited" },
    { KILLED, "Killed" },
    { RUNNING, "Running" },
    { RESTARTING, "Restarting" }
};

typedef enum {
    NO_PROCESS_TYPE = -1,
    DAEMON = 0,
    ONESHOT = 1,
    TIMER = 2,
} PType;

typedef struct PTypeData {
    PType pType;
    const char *desc;
} PTypeData;

static const struct PTypeData PTYPE_DATA_ITEMS[] = {
    { DAEMON, "daemon" },
    { ONESHOT, "oneshot" },
    { TIMER, "timer" },
};

typedef enum {
    FINAL_STATUS_READY = -1,
    FINAL_STATUS_SUCCESS = 0,
    FINAL_STATUS_FAILURE = 1
} FinalStatusEnum;

typedef struct {
    pid_t *pid;
    int *exitCode;
    PStateData *pStateData;
    int *signalNum;
    int *finalStatus;
    char *dateTimeStartStr;
    char *dateTimeStopStr;
    Time *timeStart;
    Time *timeStop;
    char *duration;
} ProcessData;

/* States */
static const int STATE_DATA_ITEMS_LEN = 10;

typedef enum {
    NO_STATE = -1,
    INIT = 0,
    POWEROFF = 1,
    SINGLE_USER = 2,
    MULTI_USER = 3,
    MULTI_USER_NET = 4,
    CUSTOM = 5,
    GRAPHICAL = 6,
    REBOOT = 7,
    FINAL = 8,
    USER = 9
} State;

typedef struct StateData {
    State state;
    const char *desc;
} StateData;

static const struct StateData STATE_DATA_ITEMS[] = {
    { INIT, "init" },
    { POWEROFF, "poweroff" },
    { SINGLE_USER, "single-user" },
    { MULTI_USER, "multi-user" },
    { MULTI_USER_NET, "multi-user-net" },
    { CUSTOM, "custom" },
    { GRAPHICAL, "graphical" },
    { REBOOT, "reboot" },
    { FINAL, "final" },
    { USER, "user" }
};

/* Units */
typedef struct {
    int fds[2];
    pthread_mutex_t *mutex;
} Pipe;

typedef struct {
    char *desc;
    Array *requires;
    PType type;
    bool restart;
    int restartMax;
    Array *conflicts;
    char *runCmd;
    char *stopCmd;
    Array *wantedBy;
    int restartNum;
    char *name;
    char *path;
    bool enabled;
    Array *errors;
    pthread_cond_t *cv;
    pthread_mutex_t *mutex;
    Array *processDataHistory;
    ProcessData *processData;
    Pipe *pipe;
    bool showResult;
    bool isStopping;
    bool isChanged;
    char *timerName;
    PState *timerPState;
    // Timer
    int *intSeconds;
    int *intMinutes;
    int *intHours;
    int *intDays;
    int *intWeeks;
    int *intMonths;
    char *intervalStr;
    long *leftTime;
    char *leftTimeDuration;
    Time *nextTime;
    char *nextTimeDate;
    Pipe *timerPipe;
} Unit;

typedef struct {
    Array *bootUnits;
    Array *initUnits;
    Array *units;
    Array *shutDownUnits;
    Array *finalUnits;
} UnitdData;

/* Commands */
typedef enum {
    NO_COMMAND = -1,
    REBOOT_COMMAND = 0,
    POWEROFF_COMMAND = 1,
    HALT_COMMAND = 2,
    LIST_COMMAND = 3,
    STATUS_COMMAND = 4,
    STOP_COMMAND = 5,
    START_COMMAND = 6,
    RESTART_COMMAND = 7,
    DISABLE_COMMAND = 8,
    ENABLE_COMMAND = 9,
    LIST_REQUIRES_COMMAND = 10,
    LIST_CONFLICTS_COMMAND = 11,
    LIST_STATES_COMMAND = 12,
    GET_DEFAULT_STATE_COMMAND = 13,
    SET_DEFAULT_STATE_COMMAND = 14,
    KEXEC_COMMAND = 15,
    CAT_COMMAND = 16,
    EDIT_COMMAND = 17,
    ANALYZE_COMMAND = 18,
    RE_ENABLE_COMMAND = 19,
    CREATE_COMMAND = 20,
    LIST_ENABLED_COMMAND = 21,
    LIST_DISABLED_COMMAND = 22,
    LIST_STARTED_COMMAND = 23,
    LIST_DEAD_COMMAND = 24,
    LIST_FAILED_COMMAND = 25,
    LIST_RESTARTABLE_COMMAND = 26,
    LIST_RESTARTED_COMMAND = 27,
    LIST_TIMERS_COMMAND = 28
} Command;

/* Socket */
typedef struct {
    Array *unitsDisplay;
    Array *errors;
    Array *messages;
} SockMessageOut;

/* List filter */
typedef enum {
    NO_FILTER = -1,
    ENABLED_FILTER = 0,
    DISABLED_FILTER = 1,
    STARTED_FILTER = 2,
    DEAD_FILTER = 3,
    FAILED_FILTER = 4,
    RESTARTABLE_FILTER = 5,
    RESTARTED_FILTER = 6,
    TIMERS_FILTER = 7
} ListFilter ;

#endif //UNITD_TYPES_H
