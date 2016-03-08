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

#define IMG_DELIMITERS      " ="

static inline char* skipleading(char *S) {
    while (*S != '\0' && *S <= ' ') S++;
    return S;
}

static void TmrImgCallback(void *p) {
    chSysLockFromISR();
    App.SignalEvtI(EVT_IMGLIST_TIME);
    chSysUnlockFromISR();
}

uint8_t ImgList_t::TryToConfig() {
    Stop();
    IIsCfgOk = false;
    uint8_t Rslt = TryOpenFileRead(CONFIG_FILENAME, &IFile);
//    Uart.Printf("CfgRslt: %u\r", Rslt);
    if(Rslt != OK) return FAILURE;
    // Check if at least single file exists
    while(true) {
        if(ReadNextInfo() == OK) {
            FILINFO FInfo;
            if(f_stat(Info.Name, &FInfo) == FR_OK) {
                if(FInfo.fsize > 12) {   // Min BMP file sz is 12
                    f_lseek(&IFile, 0); // Rewind file to beginning
                    IIsCfgOk = true;
                    return OK;
                }
            }
        }
        else return FAILURE;
    }
}

uint8_t ImgList_t::ReadNextInfo() {
    uint8_t Rslt;
    char S[LINE_SZ];
    *Info.Name = '\0';
    while(true) {
        Rslt = ReadLine(&IFile, S, LINE_SZ);
        if(Rslt != OK) {
//            Uart.Printf("RL: %u\r", Rslt);
            return Rslt;
        }
//        Uart.Printf(">%S\r", S);
        // Parse line
        char* StartP = skipleading(S);     // Skip leading spaces
        if(*StartP == ';' or *StartP == '#' or *StartP == '\0') continue;  // Skip comments
        // If name was found
        if(*StartP == '[') {    // New filename
            StartP++;           // Move to first char of name
            char* EndP = strchr(StartP, ']');
            if(EndP == nullptr) { Uart.Printf("Bad param at %S\r", StartP); continue; }
            int32_t len = EndP - StartP;
            // Check length: 1.bmp is shortest
            if(len < 5) { Uart.Printf("Bad fname1: %S\r", StartP); continue; }
            *EndP = 0;  // Make End of string
            EndP -= 4;  // Move to '.' char
            if(strcasecmp(EndP, ".BMP") != 0) { Uart.Printf("Bad fname2: %S\r", StartP); continue; }
            strcpy(Info.Name, StartP);
//            Uart.Printf("FName: %S\r", Info.Name);
            // Init info with default values
            Info.TimeToShow = -1;
            Info.FadeIn = -1;
            Info.FadeOut = -1;
            Info.BckltOn = -1;
        }
        // Parameter (maybe) was found. Skip if name was not found.
        else {
            char* ParName = strtok(StartP, IMG_DELIMITERS);
            char* ParValue = strtok(NULL, IMG_DELIMITERS);
//            Uart.Printf("ParName: %S; ParValue: %S\r", ParName, ParValue);
            if(*ParValue == '\0') continue;
            char *p;
            int32_t Value = strtol(ParValue, &p, 0);
            if(*p == '\0') {
                if     (strcasecmp(ParName, "FadeIn") == 0)     Info.FadeIn = Value;
                else if(strcasecmp(ParName, "TimeToShow") == 0) Info.TimeToShow = Value;
                else if(strcasecmp(ParName, "FadeOut") == 0)    Info.FadeOut = Value;
                else if(strcasecmp(ParName, "Backlight") == 0)  Info.BckltOn = Value;
                else Uart.Printf("Bad Par Name at %S\r", Info.Name);
            }
            else {
                Uart.Printf("NotANumber at %S\r", Info.Name);
                continue;
            }
            // Check if Info completed
            if(*Info.Name != '\0' and Info.FadeIn >= 0 and Info.TimeToShow > 0 and Info.FadeOut >= 0 and Info.BckltOn >= 0) {
//                Uart.Printf("Read Info ok\r");
                return OK;
            }
        } // Parameter
    } // while readline
}

void ImgList_t::Start() {
    if(!IIsCfgOk) return;
    PrevFadeOut = 0;
    IIsActive = true;
    OnTime();
}

void ImgList_t::Stop() {
    chVTReset(&Tmr);
    IIsActive = false;
}

void ImgList_t::OnTime() {
    uint8_t Rslt;
    bool WrapAround = false;
    while(true) {
        Rslt = ReadNextInfo();
        if(Rslt == END_OF_FILE) {
            if(WrapAround) return; // Seems like empty file
            WrapAround = true;
            // Reread file
            f_close(&IFile);
            Rslt = TryOpenFileRead(CONFIG_FILENAME, &IFile);
            if(Rslt != OK) return;
            continue;
        }
        else if(Rslt != OK) return; // Read info failure
        // Info ok
//        Uart.Printf("OnTime %S t=%u; fi=%u; fo=%u\r", Info.Name, Info.TimeToShow, Info.FadeIn, Info.FadeOut);
        Rslt = Lcd.DrawBmpFile(0,0, Info.Name, &File, Info.BckltOn, Info.FadeIn, PrevFadeOut);
        // Start timer if draw succeded
        if(Rslt == OK) {
            chVTSet(&Tmr, MS2ST(Info.TimeToShow), TmrImgCallback, nullptr);
            PrevFadeOut = Info.FadeOut;
            return;
        }
    } // while true
}
