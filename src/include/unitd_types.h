/**
 * @brief Unitd types
 * @file unitd_types.h
 * @author Domenico Panella <pandom79@gmail.com>
 *
*/

#ifndef UNITD_TYPES_H
#define UNITD_TYPES_H
#include <ulib/ulib.h>
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
#include <fnmatch.h>

/**
 * This enumerator represents the process state.<br>
 */
typedef enum {
    DEAD = 0,
    EXITED = 1,
    KILLED = 2,
    RUNNING = 3,
    RESTARTING = 4
} PState;

/**
 * @struct PStateData
 * @brief This structure contains the possible values of the process state.
 * @var PStateData::pState
 * Represents the state of a process.
 * @var PStateData::desc
 * Represents the description of a process.
 */
typedef struct {
    PState pState;
    const char *desc;
} PStateData;

static const PStateData PSTATE_DATA_ITEMS[] = {
    { DEAD, "Dead" },
    { EXITED, "Exited" },
    { KILLED, "Killed" },
    { RUNNING, "Running" },
    { RESTARTING, "Restarting" }
};

/**
 * This enumerator represents the process type.<br>
 */
typedef enum {
    NO_PROCESS_TYPE = -1,
    DAEMON = 0,
    ONESHOT = 1,
    TIMER = 2,
    UPATH = 3
} PType;

/**
 * @struct PTypeData
 * @brief This structure contains the data of a process type.
 * @var PTypeData::pType
 * Represents the type of a process.
 * @var PTypeData::desc
 * Represents the description of a process type.
 *
*/
typedef struct {
    PType pType;
    const char *desc;
} PTypeData;

static const PTypeData PTYPE_DATA_ITEMS[] = {
    { DAEMON, "daemon" },
    { ONESHOT, "oneshot" },
    { TIMER, "timer" },
    { UPATH, "path" },
};

/**
 * @struct ProcessData
 * @brief This structure contains the data of a process.
 * @var ProcessData::pid
 * Represents the pid of the process.
 * @var ProcessData::exitCode
 * Represents the exit code of the process.
 * @var ProcessData::pStateData
 * Represents the data of the process state.
 * @var ProcessData::signalNum
 * Represents the signal number received by process.
 * @var ProcessData::finalStatus
 * Represents the final status of the process.
 * @var ProcessData::dateTimeStartStr
 * Represents the start timestamp of the process as string.
 * @var ProcessData::dateTimeStopStr
 * Represents the stop timestamp of the process as string.
 * @var ProcessData::timeStart
 * Represents the start timestamp of the process.
 * @var ProcessData::timeStop
 * Represents the stop timestamp of the process.
 * @var ProcessData::duration
 * Represents the duration of the process as string.
 *
*/
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

/**
 * This enumerator represents the possible states.<br>
 */
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

/**
 * @struct StateData
 * @brief This structure contains the data of the process state.
 * @var StateData::state
 * Represents the state of the process.
 * @var StateData::desc
 * Represents the process state description.
 *
*/
typedef struct {
    State state;
    const char *desc;
} StateData;

static const StateData STATE_DATA_ITEMS[] = {
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

/**
 * @struct Pipe
 * @brief This structure contains the data to handle a pipe.
 * @var Pipe::fds[]
 * Array of file descriptors.
 * @var Pipe::mutex
 * Represents the pipe's mutex.
 *
*/
typedef struct {
    int fds[2];
    pthread_mutex_t *mutex;
} Pipe;

/**
 * @struct Timer
 * @brief This structure contains the data to handle a timer.
 * @var Timer::timerId
 * Represents the timer id.
 * @var Timer::eventData
 * Represents the event data.
 * @var Timer::sev
 * Represents the signal event structure.
 * @var Timer::its
 * Represents the itimerspec structure.
 *
*/
typedef struct {
    timer_t *timerId;
    struct eventData *eventData;
    struct sigevent *sev;
    struct itimerspec *its;
} Timer;

/**
 * @struct Notifier
 * @brief This structure represents the Notifier.
 * @var Notifier::pipe
 * Represents the notifier's pipe.
 * @var Notifier::fd
 * Represents the file descriptor to monitor.
 * @var Notifier::watchers
 * This structure contains all watchers.
 */
typedef struct {
    Pipe *pipe;
    int *fd;
    Array *watchers;
} Notifier;

/**
 * @struct Unit
 * @brief This structure represents the unit.
 * @var Unit::desc
 * Represents the unit description.
 * @var Unit::requires
 * Represents the unit dependencies.
 * @var Unit::type
 * Represents the process type of the unit.
 * @var Unit::restart
 * Set restart mode for an unit.
 * @var Unit::restartMax
 * Set the maximum restart number for an unit.
 * @var Unit::conflicts
 * Represents the unit conflicts.
 * @var Unit::runCmd
 * Represents the command to start an unit.
 * @var Unit::stopCmd
 * Represents the command to stop an unit.
 * @var Unit::failureCmd
 * Represents the command to run when an unit fails.
 * @var Unit::wantedBy
 * Represents the wanted states of the unit.
 * @var Unit::restartNum
 * Represents the restart number of the unit.
 * @var Unit::name
 * Represents the unit name.
 * @var Unit::path
 * Represents the unit path.
 * @var Unit::enabled
 * Boolean value which allows to enable/disable an unit for a state.
 * @var Unit::errors
 * Contains the unit errors.
 * @var Unit::cv
 * The condition variable of the unit.
 * @var Unit::mutex
 * The mutex of the unit.
 * @var Unit::processDataHistory
 * Contains the history of the process data.
 * @var Unit::processData
 * Contains the process data.
 * @var Unit::pipe
 * Represents the unit pipe.
 * @var Unit::showResult
 * Boolean value which allows to show the data on the console.
 * @var Unit::isStopping
 * Boolean value which allows to understand if a process is stopping.
 * @var Unit::isChanged
 * Boolean value which allows to understand if the unit content is changed.
 * @var Unit::timerName
 * The timer unit name.
 * @var Unit::timerPState
 * The process state of a timer unit.
 * @var Unit::failurePid
 * Represents the pid of the failure command.
 * @var Unit::failureExitCode
 * Represents the pid of the failure command.
 * @var Unit::pathUnitName
 * The path unit name.
 * @var Unit::pathUnitPState
 * The process state of te path unit.
 * @var Unit::timer
 * Represents the timer of the unit.
 * @var Unit::wakeSystem
 * Set the wake mode for a timer.
 * @var Unit::intSeconds
 * Set the elapsed time in seconds.
 * @var Unit::intMinutes
 * Set the elapsed time in minutes.
 * @var Unit::intHours
 * Set the elapsed time in hours.
 * @var Unit::intDays
 * Set the elapsed time in days.
 * @var Unit::intWeeks
 * Set the elapsed time in weeks.
 * @var Unit::intMonths
 * Set the elapsed time in months.
 * @var Unit::intervalStr
 * Set the interval as string.
 * @var Unit::leftTime
 * Represents the left time.
 * @var Unit::leftTimeDuration
 * Represents the left time as string.
 * @var Unit::nextTime
 * Represents the the next time.
 * @var Unit::nextTimeDate
 * Represents the next time as string.
 * @var Unit::notifier
 * Represents the notifier struct.
 * @var Unit::pathExists
 * Contains the path which must be checked the existence.
 * @var Unit::pathExistsMonitor
 * Contains the real path defined in "pathExists".
 * @var Unit::pathExistsGlob
 * Contains the glob pattern.
 * @var Unit::pathExistsGlobMonitor
 * Contains the real path to monitor defined in "pathExistsGlob".
 * @var Unit::pathResourceChanged
 * Contains the resource path that can be changed.
 * @var Unit::pathResourceChangedMonitor
 * Contains the real resource path defined in "pathResourceChanged".
 * @var Unit::pathDirectoryNotEmpty
 * Contains the folder path must be checked.
 * @var Unit::pathDirectoryNotEmptyMonitor
 * Contains the real folder defined in "pathDirectoryNotEmpty".
 */
typedef struct {
    char *desc;
    Array *requires;
    PType type;
    bool restart;
    int restartMax;
    Array *conflicts;
    char *runCmd;
    char *stopCmd;
    char *failureCmd;
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
    pid_t *failurePid;
    int *failureExitCode;
    char *pathUnitName;
    PState *pathUnitPState;
    // Timer
    Timer *timer;
    bool *wakeSystem;
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
    // Path Unit
    Notifier *notifier;
    char *pathExists;
    char *pathExistsMonitor;
    char *pathExistsGlob;
    char *pathExistsGlobMonitor;
    char *pathResourceChanged;
    char *pathResourceChangedMonitor;
    char *pathDirectoryNotEmpty;
    char *pathDirectoryNotEmptyMonitor;
} Unit;

/**
 * @struct UnitdData
 * @brief This structure contains all data.
 * @var UnitdData::bootUnits
 * This structure contains the boot units.
 * @var UnitdData::initUnits
 * This structure contains the initialization units.
 * @var UnitdData::units
 * This structure contains the units for the default or command line state.
 * @var UnitdData::shutDownUnits
 * This structure contains the units for the poweroff/reboot state.
 * @var UnitdData::finalUnits
 * This structure contains the finalization units.
*/
typedef struct {
    Array *bootUnits;
    Array *initUnits;
    Array *units;
    Array *shutDownUnits;
    Array *finalUnits;
} UnitdData;

/**
 * @brief This enumerator represents the commands received by unit daemon.
 *
 */
typedef enum {
    /** No command */
    NO_COMMAND = -1,
    /** Reboot the system.<br>
     *  Available for system instance.
    */
    REBOOT_COMMAND = 0,
    /** Poweroff the unitd instance. */
    POWEROFF_COMMAND = 1,
    /** Halt the system.<br>
     *  Available for system instance.
    */
    HALT_COMMAND = 2,
    /** List the units. */
    LIST_COMMAND = 3,
    /** Show the unit status. */
    STATUS_COMMAND = 4,
    /** Stop the unit. */
    STOP_COMMAND = 5,
    /** Start the unit. */
    START_COMMAND = 6,
    /** Restart the unit. */
    RESTART_COMMAND = 7,
    /** Disable the unit. */
    DISABLE_COMMAND = 8,
    /** Enable the unit. */
    ENABLE_COMMAND = 9,
    /** List the dependencies of the unit. */
    LIST_REQUIRES_COMMAND = 10,
    /** List the conflicts of the unit. */
    LIST_CONFLICTS_COMMAND = 11,
    /** List the wanted states of the unit. */
    LIST_STATES_COMMAND = 12,
    /** Show the default state.<br>
     *  Available for system instance.
    */
    GET_DEFAULT_STATE_COMMAND = 13,
    /** Set the default state.<br>
     *  Available for system instance.
    */
    SET_DEFAULT_STATE_COMMAND = 14,
    /** Reboot the system with kexec.<br>
     *  Available for system instance.
    */
    KEXEC_COMMAND = 15,
    /** Show the unit content.
    */
    CAT_COMMAND = 16,
    /** Edit the unit content.
    */
    EDIT_COMMAND = 17,
    /** Analyze the boot process of the unitd instance.
    */
    ANALYZE_COMMAND = 18,
    /** Re-enable the unit.
    */
    RE_ENABLE_COMMAND = 19,
    /** Create the unit.
    */
    CREATE_COMMAND = 20,
    /** Show the enabled units.
    */
    LIST_ENABLED_COMMAND = 21,
    /** Show the disabled units.
    */
    LIST_DISABLED_COMMAND = 22,
    /** Show the started units.
    */
    LIST_STARTED_COMMAND = 23,
    /** Show the dead units.
    */
    LIST_DEAD_COMMAND = 24,
    /** Show the failed units.
    */
    LIST_FAILED_COMMAND = 25,
    /** Show the restartable units.
    */
    LIST_RESTARTABLE_COMMAND = 26,
    /** Show the restarted units.
    */
    LIST_RESTARTED_COMMAND = 27,
    /** Show the timer units.
    */
    LIST_TIMERS_COMMAND = 28,
    /** Show the path units.
    */
    LIST_UPATH_COMMAND = 29
} Command;

/**
 *  @struct SockMessageOut
 *  @brief This structure the response message from unix socket.
 *  @var SockMessageOut::unitsDisplay
 *  This structure contains the received units.
 *  @var SockMessageOut::errors
 *  This structure contains the received errors.
 *  @var SockMessageOut::messages
 *  This structure contains the messages errors.
 */
typedef struct {
    Array *unitsDisplay;
    Array *errors;
    Array *messages;
} SockMessageOut;

/**
 * @brief This enumerator represents the filter to apply to units list.
 *
 */
typedef enum {
    /** No filter */
    NO_FILTER = -1,
    /** List the enabled units. */
    ENABLED_FILTER = 0,
    /** List the disabled units. */
    DISABLED_FILTER = 1,
    /** List the started units. */
    STARTED_FILTER = 2,
    /** List the dead units. */
    DEAD_FILTER = 3,
    /** List the failed units. */
    FAILED_FILTER = 4,
    /** List the restartable units. */
    RESTARTABLE_FILTER = 5,
    /** List the restarted units. */
    RESTARTED_FILTER = 6,
    /** List the timer units. */
    TIMERS_FILTER = 7,
    /** List the path units. */
    UPATH_FILTER = 8
} ListFilter ;

#endif //UNITD_TYPES_H
