/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define EUIRUN 114

typedef enum {
    NO_FUNC = -1,
    PARSE_UNIT = 0,
    PARSE_TIMER_UNIT = 1,
    PARSE_PATH_UNIT = 2,
    PARSE_SOCK_RESPONSE_UNITLIST = 3,
    PARSE_SOCK_RESPONSE = 4
} ParserFuncType;

typedef struct {
    pthread_mutex_t *mutex;
    bool lock;
} MutexThreadData;

int readSymLink(const char *, char **);
void addEnvVar(Array **, const char *, const char *);
State getStateByStr(char *);
int getDefaultStateStr(char **);
int setNewDefaultStateSyml(State, Array **, Array **);
void arrayPrint(int options, Array **, bool);
bool isKexecLoaded();
int writeWtmp(bool);
int unitdUserCheck(const char *, const char *);
int parseProcCmdLine();
int setSigAction();
int setUserData(int, struct passwd **);
void userDataRelease();
int handleMutex(pthread_mutex_t *, bool);
void *handleMutexThread(void *);
void setStopAndDuration(ProcessData **);
int getMaxFileDesc(int *, int *);
char *getUnitNameByOther(const char *, PType);
char *getOtherNameByUnitName(const char *, PType);
void showVersion();
int uRead(int, void *, size_t);
int uWrite(int, void *, size_t);
ssize_t uSend(int, const void *, size_t, int);
ssize_t uRecv(int, void *, size_t, int);
