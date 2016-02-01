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
#include "kl_fs_common.h"

App_t App;

// Extension of graphic files to search
const char Extension[] = "*.bmp";

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


//    Uart.PrintfNow("cr20=%X; cfgr=%X; cfgr3=%X\r", RCC->CR2, RCC->CFGR, RCC->CFGR3);
//    while(true) {
//        chThdSleepMilliseconds(540);
//        Clk.EnableHSI48();
//        Uart.PrintfNow("cr21=%X; cfgr=%X; cfgr3=%X\r", RCC->CR2, RCC->CFGR, RCC->CFGR3);
//        chThdSleepMilliseconds(540);
////        RCC->CFGR3 |= RCC_CFGR3_USBSW_PLLCLK;
////        RCC->CR2 = 0;
////        RCC->CR2 &=
//        Clk.DisableHSI48();
//        Uart.PrintfNow("cr22=%X; cfgr=%X; cfgr3=%X\r", RCC->CR2, RCC->CFGR, RCC->CFGR3);
//    }

    Lcd.Init();
    Lcd.SetBrightness(100);

    Mem.Init();
    UsbMsd.Init();

    // ==== FAT init ====
    FRESULT rslt = f_mount(&FatFS, "", 1); // Mount it now
    if(rslt == FR_OK) App.DrawNextBmp();
    else Uart.Printf("FS mount error: %u\r", rslt);

    PinSensors.Init();
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
#if 1 // ==== USB ====
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
        if(EvtMsk & EVTMSK_BTN_PRESS) {
            FRESULT rslt = FR_OK;
            // Try to mount FS again if not mounted
            if(!FATFS_IS_OK()) {
                rslt = f_mount(&FatFS, "", 1); // Mount it now
            }
            if(rslt == FR_OK) App.DrawNextBmp();
            else Uart.Printf("FS mount error: %u\r", rslt);
        } // EVTMSK_BTN_PRESS
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

uint8_t App_t::DrawNextBmp() {
    uint8_t rslt = f_findnext(&Dir, &FileInfo);
    if(rslt == FR_OK and FileInfo.fname[0]) goto lbl_Found;
    else {  // Not found, or dir closed
        f_closedir(&Dir);
        rslt = f_findfirst(&Dir, &FileInfo, "", Extension);
        if(rslt == FR_OK and FileInfo.fname[0]) goto lbl_Found;
        else {
            Uart.Printf("Not found: %u\r", rslt);
            return FAILURE;
        }
    } // findnext not succeded
    lbl_Found:
    Uart.PrintfNow("%S\r", FileInfo.fname);
    Lcd.DrawBmpFile(0,0, FileInfo.fname, &File);
    return OK;
}

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
