/*
 * main.h
 *
 *  Created on: 15 ����. 2014 �.
 *      Author: g.kruglov
 */

#pragma once

#include "ch.h"
#include "kl_lib.h"
#include "uart.h"
#include "evt_mask.h"
#include "board.h"
#include "lcd_round.h"
#include "battery_consts.h"

#define APP_NAME                "Badge3"

#define MEASUREMENT_PERIOD_MS   2700

class App_t {
private:
    thread_t *PThread;
public:
    bool IsDisplayingBattery;
    void DrawNext();
    uint8_t BatteryPercent = 255; // dummy value
    void OnAdcSamplingTime();
    void OnAdcDone(LcdHideProcess_t Hide);
    void Shutdown();
    void OnBtnPress();
    // Eternal methods
    void InitThread() { PThread = chThdGetSelfX(); }
    void SignalEvt(eventmask_t Evt) {
        chSysLock();
        chEvtSignalI(PThread, Evt);
        chSysUnlock();
    }
    void SignalEvtI(eventmask_t Evt) { chEvtSignalI(PThread, Evt); }
    void OnCmd(Shell_t *PShell);
    // Inner use
    void ITask();
};

extern App_t App;
