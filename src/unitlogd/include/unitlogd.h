/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITLOGD_H
#define UNITLOGD_H

typedef struct {
    char *bootId;
    Time *start;
    char  *startOffset;
    Time *stop;
    char  *stopOffset;
} IndexEntry;

/* Return the list of the boots.
 * Each element is an 'IndexEntry' struct.
 * The array must be freed via 'arrayRelease' function.
 * Return value:
 * On success returns '0' otherwise an error has occurred.
*/
int getBootsList(Array **bootsList);

/* Repair the index reading the entries from log.
 * This function is useful in the corruption case of the index file.
 * Return value:
 * On success returns '0' otherwise an error has occurred.
*/
int indexRepair();

#endif // UNITLOGD_H
