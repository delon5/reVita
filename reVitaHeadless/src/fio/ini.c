#include <vitasdkkern.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include "ini.h"
#include "../log.h"

#define TRUE_STR "TRUE"
#define FALSE_STR "FALSE"
_Static_assert(SECTION_SIZE == 30 && SECTION_ATTR_SIZE == 10 && ENTRY_NAME_SIZE == 30 && ENTRY_VALUE_SIZE == 150,
    "update the sscanf() field widths in ini_nextEntry()");

// Bounded append: never writes past ini->max bytes. The text is formatted into
// a local scratch first because libk's (PDCLib) vsnprintf stores literal format
// characters and the terminator without honouring its size argument, so it
// cannot be trusted to bound writes into the INI buffer itself. Every ini_*
// format here produces well under INI_APPEND_MAX bytes (keys <= 30 chars,
// values <= 31 chars, list items <= 9 chars). On overflow the output is
// truncated and ini->overflow is set so the caller can refuse to save.
#define INI_APPEND_MAX 256
void ini_append(struct INI* ini, const char *fmt, ...) {
    char tmp[INI_APPEND_MAX];
    va_list va;
    va_start (va, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, va);
    va_end (va);
    int room = ini->max - (int)(ini->idx - ini->buff);   // bytes left, terminator included
    if (n < 0 || n >= (int)sizeof(tmp) || n >= room){
        ini->overflow = true;
        if (ini->max > 0)
            ini->buff[ini->max - 1] = '\0';
        return;
    }
    memcpy(ini->idx, tmp, n + 1);
    ini->idx += n;
}

void ini_addNL(struct INI* ini){
	ini_append(ini, "\n");
}

void ini_addSection(struct INI* ini, const char* name) {
	ini_append(ini, "\n[%s]", name);
}
void ini_addInt(struct INI* ini, const char* name, int val) {
	ini_append(ini, "\n%s=%i", name, val);
}
void ini_addStr(struct INI* ini, const char* name, const char* val) {
	ini_append(ini, "\n%s=%s", name, val);
}
void ini_addBool(struct INI* ini, const char* name, bool val) {
	ini_append(ini, "\n%s=%s", name, val ? TRUE_STR : FALSE_STR);
}
void ini_addList(struct INI* ini, const char* name) {
    ini_append(ini, "\n%s=", name);
}
void ini_addListInt(struct INI* ini, int val){
	ini_append(ini, "%i,", val);
}
void ini_addListStr(struct INI* ini, const char* val){
	ini_append(ini, "%s,", val);
}
void ini_addBGR(struct INI* ini, const char* name, uint val){
    ini_append(ini, "\n%s=#%02X%02X%02X", name, 
            val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF);
}

char* ini_nextLine(INI_READER* ini){
    if (ini->_.EOL == ini->_.EOS)           //End of buffer reached
        return NULL;                    //No more lines
    if (ini->_.EOL == NULL)               //if first call
        ini->_.line = ini->_.buff;          //Set line start at buff start
    else
        ini->_.line = &ini->_.EOL[1];       
    ini->_.EOL = strchr(ini->_.line, '\n'); 
    if (ini->_.EOL == NULL)
        ini->_.EOL = ini->_.EOS;
    ini->_.EOL[0] = '\0';
    return ini->_.line;
}
// Iterative on purpose: one frame per file, however many section or blank
// lines it contains (the kernel thread stack is small).
char* ini_nextEntry(INI_READER* ini){
    for (;;){
        if (ini_nextLine(ini) == NULL)
            return NULL;
        if (!strlen(ini->_.line)) 
            continue;
        if (ini->_.line[0] == '[') { // Looks like section
            char section[SECTION_SIZE];
            char sectionAttr[SECTION_ATTR_SIZE];
            if (sscanf(ini->_.line, "[%29[^:]:%9[^]]", section, sectionAttr) == 2){ // Section with attr
                strcpy(ini->section, section);
                strcpy(ini->sectionAttr, sectionAttr);
            } else if (sscanf (ini->_.line, "[%29[^]]", section) == 1){ // Section without attr
                strcpy(ini->section, section);
                ini->sectionAttr[0] = '\0';
            }// Error parsing: skip the line
            continue;
        } else if (strchr(ini->_.line, '=')){ // Looks like Entry
            char key[ENTRY_NAME_SIZE];
            char value[ENTRY_VALUE_SIZE];
            int num = sscanf (ini->_.line, "%29[^=]=%149s", key, value);
            if (num >= 1){
                strcpy(ini->name, key);
                strcpy(ini->val, num == 2 ? value : "");
                strcpy(ini->listVal, "");
                ini->_.EOE = NULL;
                return ini->name;
            } // Error parsing: skip the line
        }
    }
}
char* ini_nextListVal(INI_READER* ini){
    if (ini->_.EOE == ini->_.EOL)          //End of list reached
        return NULL;                       //No more list entries to return
    if (ini->_.EOE == NULL){               //if first func call
        // char* eq = strchr(ini->_.line, '=');
        // if (eq[1] == '\n')                  //if list value is empty
        //     return NULL;
        // ini->_.listEntry = &eq[1];
        ini->_.listEntry = &strchr(ini->_.line, '=')[1];
    } else
        ini->_.listEntry = &ini->_.EOE[1];  
    ini->_.EOE = strchr(ini->_.listEntry, ',');  
    if (ini->_.EOE == NULL)
        ini->_.EOE = ini->_.EOL;
    int len = (ini->_.EOE - ini->_.listEntry)/sizeof(char);
    if (len > ENTRY_LIST_VALUE_SIZE - 1)
        len = ENTRY_LIST_VALUE_SIZE - 1;
    strncpy(ini->listVal, ini->_.listEntry, len);
    ini->listVal[len] = '\0';
    return &ini->listVal[0];
}

struct INI ini_create(char* buff, int max){
    struct INI ini;
    ini.buff = buff;
    ini.idx = buff;
    ini.max = max;
    ini.overflow = false;
    if (max > 0)
        buff[0] = '\0';
    return ini;
}
bool ini_ok(struct INI* ini){
    return !ini->overflow;
}
struct INI_READER ini_read(char* buff){
    static struct INI_READER ini;
    ini._.buff = buff;
    ini._.EOS = &buff[strlen(buff)];
    return ini;
}

int parseInt(char* c){
    int result = 0;
    if (c == NULL || sscanf(c, "%d", &result) != 1)
        return 0;
    return result;
}
bool parseBool(char* c){
    return (c != NULL && !strcmp(c, TRUE_STR)) ? true : false;
}
void addNumber(int* val, uint8_t number, uint8_t pos){
    uint8_t alignedPos = (pos % 2) ? pos - 1 : pos + 1;
    *val += number << ((alignedPos) * 4);
}
int parseBGR(char* c){
    //sscnf cannot parse HEX unfortunatly - had to do it manually
    int ret = 0xFF000000;
    if (c == NULL)
        return ret;
    size_t len = strlen(c);           // "#RRGGBB": hex digits at c[1..6]
    for (size_t i = 0; i + 1 < len && i < 6; i++){
        switch(c[i + 1]){
            case '1': addNumber(&ret, 0x1, i); break;
            case '2': addNumber(&ret, 0x2, i); break;
            case '3': addNumber(&ret, 0x3, i); break;
            case '4': addNumber(&ret, 0x4, i); break;
            case '5': addNumber(&ret, 0x5, i); break;
            case '6': addNumber(&ret, 0x6, i); break;
            case '7': addNumber(&ret, 0x7, i); break;
            case '8': addNumber(&ret, 0x8, i); break;
            case '9': addNumber(&ret, 0x9, i); break;
            case 'A': addNumber(&ret, 0xA, i); break;
            case 'B': addNumber(&ret, 0xB, i); break;
            case 'C': addNumber(&ret, 0xC, i); break;
            case 'D': addNumber(&ret, 0xD, i); break;
            case 'E': addNumber(&ret, 0xE, i); break;
            case 'F': addNumber(&ret, 0xF, i); break;
            default: break;
        }
    }
    return ret;
}
