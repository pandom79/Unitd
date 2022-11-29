/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

int showUnitList(SockMessageOut **, ListFilter);
int showTimersList(SockMessageOut **, ListFilter);
int showUnitStatus(SockMessageOut **, const char *);
int showData(Command, SockMessageOut **, const char *, bool, bool, bool, bool);
int catEditUnit(Command, const char *);
int createUnit(const char *);
int showBootAnalyze(SockMessageOut **);
int checkAdministrator(char **);
