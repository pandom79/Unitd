/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define UNITD_DATA_PATH_CMD_VAR  "$UNITD_DATA_PATH"

extern int UNITS_SECTIONS_ITEMS_LEN;
extern SectionData UNITS_SECTIONS_ITEMS[];
extern int UNITS_PROPERTIES_ITEMS_LEN;
extern PropertyData UNITS_PROPERTIES_ITEMS[];

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
    UNIT_ENABLE_STATE_ERR = 20,
    UNITS_LIST_EMPTY_ERR = 21,
    UNIT_EXIST_ERR = 22,
    UTIMER_INTERVAL_ERR = 23,
    UPATH_WELL_FORMED_PATH_ERR = 24,
    UPATH_PATH_SEC_ERR = 25,
    UPATH_ACCESS_ERR = 26,
    UPATH_PATH_RESOURCE_ERR = 27
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
    UNIT_CHANGED_RE_ENABLE_MSG = 9,
    UNIT_RE_ENABLE_MSG = 10
} UnitsMessagesEnum;
typedef struct {
    UnitsMessagesEnum errorEnum;
    const char *desc;
} UnitsMessagesData;
extern const UnitsMessagesData UNITS_MESSAGES_ITEMS[];

/* List filter */
typedef struct {
    ListFilter listFilter;
    const char *desc;
} ListFilterData;
extern const ListFilterData LIST_FILTER_DATA[];
extern int LIST_FILTER_LEN;

/* Functions */
Unit* unitNew(Unit *, ParserFuncType);
void unitRelease(Unit **);
ProcessData* processDataNew(ProcessData *, ParserFuncType);
void resetPDataForRestart(ProcessData **);
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
Unit* getUnitByPid(Array *, pid_t);
Unit* getUnitByFailurePid(Array *, pid_t);
PType getPTypeByPTypeStr(const char *);
Pipe* pipeNew();
void pipeRelease(Pipe **);
PType getPTypeByUnitName(const char *);
int loadOtherUnits(Array **, const char *, const char *, bool, bool, ListFilter);
