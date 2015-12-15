/*
 * cmd.h
 *
 *  Created on: 26 сент. 2014 г.
 *      Author: g.kruglov
 */

#ifndef CMD_H_
#define CMD_H_

#include "hal.h"
#include <cstring>
#include "stdlib.h"

#define DELIMITERS      " ,"
#define EMPTY_STRING    10
#define NOT_A_NUMBER    11

enum ProcessDataResult_t {pdrProceed, pdrNewCmd};

template <uint32_t BufSz>
class Cmd_t {
private:
    char IString[BufSz];
    uint32_t Cnt;
public:
    char *Name, *Token;
    ProcessDataResult_t PutChar(char c) {
//        Uart.Printf("\r> %c %d", c, c);
        if(c == '\b') { if(Cnt > 0) Cnt--; }    // do backspace
        else if((c == '\r') or (c == '\n')) {   // end of line, check if cmd completed
            if(Cnt != 0) {  // if cmd is not empty
                IString[Cnt] = 0; // End of string
                Name = strtok(IString, DELIMITERS);
                return pdrNewCmd;
            }
        }
        else if(c == '#') { // New Cmd started
            Cnt = 1;
            IString[0] = c;
        }
        else if(Cnt < BufSz-1) IString[Cnt++] = c;  // Add char if buffer not full
        return pdrProceed;
    }
    uint8_t GetNextToken() {
        Token = strtok(NULL, DELIMITERS);
        return (*Token == '\0')? FAILURE : OK;
    }
    void Reset() { Cnt = 0; }
    uint8_t TryConvertTokenToInt(int32_t *POutput) {
        if(*Token == '\0') return EMPTY_STRING;
        char *p;
        *POutput = strtol(Token, &p, 0);
        return (*p == '\0')? OK : NOT_A_NUMBER;
    }
    bool NameIs(const char *S) { return (strcasecmp(Name, S) == 0); }
    bool TokenIs(const char *S) { return (strcasecmp(Token, S) == 0); }
    uint8_t GetNextTokenAndConvertToUint8(uint8_t *POutput) {
        int32_t d=0;
        if(GetNextToken() != OK) return FAILURE;
        if(TryConvertTokenToInt(&d) != OK) return FAILURE;
        if(d < 0 or d > 255) return FAILURE;
        *POutput = d;
        return OK;
    }
    uint8_t GetNextTokenAndConvertToInt32(uint32_t *POutput) {
        int32_t d=0;
        if(GetNextToken() != OK) return FAILURE;
        if(TryConvertTokenToInt(&d) != OK) return FAILURE;
        *POutput = d;
        return OK;
    }
};

#endif /* CMD_H_ */
