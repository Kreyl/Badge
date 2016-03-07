/*
 * ImgList.h
 *
 *  Created on: 3 марта 2016 г.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"

#define FILENAME_SZ     13  // 8 + . + 3 + \0
#define LIST_CNT        20

#define LINE_SZ         256

struct ImgInfo_t {
    char Name[FILENAME_SZ];
    uint32_t TimeToShow;
    uint16_t FadeIn, FadeOut;   // Fade constants
} __packed;

class ImgList_t {
private:
    uint32_t Count, Current;
    ImgInfo_t Info[LIST_CNT];
    uint16_t PrevFadeOut;
    virtual_timer_t Tmr;
public:
    bool AutoChange = false;
    uint8_t TryToConfig(const char* Filename);
    void Print();
    void Start();
    void Stop();
    void OnTime();
};
