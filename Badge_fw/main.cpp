/*
 * main.cpp
 *
 *  Created on: 20 февр. 2014 г.
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

    Lcd.Init();
    Lcd.SetBrightness(100);

    Mem.Init();
    UsbMsd.Init();

    // FAT init
    FRESULT r = f_mount(&FatFS, "", 1); // Mount it now
    if(r != FR_OK) Uart.Printf("FS mount error: %u\r", r);
//    else Lcd.DrawBmpFile(0,0, "badge2016.bmp", &File);
    else Lcd.DrawBmpFile(0,0, "sign2.bmp", &File);

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
            if(rslt == FR_OK) {
                // Search next bmp

//                rslt = f_open(&File, "readme.txt", FA_READ);
//                if(rslt == FR_OK) {
//                    uint32_t BytesRead=0;
//                    rslt = f_read(&File, Buf, 255, &BytesRead);
//                    if(rslt == FR_OK) {
//                        Buf[BytesRead] = 0;
//                        Uart.Printf("> %S\r", Buf);
//                    }
//                    else Uart.Printf("Read error: %u\r", rslt);
//                    f_close(&File);
//                } // open OK
//                else Uart.Printf("Open error: %u\r", rslt);
            }
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
