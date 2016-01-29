/*
 * main.cpp
 *
 *  Created on: 20 февр. 2014 г.
 *      Author: g.kruglov
 */

#include "main.h"
#include "lcd_round.h"
//#include "SimpleSensors.h"
#include "FlashW25Q64t.h"

App_t App;

uint8_t Buf[4096];

uint8_t WBuf[16] = {100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115};

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

    Mem.Read(0, Buf, 16);
    for(uint32_t i=0; i<16; i++) {
        Uart.Printf("%X ", Buf[i]);
    }
    Uart.Printf("\r");

    Mem.EraseBlock4k(0);

    Mem.WritePage(0, WBuf, 16);

    Mem.Read(0, Buf, 16);
    for(uint32_t i=0; i<16; i++) {
        Uart.Printf("%X ", Buf[i]);
    }
    Uart.Printf("\r");

//    PinSetupOut(GPIOB, 15, omPushPull);
//    PinClear(GPIOB, 15);


//    PinSensors.Init();

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
