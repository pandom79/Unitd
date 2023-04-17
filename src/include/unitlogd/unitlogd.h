/*
(C) 2022 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#ifndef UNITLOGD_H
#define UNITLOGD_H

/**
 * @brief Unitlogd library
 * @file unitlogd.h
 * @author Domenico Panella <pandom79@gmail.com>
*/

/**
 * @struct IndexEntry
 * @brief This structure contains the index entry data.
 * @var IndexEntry::bootId
 * Contains the boot id.
 * @var IndexEntry::start
 * Contains the start timestamp.
 * @var IndexEntry::startOffset
 * Contains the start offset.
 * @var IndexEntry::stop
 * Contains the stop timestamp.
 * @var IndexEntry::stopOffset
 * Contains the stop offset.
 *
*/
typedef struct {
    char *bootId;
    Time *start;
    char  *startOffset;
    Time *stop;
    char  *stopOffset;
} IndexEntry;

/**
 * Populate an array of IndexEntry structures.<br>
 * The array must be freed via 'arrayRelease' function.<br>
 * Return value:<br>
 * On success returns '0' otherwise an error has occurred.
 * @param[in] bootsList
 * @return integer
 */
int getBootsList(Array **bootsList);

/**
 * Repair the index reading the entries from log.<br>
 * This function is useful in the corruption case of the index file.<br>
 * Return value:<br>
 * On success returns '0' otherwise an error has occurred.
 * @return integer
 */

int indexRepair();

/**
 * Reduces log disk space by removing the lines between<br>
 * the values of the 'startOffset' and 'stopOffset' arguments.<br>
 * Return value:<br>
 * On success returns '0' otherwise an error has occurred.
 * @param[in] startOffset
 * @param[in] stopOffset
 * @return integer
*/
int cutLog(off_t startOffset, off_t stopOffset);

#endif // UNITLOGD_H
