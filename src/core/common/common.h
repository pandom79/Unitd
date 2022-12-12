/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

typedef struct {
    pthread_mutex_t *mutex;
    bool lock;
} MutexThreadData;

int readSymLink(const char *, char **);
int msleep(long);
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
void* handleMutexThread(void *);
void setStopAndDuration(ProcessData **);
int getMaxFileDesc(int *, int *);
char* getUnitNameByOther(const char *, PType);
char* getTimerNameByUnit(const char *);
void showVersion();
