/*
 * ImgList.cpp
 *
 *  Created on: 3 марта 2016 г.
 *      Author: Kreyl
 */

#include "ImgList.h"
#include "kl_fs_common.h"
#include "uart.h"
#include <cstring>
#include "lcd_round.h"
#include "main.h"

#define IMG_DELIMITERS      " :="

static inline char* skipleading(char *S) {
    while (*S != '\0' && *S <= ' ') S++;
    return S;
}

static void TmrImgCallback(void *p) {
    chSysLockFromISR();
    App.SignalEvtI(EVTMSK_IMGLIST_TIME);
    chSysUnlockFromISR();
}

uint8_t ImgList_t::TryToConfig(const char* Filename) {
    // Emty list
    Count = 0;
    uint32_t Indx = 0;
    if(TryOpenFileRead(Filename, &File) != OK) return FAILURE;
    char S[LINE_SZ];
    while(ReadLine(&File, S, LINE_SZ) == OK) {
        char* StartP = skipleading(S);     // Skip leading spaces
        if(*StartP == ';' or *StartP == '#' or *StartP == '\0') continue;  // Skip comments
        // If name was found
        if(*StartP == '[') {    // New filename
            StartP++;           // Move to first char of name
            char* EndP = strchr(StartP, ']');
            int32_t len = EndP - StartP;
            if(len < 5 or len > 12) {   // 1.bmp is shortest, 12345678.bmp is longest
                Uart.Printf("Bad fname at %u\r", Indx);
                goto end;
            }
            *EndP = 0;   // End of string
            Count++;
            Indx = Count-1;
            strcpy(Info[Indx].Name, StartP);
            // Init info with default values
            Info[Indx].TimeToShow = 2700;
            Info[Indx].FadeIn = 0;
            Info[Indx].FadeOut = 0;
        }
        // Parameter (maybe) was found
        else {
            char* ParName = strtok(StartP, IMG_DELIMITERS);
            char* ParValue = strtok(NULL, IMG_DELIMITERS);
//            Uart.Printf("ParName: %S; ParValue: %S\r", ParName, ParValue);
            if(*ParValue == '\0') continue;
            char *p;
            int32_t Value = strtol(ParValue, &p, 0);
            if(*p == '\0') {
                if     (strcasecmp(ParName, "FadeIn") == 0)     Info[Indx].FadeIn = Value;
                else if(strcasecmp(ParName, "TimeToShow") == 0) Info[Indx].TimeToShow = Value;
                else if(strcasecmp(ParName, "FadeOut") == 0)    Info[Indx].FadeOut = Value;
                else Uart.Printf("Bad Par Name at %u\r", Indx);
            }
            else {
                Uart.Printf("NotANumber at %u\r", Indx);
                continue;
            }
        } // not a section
    }
    end:
    f_close(&File);
    Print();
    return (Count > 0)? OK : FAILURE;
}

void ImgList_t::Print() {
    Uart.Printf("Config Cnt=%u\r", Count);
    for(uint32_t i=0; i<Count; i++) {
        Uart.Printf("%S: Time=%u; FIn=%u; FOut=%u\r", Info[i].Name, Info[i].TimeToShow, Info[i].FadeIn, Info[i].FadeOut);
    }
}

void ImgList_t::Start() {
    if(Count == 0) return;
    Current = 0;
    OnTime();
}

void ImgList_t::OnTime() {
    Uart.Printf("OnTime %u %S\r", Current, Info[Current].Name);
//    uint8_t Rslt = Lcd.DrawBmpFile(0,0, Info[Current].Name, &File);
    Current++;
    if(Current >= Count) Current = 0;
    // Start timer if draw succeded
//    if(Rslt == OK) chVTSet(&Tmr, MS2ST(Info[Current].TimeToShow), TmrImgCallback, nullptr);
}
