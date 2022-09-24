/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITLOGD_IMPL_H
#define UNITLOGD_IMPL_H

#include "common/common.h"
#include "signals/signals.h"
#include "socket/socket.h"

extern bool UNITLOGD_DEBUG;
extern int SELF_PIPE[2];
extern int UNITLOGD_PID;
extern FILE *UNITLOGD_INDEX_FILE;
extern FILE *UNITLOGD_LOG_FILE;
extern FILE *UNITLOGD_BOOT_LOG_FILE;

#endif // UNITLOGD_IMPL_H
