/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

extern int UPATH_SECTIONS_ITEMS_LEN;
extern SectionData UPATH_SECTIONS_ITEMS[];
extern int UPATH_PROPERTIES_ITEMS_LEN;
extern PropertyData UPATH_PROPERTIES_ITEMS[];

int parsePathUnit(Array **, Unit **, bool);
int checkWellFormedPath(Unit **, const char *, int);
int checkWatchers(Unit **, bool);
void addWatchers(Unit **);
