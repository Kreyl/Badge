/*
 * File:   main.cpp
 * Author: Kreyl
 * Project: Armlet2South
 *
 * Created on Feb 05, 2013, 20:27
 */

#include "kl_lib_f100.h"
#include "ch.h"
#include "hal.h"
#include "cmd_uart_f10x.h"
#include "cmd.h"
#include "Sequences.h"
#include "led_rgb.h"

// Button
#define BTN_POLL_INTERVAL   18
#define BTN_LONGPRESS_DELAY 3006
#define BTN_GPIO            GPIOA
#define BTN_PIN             0
#define BtnPressedNow()     (PinIsSet(BTN_GPIO, BTN_PIN))

#define THE_WORD            0xCa11  // For wake-up checking

LedRGB_t Led({GPIOB, 0, TIM3, 3}, {GPIOB, 3, TIM2, 2}, {GPIOB, 1, TIM3, 4});

enum State_t {stChanging, stStop, stSleep};
State_t State = stChanging;

// Prototypes
static inline void GoSleep();
static inline void LoadLedState();
static inline void SaveLedState();

int main(void) {
    // ==== Init clock system ====
    Clk.SetupBusDividers(ahbDiv4, apbDiv1, apbDiv1);
    Clk.UpdateFreqValues();
    // ==== Init OS ====
    halInit();
    chSysInit();
    // ==== Init Hard & Soft ====
    JtagDisable();
    Uart.Init(115200);
    Uart.Printf("\rBadge  AHB=%u; APB1=%u; APB2=%u\r\n", Clk.AHBFreqHz, Clk.APB1FreqHz, Clk.APB2FreqHz);
    PinSetupIn(BTN_GPIO, BTN_PIN, pudNone); // Button

    BackupRegs::Init();

    Led.Init();
    // Remap TIM2 CH2 to PB3
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    uint32_t tmp = AFIO->MAPR;
    tmp &= ~AFIO_MAPR_TIM2_REMAP;
    tmp |= AFIO_MAPR_TIM2_REMAP_PARTIALREMAP1;  // 01: Partial remap (CH1/ETR/PA15, CH2/PB3, CH3/PA2, CH4/PA3)
    AFIO->MAPR = tmp;
    RCC->APB2ENR &= ~RCC_APB2ENR_AFIOEN;

    uint32_t TimeStamp = 0;
    bool BtnWasPressed = false, BtnIgnore = false;

    // Check if just awake
    if(BackupRegs::Read(1) == THE_WORD) {
        Uart.Printf("\rWas in Standby");
        // wait for a while checking button
        TimeStamp = chTimeNow();
        do {
            chThdSleepMilliseconds(BTN_POLL_INTERVAL);
            if(!BtnPressedNow()) GoSleep();
        } while((chTimeNow() - TimeStamp) < BTN_LONGPRESS_DELAY);
        BtnIgnore = true;
        LoadLedState();
    }
    else Led.StartSequence(lsqChange);

    // Main cycle: btn polling
    while(true) {
        chThdSleepMilliseconds(BTN_POLL_INTERVAL);
        // OnButtonPress
        if(BtnPressedNow() and !BtnWasPressed) {
            BtnWasPressed = true;
            TimeStamp = chTimeNow();
        }

        // OnButtonRelease
        else if(!BtnPressedNow() and BtnWasPressed) {
            BtnWasPressed = false;
            if(BtnIgnore) BtnIgnore = false;
            else {
                if(State == stChanging) {
                    State = stStop;
                    Led.PauseSequence();
                }
                else {
                    State = stChanging;
                    Led.ProceedPausedSequence();
                }
            }
        }

        // OnButtonLongPress
        else if(BtnPressedNow() and ((chTimeNow() - TimeStamp) >= BTN_LONGPRESS_DELAY) and !BtnIgnore) {
            // Shutdown lights and allow user to release button to minimize risk of inintended "keypress"
            SaveLedState();
            Led.StopAndOff();
            chThdSleepMilliseconds(999);
            GoSleep();
        }
    } // while
}

void GoSleep() {
    chSysLock();
    Sleep::EnableWakeupPin();
    Sleep::EnterStandbyMode();
    chSysUnlock();
}


using namespace BackupRegs;
void LoadLedState() {
    State = State_t(Read(2));
    Color_t Clr;
    Clr.R = Read(3);
    Clr.G = Read(4);
    Clr.B = Read(5);
    uint32_t CurrentChunk = Read(6);
    Led.StartSequence(lsqChange, CurrentChunk);
    if(State == stStop) Led.PauseSequence();
    Led.SetColor(Clr);
    Uart.Printf("\rLoad: St=%u  R=%u G=%u B=%u  Indx=%u", State, Clr.R, Clr.G, Clr.B, CurrentChunk);
}

void SaveLedState() {
    AllowWrite();
    Write(1, THE_WORD);
    Write(2, (uint32_t)State);
    Color_t Clr = Led.GetCurrentColor();
    Write(3, Clr.R);
    Write(4, Clr.G);
    Write(5, Clr.B);
    uint32_t CurrentChunk = Led.GetCurrentChunkIndx();
    Write(6, CurrentChunk);
    DenyWrite();
//    Uart.Printf("\rSave: St=%u  R=%u G=%u B=%u  Indx=%u", State, Clr.R, Clr.G, Clr.B, CurrentChunk);
//    chThdSleepMilliseconds(999);
}

