/*
 * main.h
 *
 *  Created on: 15 сент. 2014 г.
 *      Author: g.kruglov
 */

#pragma once

#include "ch.h"
#include "kl_lib.h"
#include "uart.h"
#include "evt_mask.h"
#include "board.h"
#include "battery_consts.h"

#define APP_NAME                "Badge2"
#define APP_VERSION             __DATE__ " " __TIME__

#define MEASUREMENT_PERIOD_MS   2700

class App_t {
private:
    thread_t *PThread;
public:
    bool IsDisplayingBattery = false;
    void DrawNextBmp();
    uint8_t BatteryPercent;
    void OnAdcSamplingTime();
    void OnAdcDone();
    void Shutdown();
    uint8_t TryInitFS();
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
