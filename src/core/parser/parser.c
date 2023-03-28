/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

#include "../unitd_impl.h"

static int SECTION_CURRENT = NO_SECTION;

/* These static variables will be set according the functionality which requires them */
static int PARSER_SECTIONS_ITEMS_LEN = -1;
static SectionData *PARSER_SECTIONS_ITEMS;
static int PARSER_PROPERTIES_ITEMS_LEN = -1;
static PropertyData *PARSER_PROPERTIES_ITEMS;

static ErrorsData ERRORS_ITEMS[] = {
    { FIRST_CHARACTER_ERR, "An invalid character was found at the beginning of the line!" },
    { UNRECOGNIZED_ERR, "Unrecognized data '%s'!" },
    { OCCURRENCES_ERR, "The '%s' %s is not repeatable!" },
    { PROPERTY_SECTION_ERR, "The '%s' section doesn't predict '%s' property!" },
    { ACCEPTED_VALUE_ERR, "The '%s' value for the '%s' property is not allowed!" },
    { DUPLICATE_VALUE_ERR, "Duplicate value for the '%s' property!" },
    { REQUIRED_VALUE_ERR, "The '%s' %s is required!" },
    { NUMERIC_ERR, "The '%s' property only accepts a numeric value greater than zero!" },
    { EMPTY_VALUE_ERR, "The '%s' property has an empty value!" },
};

void
parserInit(ParserFuncType func)
{
    PropertyData *propertyData = NULL;
    assert(func != NO_FUNC);
    switch (func) {
        case PARSE_UNIT:
            PARSER_SECTIONS_ITEMS_LEN = UNITS_SECTIONS_ITEMS_LEN;
            PARSER_SECTIONS_ITEMS = UNITS_SECTIONS_ITEMS;
            PARSER_PROPERTIES_ITEMS_LEN = UNITS_PROPERTIES_ITEMS_LEN;
            PARSER_PROPERTIES_ITEMS = UNITS_PROPERTIES_ITEMS;
            break;
        case PARSE_TIMER_UNIT:
            PARSER_SECTIONS_ITEMS_LEN = UTIMERS_SECTIONS_ITEMS_LEN;
            PARSER_SECTIONS_ITEMS = UTIMERS_SECTIONS_ITEMS;
            PARSER_PROPERTIES_ITEMS_LEN = UTIMERS_PROPERTIES_ITEMS_LEN;
            PARSER_PROPERTIES_ITEMS = UTIMERS_PROPERTIES_ITEMS;
            break;
        case PARSE_PATH_UNIT:
            PARSER_SECTIONS_ITEMS_LEN = UPATH_SECTIONS_ITEMS_LEN;
            PARSER_SECTIONS_ITEMS = UPATH_SECTIONS_ITEMS;
            PARSER_PROPERTIES_ITEMS_LEN = UPATH_PROPERTIES_ITEMS_LEN;
            PARSER_PROPERTIES_ITEMS = UPATH_PROPERTIES_ITEMS;
            break;
        case PARSE_SOCK_REQUEST:
            /* We have not sections */
            PARSER_SECTIONS_ITEMS_LEN = 0;
            PARSER_SECTIONS_ITEMS = NULL;
            PARSER_PROPERTIES_ITEMS_LEN = SOCKREQ_PROPERTIES_ITEMS_LEN;
            PARSER_PROPERTIES_ITEMS = SOCKREQ_PROPERTIES_ITEMS;
            break;
        case PARSE_SOCK_RESPONSE:
            PARSER_SECTIONS_ITEMS_LEN = SOCKRES_SECTIONS_ITEMS_LEN;
            PARSER_SECTIONS_ITEMS = SOCKRES_SECTIONS_ITEMS;
            PARSER_PROPERTIES_ITEMS_LEN = SOCKRES_PROPERTIES_ITEMS_LEN;
            PARSER_PROPERTIES_ITEMS = SOCKRES_PROPERTIES_ITEMS;
            break;
        default:
            break;
    }
    /* Reset all */
    for (int i = 0; i < PARSER_SECTIONS_ITEMS_LEN; i++) {
        PARSER_SECTIONS_ITEMS[i].sectionCount = 0;
    }
    SECTION_CURRENT = NO_SECTION;
    for (int i = 0; i < PARSER_PROPERTIES_ITEMS_LEN; i++) {
        propertyData = &PARSER_PROPERTIES_ITEMS[i];
        propertyData->propertyCount = 0;
        propertyData->notDupValues = NULL;
    }
}

void
parserEnd(Array **errors, bool isAggregate)
{
    SectionData *sectionData = NULL;
    PropertyData *propertyData = NULL;

    if (!(*errors))
        *errors = arrayNew(objectRelease);
    if ((*errors)->size > 0 && !isAggregate)
        return;
    /* Check required section */
    for (int i = 0; i < PARSER_SECTIONS_ITEMS_LEN; i++) {
        sectionData = &PARSER_SECTIONS_ITEMS[i];
        if (sectionData->required && sectionData->sectionCount == 0) {
            arrayAdd(*errors, getMsg(-1, ERRORS_ITEMS[REQUIRED_VALUE_ERR].desc,
                                       sectionData->sectionName.desc, "section"));
            if (!isAggregate)
                return;
            else
                continue;
        }
    }
    /* Check required properties */
    for (int i = 0; i < PARSER_PROPERTIES_ITEMS_LEN; i++) {
        propertyData = &PARSER_PROPERTIES_ITEMS[i];
        if (propertyData->required && propertyData->propertyCount == 0) {
            arrayAdd(*errors, getMsg(-1, ERRORS_ITEMS[REQUIRED_VALUE_ERR].desc,
                                       propertyData->propertyName.desc, "property"));
            if (!isAggregate)
                return;
            else
                continue;
        }
    }
}

char*
getMsg(int numLine, const char *message, ...)
{
    char *error = NULL;
    char numLineStr[1000];
    char replacedMess[1000];

    assert(message);

    va_list args;
    va_start(args, message);
    if (numLine != -1) {
        error = stringNew("An error has occurred at line ");
        sprintf(numLineStr, "%d\n", numLine);
        vsprintf(replacedMess, message, args);
        stringAppendStr(&error, numLineStr);
        stringAppendStr(&error, replacedMess);
    }
    else {
        vsprintf(replacedMess, message, args);
        error = stringNew(replacedMess);
    }
    va_end(args);

    return error;
}

int
parseLine(char *line, int numLine, Array **keyVal, PropertyData **propertyData)
{
    int rv = 0;
    char *key, *value, *error;
    key = value = error = NULL;

    assert(line);
    assert(numLine);
    if (line) {
        /* ignore comments or empty lines */
        if (*line == '#' || *line == '\n')
            return rv;

        /* Split */
        *keyVal = stringSplitFirst(line, "=");
        if (!(*keyVal)) {
            *keyVal = arrayNew(objectRelease);
            /* Adding only the key which represents a Section */
            arrayAdd(*keyVal, stringNew(line));
            arrayAdd(*keyVal, NULL);
        }
        key = arrayGet(*keyVal, 0);
        value = arrayGet(*keyVal, 1);
        /* Right trim to intercept the eventual FIRST_CHARACTER_ERR error */
        stringRtrim(key, NULL);
        /* Trim */
        if (value)
            stringTrim(value, NULL);
    }

    /* Check key and value */
    if ((error = checkKeyVal(key, value, numLine, propertyData))) {
        arrayAdd(*keyVal, error);
        rv = 1;
    }

    return rv;
}

char*
checkKeyVal(char *key, char *value, int numLine, PropertyData **propertyData)
{
    char *error = NULL;
    bool found, valueFound, isDuplicate;
    SectionData *currentSectionData = NULL;
    PropertyData *currentPropertyData = NULL;
    const char **acceptedValues = NULL;
    Array *notDupValues = NULL;
    int size = 0;

    found = valueFound = isDuplicate = false;

    assert(numLine);
    assert(key);
    /* Sections and Properties can't start with blank or tab */
    if (isblank((unsigned char)*key) || *key == '\t') {
        error = getMsg(numLine, ERRORS_ITEMS[FIRST_CHARACTER_ERR].desc);
        return error;
    }

    /* Check section name */
    if (!value) {
        for (int i = 0; i < PARSER_SECTIONS_ITEMS_LEN; i++) {
            currentSectionData =  &PARSER_SECTIONS_ITEMS[i];
            if (stringEquals(key, currentSectionData->sectionName.desc)) {
                /* Setting the current section */
                SECTION_CURRENT = i;
                /* Incrementing the counter */
                currentSectionData->sectionCount++;
                found = true;
                break;
            }
        }
    }
    else {
        /* Check property name */
        for (int i = 0; i < PARSER_PROPERTIES_ITEMS_LEN; i++) {
            currentPropertyData = &PARSER_PROPERTIES_ITEMS[i];
            if (stringEquals(key, currentPropertyData->propertyName.desc)) {
                *propertyData = currentPropertyData;
                /* Incrementing the counter */
                currentPropertyData->propertyCount++;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        error = getMsg(numLine, ERRORS_ITEMS[UNRECOGNIZED_ERR].desc, key, NULL);
        return error;
    }

    /* We assert that it is a section or a property.
     * Both isn't possible!!
    */
    assert((currentSectionData && !currentPropertyData) ||
           (!currentSectionData && currentPropertyData));

    /* Check the occurrences number about the section */
    if (currentSectionData) {
        if (!currentSectionData->repeatable && currentSectionData->sectionCount > 1) {
            error = getMsg(numLine, ERRORS_ITEMS[OCCURRENCES_ERR].desc,
                             key, "section");
            return error;
        }
    }
    /* Check if the current section contains this property */
    else if (currentPropertyData) {
        /* If the property has been configured with "IdSection = NO_SECTION = -1"
         * means that it has not section
         */
        if (SECTION_CURRENT != NO_SECTION && SECTION_CURRENT != currentPropertyData->idxSectionItem) {
            error = getMsg(numLine, ERRORS_ITEMS[PROPERTY_SECTION_ERR].desc,
                             PARSER_SECTIONS_ITEMS[SECTION_CURRENT].sectionName.desc,
                             key);
            return error;
        }
        else {
            /* Check the occurrences number about the property */
            if (!currentPropertyData->repeatable && currentPropertyData->propertyCount > 1) {
                error = getMsg(numLine, ERRORS_ITEMS[OCCURRENCES_ERR].desc,
                                 key, "property");
                return error;
            }
            else {
                /* Check empty value */
                if (stringEquals(value, "")) {
                    error = getMsg(numLine, ERRORS_ITEMS[EMPTY_VALUE_ERR].desc,
                                   key);
                    return error;
                }
                /* Check if a property is a number */
                if (currentPropertyData->numeric && !isValidNumber(value, false)) {
                    error = getMsg(numLine, ERRORS_ITEMS[NUMERIC_ERR].desc,
                                     key);
                    return error;
                }
                /* Check if the property has the default values */
                if ((acceptedValues = currentPropertyData->acceptedValues)) {
                    while (*acceptedValues) {
                        if (stringEquals(value, *acceptedValues)) {
                            valueFound = true;
                            break;
                        }
                        acceptedValues++;
                    }
                    if (!valueFound) {
                        error = getMsg(numLine, ERRORS_ITEMS[ACCEPTED_VALUE_ERR].desc,
                                         value, currentPropertyData->propertyName.desc);
                        return error;
                    }
                }
                /* If the value has been found, check if it's a duplicate */
                if ((notDupValues = currentPropertyData->notDupValues)) {
                    size = notDupValues->size;
                    for (int i = 0; i < size; i++) {
                        if (stringEquals(value, arrayGet(notDupValues, i))) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    if (isDuplicate) {
                        error = getMsg(numLine, ERRORS_ITEMS[DUPLICATE_VALUE_ERR].desc,
                                         currentPropertyData->propertyName.desc);
                    }
                }
            }
        }
    }

    return error;
}

bool
isValidNumber(const char *value, bool zeroIncluded)
{
    if (value) {
        int len = strlen(value);
        for (int i = 0; i < len; i++) {
            if (!isdigit(value[i]))
                return false;
        }
        if ((!zeroIncluded && atol(value) <= 0) || (zeroIncluded && atol(value) < 0))
            return false;
    }
    return true;
}
