#ifndef UNITD_IMPL_H
#define UNITD_IMPL_H

#include "../include/unitd.h"

/* UNITD */
#define PROJECT_NAME                "Unitd init system"
#define DEF_STATE_SYML_NAME         "default.state"

#ifndef UNITD_TEST
#define PROC_CMDLINE_PATH           "/proc/cmdline"
#else
#define PROC_CMDLINE_PATH           "/home/domenico/Scrivania/cmdline.txt"
#endif

#define PROC_CMDLINE_UNITD_DEBUG    "unitd_debug=true"
#define PATH_ENV_VAR                "/usr/bin:/usr/sbin:/bin:/sbin"

typedef struct {
    int fds[2];
    pthread_mutex_t *mutex;
} Cleaner;

typedef struct {
    int fds[2];
    int *fd;
    int *wd;
    pthread_mutex_t *mutex;
    bool *isWorking;
} Notifier;

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
extern Cleaner *CLEANER;
extern Notifier *NOTIFIER;
int parseProcCmdLine();

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
    RE_ENABLE_OPT = 8
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

/* LOGGER */
#define RED_COLOR "\033[1;31m"
#define YELLOW_COLOR "\033[1;33m"
#define GREEN_COLOR "\033[1;32m"
#define LIGHT_WHITE_COLOR "\033[0;37m"
#define WHITE_COLOR "\033[1;37m"
#define GREY_COLOR "\033[30;1m"
#define DEFAULT_COLOR "\033[0m"
#define WHITE_UNDERLINE_COLOR "\033[37;4m"

#define LOG_UNITD_CONSOLE   0x0001
#define LOG_UNITD_BOOT      0x0010
#define LOG_UNITD_ALL       0x1111

extern FILE *UNITD_LOG_FILE;

int unitdOpenLog(const char *);
int unitdCloseLog();
void unitdLogInfo(int options, const char *format, ...);
void unitdLogWarning(int options, const char *format, ...);
void unitdLogError(int options, const char *transUnit, const char *funcName, int returnValue,
                   const char *errDesc, const char *format, ...);
void unitdLogErrorStr(int options, const char *format, ...);
void unitdLogSuccess(int options, const char *format, ...);
/*********************************************************************************/

/* PARSER */
typedef enum {
    NO_FUNC = -1,
    PARSE_UNIT = 0,
    PARSE_SOCK_REQUEST = 1,
    PARSE_SOCK_RESPONSE_UNITLIST = 2,
    PARSE_SOCK_RESPONSE = 3
} ParserFuncType;

/* STATIC DATA */
#define NO_SECTION -1

/* Generic Errors */
typedef enum {
    FIRST_CHARACTER_ERR = 0,
    UNRECOGNIZED_ERR = 1,
    OCCURRENCES_ERR = 2,
    PROPERTY_SECTION_ERR = 3,
    ACCEPTED_VALUE_ERR = 4,
    DUPLICATE_VALUE_ERR = 5,
    REQUIRED_VALUE_ERR = 6,
    NUMERIC_ERR = 7,
    OCCURRENCES_EQUAL_ERR = 8,
} ErrorsEnum;

typedef struct {
    ErrorsEnum errorEnum;
    const char *desc;
} ErrorsData;

/* DYNAMIC DATA which will be built from translation
 * unit that will include this header */

/* Sections */
typedef struct SectionName {
    int sectionNameEnum;
    const char *desc;
} SectionName;

typedef struct SectionData {
    SectionName sectionName;
    bool repeatable;
    bool required;
    int sectionCount;
} SectionData;

extern int UNITS_SECTIONS_ITEMS_LEN;
extern SectionData UNITS_SECTIONS_ITEMS[];

/* Properties */
typedef struct PropertyName {
    int propertyNameEnum;
    const char *desc;
} PropertyName;

typedef struct PropertyData {
    int idxSectionItem;
    PropertyName propertyName;
    bool repeatable;
    bool required;
    bool numeric;
    int propertyCount;
    const char **acceptedValues;
    Array *notDupValues;
} PropertyData;

extern int UNITS_PROPERTIES_ITEMS_LEN;
extern PropertyData UNITS_PROPERTIES_ITEMS[];

/* Functions */
void parserInit(ParserFuncType funcType);
int parseLine(char *, int, Array **keyVal, PropertyData **);
char* checkKeyVal(char *key, char *value, int numLine, PropertyData **);
bool isValidNumber(const char *);
char* getMsg(int numLine, const char *message, ...);
void parserEnd(Array **, bool);
/*********************************************************************************/

/* UNITS */

/* Specific Errors */
typedef enum {
    UNABLE_OPEN_UNIT_ERR = 0,
    REQUIRE_ITSELF_ERR = 1,
    CONFLICT_ITSELF_ERR = 2,
    UNSATISFIED_DEP_ERR = 3,
    BIDIRECTIONAL_DEP_ERR = 4,
    DEPS_ERR = 5,
    CONFLICT_DEP_ERROR = 6,
    CONFLICT_EXEC_ERROR = 7,
    WANTEDBY_ERR = 8,
    WANTEDBY_INIT_FINAL_ERR = 9,
    UNIT_PATH_INIT_FINAL_ERR = 10,
    UNIT_PATH_ERR = 11,
    UNIT_NOT_EXIST_ERR = 12,
    UNIT_TIMEOUT_ERR = 13,
    UNIT_ALREADY_ERR = 14,
    DEFAULT_SYML_MISSING_ERR = 15,
    DEFAULT_SYML_TYPE_ERR = 16,
    DEFAULT_SYML_BAD_DEST_ERR = 17,
    DEFAULT_SYML_SET_ERR = 18,
    UNIT_CHANGED_ERR = 19,
    UNIT_ENABLE_STATE_ERR = 20
} UnitsErrorsEnum;
typedef struct {
    UnitsErrorsEnum errorEnum;
    const char *desc;
} UnitsErrorsData;
extern const UnitsErrorsData UNITS_ERRORS_ITEMS[];

/* Specific Messages */
typedef enum {
    UNIT_START_MSG = 0,
    UNIT_FORCE_START_CONFLICT_MSG = 1,
    UNIT_REMOVED_SYML_MSG = 2,
    UNIT_CREATED_SYML_MSG = 3,
    STATE_MSG = 4,
    DEFAULT_STATE_SYML_RESTORED_MSG = 5,
    TIME_MSG = 6,
    UNIT_CHANGED_MSG = 7,
    UNIT_ENABLE_STATE_MSG = 8,
    UNIT_CHANGED_RE_ENABLE_MSG = 9
} UnitsMessagesEnum;
typedef struct {
    UnitsMessagesEnum errorEnum;
    const char *desc;
} UnitsMessagesData;
extern const UnitsMessagesData UNITS_MESSAGES_ITEMS[];

/* Functions */
Unit* unitNew(Unit *, ParserFuncType);
void unitRelease(Unit **);
ProcessData* processDataNew(ProcessData *, ParserFuncType, bool);
void processDataRelease(ProcessData **);
int loadUnits(Array **, const char *, const char *, State,
              bool, const char *, ParserFuncType, bool);
int parseUnit(Array **units, Unit **, bool, State);
int checkConflicts(Unit **, const char *, bool);
int checkRequires(Array **, Unit **, bool);
int checkWantedBy(Unit **, State, bool);
int checkAndSetUnitPath(Unit **, State);
bool isEnabledUnit(const char *, State);
char* getUnitName(const char *);
Unit* getUnitByName(Array *, const char *);
Unit* getUnitByPid(Array *, pid_t pid);
PType getPTypeByPTypeStr(const char *);
Pipe* pipeNew();
void pipeRelease(Pipe **);
Cleaner* cleanerNew();
void cleanerRelease(Cleaner **);
Notifier* notifierNew();
void notifierRelease(Notifier **);
/*********************************************************************************/

/* COMMANDS */
#define TIMEOUT_INC_MS            250
//Start
#define TIMEOUT_MS              15000
#define MIN_TIMEOUT_MS           3500
//Stop
#define TIMEOUT_STOP_MS          1000

int execScript(const char *, const char *, char **, char **);
int execProcess(const char *, char **, Unit **);
char** cmdlineSplit(const char *);
void cmdlineRelease(char **);
int stopDaemon(const char *, char **, Unit **);
void reapPendingChild();
/*********************************************************************************/

/* PROCESSES */
#define SHOW_MAX_RESULTS        10
#define THREAD_EXIT             -1
typedef struct {
    pthread_t thread;
    Unit *unit;
    Array *units;
} UnitThreadData;

int startProcesses(Array **, Unit *);
void* startProcess(void *);
Array* getDaemonUnits(Array **);
int stopProcesses(Array **, Unit *);
void* stopProcess(void *);
void listenPipe(Unit *);
void* listenPipeThread(void *);
bool hasPipe(Unit *);
Array* getRestartableUnits(Array **);
int openPipes(Array **, Unit *);
void* openPipe(void *);
Array* getUnitsPipes(Array **);
int closePipes(Array **, Unit *);
void* closePipe(void *);
/*********************************************************************************/

/* INIT */
int unitdInit(UnitdData **, bool);
void unitdEnd(UnitdData **);
/*********************************************************************************/

/* HANDLERS */
#define CLEANER_TIMEOUT         10
//Signals
int signalsHandler(int, siginfo_t *, void *);
//Cleaner
void startCleaner();
void* startCleanerThread(void *);
void stopCleaner();
void* stopCleanerThread(void *);
//Notifier
void startNotifier();
void* startNotifierThread(void *);
void stopNotifier();
void* stopNotifierThread(void *);
/*********************************************************************************/

/* SOCKET */
#define SOCKET_NAME             "/run/unitd.sock"
#define MAX_CLIENT_SUPPORTED    32
#define BACK_LOG                20
#define INITIAL_SIZE            512
#define TOKEN                   "|"
#define ASSIGNER                "="
#define NONE                    "none"
#define SYML_REMOVE_OP          "remove"
#define SYML_ADD_OP             "add"

typedef struct {
    char *arg;
    Command command;
    Array *options;
} SockMessageIn;

/* Common */
Command getCommand(const char *command);
int initSocket(struct sockaddr_un *);
int unitdSockConn(int *, struct sockaddr_un *);
int readMessage(int *, char **, int *);
SockMessageIn* sockMessageInNew();
void sockMessageInRelease(SockMessageIn **);
SockMessageOut* sockMessageOutNew();
int sortUnitsByName(const void *, const void *);
void setValueForBuffer(char **, int);
Array* getScriptParams(const char *, const char *, const char *);
int sendWallMsg(Command);
int checkAdministrator(char **);
void fillUnitsDisplayList(Array **, Array **);

/* Server */
int listenSocketRequest();
int socketDispatchRequest(char *, int *);
int getUnitListServer(int *, SockMessageIn *, SockMessageOut **);
int getUnitStatusServer(int *, SockMessageIn *, SockMessageOut **);
int stopUnitServer(int *, SockMessageIn *, SockMessageOut **, bool);
int startUnitServer(int *, SockMessageIn *, SockMessageOut **, bool);
int disableUnitServer(int *, SockMessageIn *, SockMessageOut **, const char *, bool);
int enableUnitServer(int *, SockMessageIn *, SockMessageOut **);
int getUnitDataServer(int *, SockMessageIn *, SockMessageOut **);
int getDefaultStateServer(int *, SockMessageIn *, SockMessageOut **);
int setDefaultStateServer(int *, SockMessageIn *, SockMessageOut **);

/* Client */
int showUnitList(SockMessageOut **);
int showUnitStatus(SockMessageOut **, const char *unitName);
int showUnit(Command, SockMessageOut **, const char *, bool, bool, bool, bool);
int catEditUnit(Command, const char *unitName);
int showBootAnalyze(SockMessageOut **);

/* Request */
extern int SOCKREQ_PROPERTIES_ITEMS_LEN;
extern PropertyData SOCKREQ_PROPERTIES_ITEMS[];
char* marshallRequest(SockMessageIn *);
int unmarshallRequest(char *, SockMessageIn **);

/* Response */
extern int SOCKRES_SECTIONS_ITEMS_LEN;
extern SectionData SOCKRES_SECTIONS_ITEMS[];
extern int SOCKRES_PROPERTIES_ITEMS_LEN;
extern PropertyData SOCKRES_PROPERTIES_ITEMS[];
char* marshallResponse(SockMessageOut *, ParserFuncType);
int unmarshallResponse(char *, SockMessageOut **);
/*********************************************************************************/

/* COMMON */
int readSymLink(const char *, char **);
int msleep(long);
void addEnvVar(Array **, const char *, const char *);
State getStateByStr(char *);
int getDefaultStateStr(char **);
int setNewDefaultStateSyml(State, Array **);
void arrayPrint(int options, Array **, bool);
bool isKexecLoaded();
int writeWtmp(bool);
/*********************************************************************************/

#endif // UNITD_IMPL_H
