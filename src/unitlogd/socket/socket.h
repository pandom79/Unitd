/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#define BUFFER_SIZE 1024

SocketThread* socketThreadNew();
void socketThreadRelease(SocketThread **);
int startSockets(Array *);
void* startSocket(void *);
void* startUnixThread(void *);
int getSocketFd(struct sockaddr_un *, const char *);
int stopSockets(Array *);
void* stopSocket(void *);