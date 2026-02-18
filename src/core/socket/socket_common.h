/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define SOCKET_PATH "/run/unitd.sock"
#define MAX_CLIENT_SUPPORTED 32
#define BACK_LOG 20
#define INITIAL_SIZE 512
#define TOKEN "|"
#define ASSIGNER "="
#define NONE "none"
#define SYML_REMOVE_OP "remove"
#define SYML_ADD_OP "add"

typedef struct {
    char *arg;
    Command command;
    Array *options;
} SockMessageIn;

typedef struct {
    int key;
    const char *value;
} key_value;

Command getCommand(const char *command);
int initSocket(struct sockaddr_un *);
int unitdSockConn(int *, struct sockaddr_un *);
int readMessage(int *, char **, int *);
SockMessageIn *sockMessageInNew();
void sockMessageInRelease(SockMessageIn **);
SockMessageOut *sockMessageOutNew();
int sortUnitsByName(const void *, const void *);
void setValueForBuffer(char **, int);
Array *getScriptParams(const char *, const char *, const char *, const char *);
int sendWallMsg(Command);
void fillUnitsDisplayList(Array **, Array **, ListFilter);
int loadAndCheckUnit(Array **, bool, const char *, bool, Array **);
ListFilter getListFilterByCommand(Command);
ListFilter getListFilterByOpt(Array *);
void applyListFilter(Array **, ListFilter);
