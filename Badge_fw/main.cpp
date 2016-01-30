/*
 * main.cpp
 *
 *  Created on: 20 ����. 2014 �.
 *      Author: g.kruglov
 */

#include <descriptors_msd.h>
#include <usb_msd.h>
#include "main.h"
#include "lcd_round.h"
#include "SimpleSensors.h"
#include "FlashW25Q64t.h"
#include "ff.h"

App_t App;

FATFS FatFS;
FIL File;

uint8_t Buf[256];

int main(void) {
    // ==== Setup clock frequency ====
//    Clk.EnablePrefetch();
    Clk.SetupBusDividers(ahbDiv2, apbDiv1);
    Clk.UpdateFreqValues();

    // Init OS
    halInit();
    chSysInit();
    App.InitThread();

    // ==== Init hardware ====
    Uart.Init(115200, UART_GPIO, UART_TX_PIN);//, UART_GPIO, UART_RX_PIN);
    Uart.Printf("\r%S %S\r", APP_NAME, APP_VERSION);
    Clk.PrintFreqs();

//    Lcd.Init();
//    Lcd.SetBrightness(100);
//    Lcd.PutBitmap(80, 20, 100, 150, NULL);

    Mem.Init();
    PinSensors.Init();
    UsbMsd.Init();

    // FAT init
    FRESULT r = f_mount(&FatFS, "", 1); // Mount it now
    if(r != FR_OK) Uart.Printf("FS mount error: %u\r", r);
    else {
        r = f_open(&File, "readme.txt", FA_READ);
        if(r != FR_OK) Uart.Printf("Open error: %u\r", r);
        else {
            uint32_t BytesRead=0;
            r = f_read(&File, Buf, 255, &BytesRead);
            if(r != FR_OK) Uart.Printf("Read error: %u\r", r);
            else {
                Buf[BytesRead] = 0;
                Uart.Printf("> %S\r", Buf);
            }
        }
    }

    // Main cycle
    App.ITask();
}

__attribute__ ((__noreturn__))
void App_t::ITask() {
    while(true) {
        uint32_t EvtMsk = chEvtWaitAny(ALL_EVENTS);
        if(EvtMsk & EVTMSK_UART_NEW_CMD) {
            OnCmd((Shell_t*)&Uart);
            Uart.SignalCmdProcessed();
        }
#if 0 // ==== USB ====
        if(EvtMsk & EVTMSK_USB_CONNECTED) {
            Uart.Printf("5v is here\r");
            chThdSleepMilliseconds(270);
            // Enable HSI48
            chSysLock();
            uint8_t r = Clk.SwitchTo(csHSI48);
            Clk.UpdateFreqValues();
            Uart.OnAHBFreqChange();
            chSysUnlock();
            Clk.PrintFreqs();
            if(r == OK) {
                Clk.SelectUSBClock_HSI48();
                Clk.EnableCRS();
                UsbMsd.Connect();
            }
            else Uart.Printf("Hsi Fail\r");
        }

        if(EvtMsk & EVTMSK_USB_READY) {
            Uart.Printf("UsbReady\r");
        }
#endif
    } // while true
}

#if 1 // ======================= Command processing ============================
void App_t::OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
    Uart.Printf("%S\r", PCmd->Name);
    // Handle command
    if(PCmd->NameIs("Ping")) {
        PShell->Ack(OK);
    }

    else PShell->Ack(CMD_UNKNOWN);
}
#endif

// 5v Sns
void Process5VSns(PinSnsState_t *PState, uint32_t Len) {
    if(PState[0] == pssRising) App.SignalEvt(EVTMSK_USB_CONNECTED);
    else if(PState[0] == pssFalling) App.SignalEvt(EVTMSK_USB_DISCONNECTED);
}
// Button
void ProcessBtnPress(PinSnsState_t *PState, uint32_t Len) {
    if(PState[0] == pssRising) App.SignalEvt(EVTMSK_BTN_PRESS);
//    else if(PState[0] == pssFalling) App.SignalEvt(EVTMSK_USB_DISCONNECTED);
}
