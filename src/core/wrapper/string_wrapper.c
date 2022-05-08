/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

char*
stringNew(const char *str)
{
    char *ret = NULL;
    if (str) {
        ret = calloc(strlen(str) + 1, sizeof(str));
        assert(ret);
        assert(strcpy(ret, str));
    }
    return ret;
}

char*
stringSet(char **str, const char *value)
{
    if (value) {
        if (*str)
            objectRelease(str);
        *str = stringNew(value);
    }
    return *str;
}

const char*
stringGet(char *str)
{
    return (const char*)str;
}

bool
stringStartsWithChr(const char *str, const char c)
{
    if (str && *str == c)
        return true;

    return false;
}

bool
stringStartsWithStr(const char *str, const char *searchStr)
{
    if (str && searchStr && strncmp(str, searchStr, strlen(searchStr)) == 0)
        return true;

    return false;
}

bool
stringEndsWithChr(const char *str, const char c)
{
    if (str && *(str + (strlen(str) - 1)) == c)
        return true;

    return false;
}

bool
stringEndsWithStr(const char *str, const char *searchStr)
{

    int lenStr = (str ? strlen(str) : 0);
    int lenSearchStr = (searchStr ? strlen(searchStr) : 0);

    if (lenStr > 0 && lenSearchStr > 0 && lenSearchStr <= lenStr) {

        int searchStrIndex = lenSearchStr - 1;
        str += lenStr - 1;
        searchStr += lenSearchStr - 1;

        while (*str) {
            if (*str == *searchStr) {
                if (searchStrIndex-- == 0)
                    return true;

                str--;
                searchStr--;
            }
            else break;
        }
    }

    return false;
}

bool
stringContainsChr(const char *str, const char c)
{
    if (str && c && strchr(str, c))
        return true;

    return false;
}

bool
stringContainsStr(const char *str, const char *searchStr)
{
    if (str && searchStr && strstr(str, searchStr))
        return true;

    return false;
}

void
stringToupper(char *str)
{
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}

void
stringTolower(char *str)
{
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
}

bool
stringAppendChr(char **a, const char b)
{
    if (a && b) {
        char supp[] = { b, '\0' };
        strcat(*a, supp);
        return true;
    }
    else
        return false;
}

bool
stringAppendStr(char **a, const char *b)
{
    if (a && b) {
        strcat(*a, b);
        return true;
    }
    else
        return false;
}

bool
stringConcat(char **a, const char *b)
{
    if (a) {
        if (b) {
            int lenA = strlen(*a);
            int lenB = strlen(b);
            *a = realloc(*a, (lenA + lenB + 1) * sizeof(char));
            assert(*a);
            memmove(*a + lenA, b, lenB * sizeof(char));
            *(*a + lenA + lenB) = '\0';
        }
        return true;
    }
    else
        return false;
}

bool
stringPrependChr(char **a, const char b)
{
    if (a && b) {
        char *copyA = stringNew(*a);
        (*a)[1] = '\0';
        (*a)[0] = b;
        strcat(*a, copyA);
        objectRelease(&copyA);
        return true;
    }
    else
        return false;
}

bool
stringPrependStr(char **a, const char *b)
{
    if (a && b) {
        char *copyA = stringNew(*a);
        **a = '\0';
        strcat(*a, b);
        strcat(*a, copyA);
        objectRelease(&copyA);
        return true;
    }
    else
        return false;
}

bool
stringInsertChrAt(char **a, const char b, int pos)
{
    if (a && b) {
        int len = strlen(*a);
        if (pos >= 0 && pos <= len) {
            char *copyA = stringNew(*a);
            char supp[] = { b, '\0' };
            (*a)[pos] = '\0';
            strcat(*a + pos, supp);
            strcat(*a + pos + 1, copyA + pos);
            objectRelease(&copyA);
            return true;
        }
    }
    return false;
}

bool
stringInsertStrAt(char **a, const char *b, int pos)
{
    if (a && b) {
        int lenA = strlen(*a);
        if (pos >= 0 && pos <= lenA) {
            char *copyA = stringNew(*a);
            (*a)[pos] = '\0';
            strcat(*a + pos, b);
            strcat(*a + pos + 1, copyA + pos);
            objectRelease(&copyA);
            return true;
        }
    }
    return false;
}

char*
stringLtrim(char *str, const char *seps)
{
    size_t totrim;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    totrim = strspn(str, seps);
    if (totrim > 0) {
        size_t len = strlen(str);
        if (totrim == len) {
            str[0] = '\0';
        }
        else {
            memmove(str, str + totrim, len + 1 - totrim);
        }
    }
    return str;
}

char*
stringRtrim(char *str, const char *seps)
{
    int i;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    i = strlen(str) - 1;
    while (i >= 0 && strchr(seps, str[i]) != NULL) {
        str[i] = '\0';
        i--;
    }
    return str;
}

char*
stringTrim(char *str, const char *seps)
{
    return stringLtrim(stringRtrim(str, seps), seps);
}

int
stringIndexOfChr(const char *str, const char c)
{
    if (str && c) {
        char *p = strchr(str, c);
        if (p) {
            int index = 0;
            while (str) {
                if (str == p)
                    return index;

                index++;
                str++;
            }
        }
    }
    return -1;
}

int
stringIndexOfStr(const char *str, const char *c)
{
    if (str && c) {
        char *p = strstr(str, c);
        if (p) {
            int index = 0;
            while (str) {
                if (str == p)
                    return index;

                index++;
                str++;
            }
        }
    }
    return -1;
}

int
stringLastIndexOfChr(const char *str, const char c)
{
    if (str && c) {
        int index = strlen(str) - 1;
        for (; index >= 0; index--) {
            if (str[index] == c)
                return index;
        }
    }
    return -1;
}

int
stringLastIndexOfStr(const char *str, const char *c)
{
    if (str && c) {
        int lenStr = strlen(str);
        int lenC = strlen(c);

        int indexStr = lenStr - 1;
        int indexC = lenC - 1;
        int elements = 0;
        bool found = false;

        while (indexStr >= 0) {
            if (str[indexStr] == c[indexC]) {
                found = true;
                elements++;
                if (elements == lenC)
                    return indexStr;
                indexStr--;
                indexC--;
            }
            else if (found) {
                indexC += elements;
                elements = 0;
                found = false;
            }
            else if (!found)
                indexStr--;
        }

    }
    return -1;
}

char *
stringSub(const char *str, int startIdx, int endIdx) {

    char *ret = NULL;
    int len = (str ? strlen(str) : 0);

    if (str && len > 0 && startIdx >= 0 && endIdx >= 0 &&
       endIdx < len) {
        int numElements = (endIdx - startIdx) + 1;
        if (numElements > 0) {
            ret = calloc(numElements + 1, sizeof(char));
            assert(ret);
            memmove(ret, str + startIdx, numElements * sizeof(char));
        }
    }

    return ret;
}

void
stringReplaceChr(char **str, const char a, const char b)
{
    if (*str) {
        char *temp = strchr(*str, a);
        if (temp)
            *temp = b;
    }
}

void
stringReplaceAllChr(char **str, const char a, const char b)
{
    if (*str) {
        char *temp = NULL;
        while ((temp = strchr(*str, a)) != NULL)
            *temp = b;
    }
}

void
stringReplaceStr(char **origin, const char *search, const char *replace)
{
    char *temp = NULL;
    if (*origin && (temp = strstr(*origin, search)) && replace) {
        char *copyOrigin = stringNew(*origin);
        int index = temp - *origin;
        (*origin)[index] = '\0';
        strcat(*origin, replace);
        strcat(*origin, copyOrigin + index + strlen(search));
        objectRelease(&copyOrigin);
    }
}

void
stringReplaceAllStr(char **origin, const char *search, const char *replace)
{
    char *temp = NULL;
    if (*origin && replace) {
        while ((temp = strstr(*origin, search)))
            stringReplaceStr(origin, search, replace);
     }
}

void
objectRelease(void **element)
{
    if (*element) {
        free(*element);
        *element = NULL;
    }
}

Array*
stringSplit(char *str, const char *sep, bool allocElements)
{
    if (str && sep && strstr(str, sep)) {
        Array *array = (allocElements ?
                        arrayNew(objectRelease) :
                        arrayNew(NULL));
        char *element = NULL;
        do {
            if (!element)
                element = strtok(str, sep);
            if (allocElements)
                arrayAdd(array, stringNew(element));
            else
                arrayAdd(array, element);
        } while ((element = strtok(NULL, sep)));
        return array;
    }
    return NULL;
}

void
stringSetDateTime(char **ret, bool hasMilliseconds)
{
    int millisec;
    char millisecStr[5];
    struct tm* timeInfo;
    struct timeval tv;
    char dateTime[50];

    gettimeofday(&tv, NULL);

    timeInfo = localtime(&tv.tv_sec);
    strftime(dateTime, 50, "%d %B %Y %H:%M:%S", timeInfo);
    if (hasMilliseconds) {
        millisec = tv.tv_usec/1000.0;
        if (millisec >= 1000) {
            millisec -= 1000;
            tv.tv_sec++;
        }
        sprintf(millisecStr, ".%03d", millisec);
        strcat(dateTime, millisecStr);
    }
    objectRelease(ret);
    *ret = stringNew(dateTime);
}

void
printTimeStamp(int options)
{
    int millisec;
    char millisecStr[5];
    struct tm* timeInfo;
    struct timeval tv;
    char dateTime[50];

    gettimeofday(&tv, NULL);

    timeInfo = localtime(&tv.tv_sec);
    strftime(dateTime, 50, "%d %B %Y %H:%M:%S", timeInfo);
    millisec = tv.tv_usec/1000.0;
    if (millisec >= 1000) {
        millisec -= 1000;
        tv.tv_sec++;
    }
    sprintf(millisecStr, ".%03d", millisec);
    strcat(dateTime, millisecStr);
    unitdLogInfo(options, "Timestamp = %s\n", dateTime);
}
