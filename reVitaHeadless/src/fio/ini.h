#ifndef _INI_H_
#define _INI_H_

#include <stdbool.h>
#include <vitasdkkern.h>
#include <taihen.h>

// Sizes of the INI_READER fields. The sscanf() widths in ini.c must match them.
#define SECTION_SIZE 30
#define SECTION_ATTR_SIZE 10
#define ENTRY_NAME_SIZE 30
#define ENTRY_VALUE_SIZE 150
#define ENTRY_LIST_VALUE_SIZE 10

enum LINE_TYPE{
    INI_SECTION =0,
    INI_ENTRY
};

typedef struct INI{  
    char* buff;
    char* idx;
    int max;        // capacity of buff in bytes, terminator included
    bool overflow;  // set when an append did not fit; the output is truncated then
}INI;

typedef struct INI_READER_INTERNAL{ //Holds inernal pointers
    char* buff;     //Buffer ptr
    char* line;     //Current line ptr
    char* listEntry;//Current list entry ptr
    char* EOL;      //End Of Line ptr
    char* EOS;      //End Of String (BUFFER) ptr
    char* EOE;      //End Of list Entry ptr
}INI_READER_INTERNAL;

typedef struct INI_READER{  
    struct INI_READER_INTERNAL _;

    char section[SECTION_SIZE];
    char sectionAttr[SECTION_ATTR_SIZE];
    char name[ENTRY_NAME_SIZE];
    char val[ENTRY_VALUE_SIZE];
    char listVal[ENTRY_LIST_VALUE_SIZE];
}INI_READER;

void ini_append(struct INI* ini, const char *fmt, ...);

void ini_addNL(struct INI* ini);

void ini_addSection(struct INI* ini, const char* name);
void ini_addInt(struct INI* ini, const char* name, int val);
void ini_addStr(struct INI* ini, const char* name, const char* val);
void ini_addBool(struct INI* ini, const char* name, bool val);
void ini_addList(struct INI* ini, const char* name);
void ini_addListInt(struct INI* ini, int val);
void ini_addListStr(struct INI* ini, const char* val);
void ini_addBGR(struct INI* ini, const char* name, unsigned int val);

char* ini_nextLine(INI_READER* ini);
char* ini_nextEntry(INI_READER* ini);
char* ini_nextListVal(INI_READER* ini);

// max is the capacity of buff in bytes; generated text never exceeds it
struct INI ini_create(char* buff, int max);
// true when everything appended so far fitted in the buffer
bool ini_ok(struct INI* ini);
struct INI_READER ini_read(char* buff);

int parseInt(char* c);
bool parseBool(char* c);
int parseBGR(char* c);

#endif
