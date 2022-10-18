/*
(C) 2021 by Domenico Panella <pandom79@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3.
See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
*/

typedef enum {
    NO_FUNC = -1,
    PARSE_UNIT = 0,
    PARSE_SOCK_REQUEST = 1,
    PARSE_SOCK_RESPONSE_UNITLIST = 2,
    PARSE_SOCK_RESPONSE = 3
} ParserFuncType;

/* STATIC DATA */
#define NO_SECTION -1

/* Generic Errors */
typedef enum {
    FIRST_CHARACTER_ERR = 0,
    UNRECOGNIZED_ERR = 1,
    OCCURRENCES_ERR = 2,
    PROPERTY_SECTION_ERR = 3,
    ACCEPTED_VALUE_ERR = 4,
    DUPLICATE_VALUE_ERR = 5,
    REQUIRED_VALUE_ERR = 6,
    NUMERIC_ERR = 7
} ErrorsEnum;

typedef struct {
    ErrorsEnum errorEnum;
    const char *desc;
} ErrorsData;

/* DYNAMIC DATA which will be built from translation
 * unit that will include this header */

/* Sections */
typedef struct SectionName {
    int sectionNameEnum;
    const char *desc;
} SectionName;

typedef struct SectionData {
    SectionName sectionName;
    bool repeatable;
    bool required;
    int sectionCount;
} SectionData;

extern int UNITS_SECTIONS_ITEMS_LEN;
extern SectionData UNITS_SECTIONS_ITEMS[];

/* Properties */
typedef struct PropertyName {
    int propertyNameEnum;
    const char *desc;
} PropertyName;

typedef struct PropertyData {
    int idxSectionItem;
    PropertyName propertyName;
    bool repeatable;
    bool required;
    bool numeric;
    int propertyCount;
    const char **acceptedValues;
    Array *notDupValues;
} PropertyData;

extern int UNITS_PROPERTIES_ITEMS_LEN;
extern PropertyData UNITS_PROPERTIES_ITEMS[];

/* Functions */
void parserInit(ParserFuncType funcType);
int parseLine(char *, int, Array **keyVal, PropertyData **);
char* checkKeyVal(char *key, char *value, int numLine, PropertyData **);
bool isValidNumber(const char *, bool);
char* getMsg(int numLine, const char *message, ...);
void parserEnd(Array **, bool);
