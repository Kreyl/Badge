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
#include "buttons.h"
#include "kl_adc.h"
#include "battery_consts.h"

App_t App;
TmrVirtual_t TmrMeasurement;

#define IsCharging()    (!PinIsSet(BAT_CHARGE_GPIO, BAT_CHARGE_PIN))

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

    // Battery: ADC
    Adc.Init();
    PinSetupAnalog(BAT_MEAS_GPIO, BAT_MEAS_PIN);
    PinSetupOut(BAT_SW_GPIO, BAT_SW_PIN, omOpenDrain, pudNone);
    PinSet(BAT_SW_GPIO, BAT_SW_PIN);
    PinSetupIn(BAT_CHARGE_GPIO, BAT_CHARGE_PIN, pudPullUp);

    // Measure battery prior to any operation
    App.OnAdcSamplingTime();
    chEvtWaitAny(EVTMSK_ADC_DONE);  // Wait AdcDone
    // Discard first measurement and restart measurement
    App.OnAdcSamplingTime();
    chEvtWaitAny(EVTMSK_ADC_DONE);  // Wait AdcDone
    App.OnAdcDone();

    // Proceed with init
    Lcd.Init();
    Lcd.SetBrightness(100);

    Mem.Init();
    UsbMsd.Init();

    // ==== FAT init ====
    FRESULT rslt = f_mount(&FatFS, "", 1); // Mount it now
    if(rslt == FR_OK) App.DrawNextBmp();
    else Uart.Printf("FS mount error: %u\r", rslt);

    PinSensors.Init();
    TmrMeasurement.InitAndStart(chThdGetSelfX(), MS2ST(MEASUREMENT_PERIOD_MS), EVTMSK_SAMPLING, tvtPeriodic);
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
            chSysUnlock();
            Clk.PrintFreqs();
            if(r == OK) {
                Clk.SelectUSBClock_HSI48();
                Clk.EnableCRS();
                UsbMsd.Connect();
            }
            else Uart.Printf("Hsi48 Fail\r");
        }

        if(EvtMsk & EVTMSK_USB_DISCONNECTED) {
            Uart.Printf("5v off\r");
            // Disable Usb & HSI48
            UsbMsd.Disconnect();
            chSysLock();
            uint8_t r = Clk.SwitchTo(csHSI);
            Clk.UpdateFreqValues();
            chSysUnlock();
            Clk.PrintFreqs();
            if(r == OK) {
                Clk.DisableCRS();
                Clk.DisableHSI48();
                Uart.Printf("cr2=%X\r", RCC->CR2);
            }
            else Uart.Printf("Hsi Fail\r");
        }

        if(EvtMsk & EVTMSK_USB_READY) {
            Uart.Printf("UsbReady\r");
        }
#endif

        if(EvtMsk & EVTMSK_BUTTONS) {
            BtnEvtInfo_t EInfo;
            while(BtnGetEvt(&EInfo) == OK) {
                if(EInfo.Type == bePress) {
                    Uart.Printf("Btn\r");
                    FRESULT rslt = FR_OK;
                    // Try to mount FS again if not mounted
                    if(!FATFS_IS_OK()) rslt = f_mount(&FatFS, "", 1); // Mount it now
                    if(rslt == FR_OK) App.DrawNextBmp();
                    else {
                        Uart.Printf("FS mount error: %u\r", rslt);
                        if(IsDisplayingBattery) {
                            IsDisplayingBattery = false;
                            Lcd.DrawNoImage();
                        }
                        else {
                            Lcd.DrawBattery(BatteryPercent, (IsCharging()? bstCharging : bstDischarging), lhpHide);
                            IsDisplayingBattery = true;
                        }
                    } // FS error
                } // if press
                else if(EInfo.Type == beLongPress) {
                    Shutdown();
                }
            } // while getinfo ok
        } // EVTMSK_BTN_PRESS

        // ==== ADC ====
        if(EvtMsk & EVTMSK_SAMPLING) OnAdcSamplingTime();
        if(EvtMsk & EVTMSK_ADC_DONE) OnAdcDone();
    } // while true
}

void App_t::OnAdcSamplingTime() {
    Adc.EnableVRef();
    PinClear(BAT_SW_GPIO, BAT_SW_PIN);  // Connect R divider to GND
    chThdSleepMicroseconds(450);
    Adc.StartMeasurement();
}

void App_t::OnAdcDone() {
    PinSet(BAT_SW_GPIO, BAT_SW_PIN);    // Disconnect R divider to GND
    Adc.DisableVRef();
    uint32_t BatAdc = 2 * Adc.GetResult(BAT_CHNL); // to count R divider
    uint32_t VRef = Adc.GetResult(ADC_VREFINT_CHNL);
    uint32_t BatVoltage = Adc.Adc2mV(BatAdc, VRef);
    BatteryPercent = mV2Percent(BatVoltage);
//    Uart.Printf("mV=%u; percent=%u\r", BatVoltage, BatteryPercent);

    // If not charging: if voltage is too low - display discharged battery and shutdown
//    if(!IsCharging()) {
//        if(BatVoltage < BAT_ZERO_mV) {
//            Lcd.DrawBattery(BatteryPercent, bstDischarging);
//            chThdSleepMilliseconds(1800);
//            Shutdown();
//        }
//    } // if not charging
    // Redraw battery charge
    if(IsDisplayingBattery) Lcd.DrawBattery(BatteryPercent, (IsCharging()? bstCharging : bstDischarging), lhpDoNotHide);
}

void App_t::Shutdown() {
    Uart.PrintfNow("Shutdown\r");
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

void App_t::DrawNextBmp() {
    uint8_t rslt = f_findnext(&Dir, &FileInfo);
    if(rslt == FR_OK and FileInfo.fname[0]) goto lbl_Found;
    else {  // Not found, or dir closed
        // Display battery if not displayed yet
        if(IsDisplayingBattery) {
            IsDisplayingBattery = false;
            // Reread dir
            f_closedir(&Dir);
            rslt = f_findfirst(&Dir, &FileInfo, "", Extension);
            if(rslt == FR_OK and FileInfo.fname[0]) goto lbl_Found;
            else {
                Lcd.DrawNoImage();
                Uart.Printf("Not found: %u\r", rslt);
                f_closedir(&Dir);
                FatFS.fs_type = 0;  // Unmount FS
                return;
            }
        }
        else {
            Lcd.DrawBattery(BatteryPercent, (IsCharging()? bstCharging : bstDischarging), lhpHide);
            IsDisplayingBattery = true;
            return;
        }
    } // findnext not succeded
    lbl_Found:
    Uart.PrintfNow("%S\r", FileInfo.fname);
    Lcd.DrawBmpFile(0,0, FileInfo.fname, &File);
}

// 5v Sns
void Process5VSns(PinSnsState_t *PState, uint32_t Len) {
    if(PState[0] == pssRising) App.SignalEvt(EVTMSK_USB_CONNECTED);
    else if(PState[0] == pssFalling) App.SignalEvt(EVTMSK_USB_DISCONNECTED);
}

