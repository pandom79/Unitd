/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITLOGD_IMPL_H
#define UNITLOGD_IMPL_H

#include "../core/unitd_impl.h"
#include "init/init.h"
#include "signals/signals.h"
#include "socket/socket.h"
#include "index/index.h"
#include "file/file.h"
#include "logline/logline.h"
#include "client/client.h"

#define BOOT_ID_SIZE 20
#define BOOT_ID_CHARSET "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define ENTRY_STARTED "Started"
#define ENTRY_FINISHED "Finished"
#define TOKEN_ENTRY " | "
#define NEW_LINE "\n"

extern bool DEBUG;
extern int SELF_PIPE[2];
extern int UNITLOGD_PID;
extern FILE *UNITLOGD_INDEX_FILE;
extern FILE *UNITLOGD_LOG_FILE;
extern FILE *UNITLOGD_KMSG_FILE;
extern char *BOOT_ID_STR;
extern bool UNITLOGD_EXIT;

#endif // UNITLOGD_IMPL_H
