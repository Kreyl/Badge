/*
 * ImgList.h
 *
 *  Created on: 3 марта 2016 г.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"
#include "ff.h"

#define LINE_SZ         256
#define CONFIG_FILENAME "config.ini"

struct ImgInfo_t {
    uint32_t TimeToShow;
    int32_t FadeIn, FadeOut;   // Fade constants
    char Name[256];
} __packed;

class ImgList_t {
private:
    uint16_t PrevFadeOut;
    virtual_timer_t Tmr;
    bool IIsCfgOk = false, IIsActive = false;
    FIL IFile;
    ImgInfo_t Info;
    uint8_t ReadNextInfo();
public:
    uint8_t TryToConfig();
    bool IsActive() { return IIsActive; }
    void Start();
    void Stop();
    void OnTime();
};
