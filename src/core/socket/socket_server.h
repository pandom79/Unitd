/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

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
