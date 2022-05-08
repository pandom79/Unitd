/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

/* If the array contains elements of the same type then
 * you can pass a function pointer which will be automatically called to release
 * the element when we remove or set the element or when we release the whole array.
 * It's optional thus can accept NULL value.
*/
Array*
arrayNew(void (*release_fn)(void **))
{
    Array *array = calloc(1, sizeof(Array));
    assert(array);
    array->size = 0;
    array->arr = calloc(1, sizeof(void *));
    assert(array->arr);
    if (release_fn)
        array->release_fn = release_fn;

    return array;
}

/* If the array contains elements of the same type then
 * you can pass a function pointer which will be automatically called to release
 * the element when we remove or set the element or when we release the whole array.
 * It's optional thus can accept NULL value.
*/
Array*
arrayNewWithAmount(int amount, void (*release_fn)(void **))
{
    if (amount > 0) {
        Array *array = calloc(1, sizeof(Array));
        assert(array);
        array->size = amount;
        array->arr = calloc(amount, sizeof(void *));
        assert(array->arr);
        if (release_fn)
            array->release_fn = release_fn;

        return array;
    }
    return NULL;
}

bool
arrayAdd(Array *array, void *element)
{
    if (array) {
        int size = array->size++;
        void ***arr = &array->arr;
        if (size > 0) {
            *arr = realloc(*arr, (size + 1) * sizeof(void *));
            assert(*arr);
        }
        (*arr)[size] = element;
        return true;
    }
    return false;
}

bool
arrayAddFirst(Array *array, void *element)
{
    if (array) {
        int size = ++array->size;
        void ***arr = &array->arr;
        if (size > 0) {
            *arr = realloc(*arr, (size) * sizeof(void *));
            assert(*arr);
        }
        for (int i = (size - 1); i > 0; i--)
            (*arr)[i] = (*arr)[i - 1];

        (*arr)[0] = element;
        return true;
    }
    return false;
}

/*
 * We move the elements into memory decreasing the array size.
 * The element must be freed before remove operation
 * if a release function pointer is not set.
 */
bool
arrayRemoveAt(Array *array, int idx)
{
    if (array) {
        void **arr = array->arr;
        int *size = &array->size;
        void (*release_fn)(void **) = array->release_fn;
        if (arr && idx >= 0 && idx < *size) {
            /* Clean the item */
            if (release_fn)
                (*release_fn)(&(arr[idx]));

            if (idx < (*size - 1))
                memmove(arr + idx, arr + idx + 1, (*size - (idx + 1)) * sizeof(void *));
            (*size)--;
            return true;
        }
    }
    return false;
}

/*
 * We move the elements into memory decreasing the array size.
 * The element must be freed before remove operation
 * if a release function pointer is not set.
 */
bool
arrayRemove(Array *array, void *element)
{
    if (array) {
        void **arr = array->arr;
        int *size = &array->size;
        void (*release_fn)(void **) = array->release_fn;
        for (int i = 0; i < *size; i++) {
            if (arr[i] == element) {
                /* Clean the item */
                if (release_fn)
                    (*release_fn)(&element);

                if (i < (*size - 1))
                    memmove(arr + i, arr + i + 1, (*size - (i + 1)) * sizeof(void *));
                (*size)--;
                return true;
            }
        }
    }
    return false;
}

/* We only release Array->arr and and Array pointer */
/* Each element must be freed before release operation
 * if a release function pointer is not set.
 */
bool
arrayRelease(Array **array)
{
    if (*array) {
        void **arr = (*array)->arr;
        void (*release_fn)(void **) = (*array)->release_fn;
        if (release_fn) {
            int *size = &(*array)->size;
            for (int i = 0; i < *size; )
                arrayRemoveAt((*array), i);
        }
        if (arr) {
            free(arr);
            arr = NULL;
        }
        free(*array);
        *array = NULL;
        return true;
    }
    return false;
}

/*
 * We only set the element at the 'idx' index.
 * The replaced element must be freed before set operation
 * if a release function pointer is not set.
 */
bool
arraySet(Array *array, void *element, int idx)
{
    if (array && element && idx < array->size) {
        void **arr = array->arr;
        void (*release_fn)(void **) = array->release_fn;
        if (release_fn)
            release_fn(&(arr[idx]));

        arr[idx] = element;
        return true;
    }

    return false;
}

/* This function only works for the array of strings */
bool
arrayContainsStr(Array *array, const char *str)
{
    int len = (array ? array->size : 0);
    for (int i = 0; i < len; i++) {
        if (strcmp(str, arrayGet(array, i)) == 0)
            return true;
    }

    return false;
}

Array*
arrayStrCopy(Array *strArr)
{
    Array *ret = NULL;
    if (strArr) {
        int len = strArr->size;
        if (len > 0)
            ret = arrayNew(objectRelease);
        for (int i = 0; i < len; i++)
            arrayAdd(ret, stringNew(arrayGet(strArr, i)));
    }

    return ret;
}

void*
arrayGet(Array *array, int idx)
{
    if (array && idx < array->size)
        return array->arr[idx];

    return NULL;
}

int
arrayGetIdx(Array *array, void *element)
{
    if (array) {
        void **arr = array->arr;
        int size = array->size;
        for (int i = 0; i < size; i++) {
            if (arr[i] == element)
                return i;
        }
    }

    return -1;
}

void
arrayPrint(int options, Array **array, bool hasStrings)
{
    int len = (*array ? (*array)->size : 0);
    if (hasStrings) {
        for (int i = 0; i < len; i++)
            unitdLogInfo(options, "%s\n", (char *)(*array)->arr[i]);
    }
    else {
        for (int i = 0; i < len; i++)
            unitdLogInfo(options, "%p\n", (*array)->arr[i]);
    }
}

