/*
 * main.cpp
 *
 *  Created on: 20 февр. 2014 г.
 *      Author: g.kruglov
 */

#include <descriptors_msd.h>
#include <usb_msd.h>
#include "main.h"
#include "SimpleSensors.h"
#include "FlashW25Q64t.h"
#include "kl_fs_common.h"
#include "buttons.h"
#include "kl_adc.h"
#include "battery_consts.h"
#include "ImgList.h"

#include "pics.h"

App_t App;
TmrKL_t TmrMeasurement;
ImgList_t ImgList;

#define IsCharging()    (!PinIsSet(BAT_CHARGE_GPIO, BAT_CHARGE_PIN))

// Extension of graphic files to search
const char Extension[] = "*.bmp";

#define AHB_DIVIDER ahbDiv8

int main(void) {
    // ==== Setup clock frequency ====
//    Clk.EnablePrefetch();
    Clk.SetupBusDividers(AHB_DIVIDER, apbDiv1);
    Clk.UpdateFreqValues();

    // Init OS
    halInit();
    chSysInit();
    App.InitThread();

    // ==== Init hardware ====
    Uart.Init(115200, UART_GPIO, UART_TX_PIN);
    Uart.Printf("\r%S %S\r", APP_NAME, BUILD_TIME);
    Clk.PrintFreqs();

    // Battery: ADC and charging
    Adc.Init();
    PinSetupAnalog(BAT_MEAS_GPIO, BAT_MEAS_PIN);
    PinSetupOut(BAT_SW_GPIO, BAT_SW_PIN, omOpenDrain, pudNone);
    PinSet(BAT_SW_GPIO, BAT_SW_PIN);
    PinSetupIn(BAT_CHARGE_GPIO, BAT_CHARGE_PIN, pudPullUp);

    Lcd.Init();
    Lcd.SetBrightness(20);

    // Measure battery prior to any operation
    App.OnAdcSamplingTime();
    chEvtWaitAny(EVTMSK_ADC_DONE);  // Wait AdcDone
    // Discard first measurement and restart measurement
    App.OnAdcSamplingTime();
    chEvtWaitAny(EVTMSK_ADC_DONE);      // Wait AdcDone
    App.IsDisplayingBattery = true;     // Do not draw battery now
    App.OnAdcDone(lhpHide);             // Will draw battery and shutdown if discharged
    // Will proceed with init if not shutdown
    Lcd.SetBrightness(100);

    Mem.Init();

    UsbMsd.Init();

    // ==== FAT init ====
    // DMA-based MemCpy init
    dmaStreamAllocate(STM32_DMA1_STREAM3, IRQ_PRIO_LOW, NULL, NULL);
//    if(TryInitFS() == OK) App.DrawNextBmp();

    PinSensors.Init();
    TmrMeasurement.InitAndStart(chThdGetSelfX(), MS2ST(MEASUREMENT_PERIOD_MS), EVTMSK_SAMPLING, tktPeriodic);
    // Main cycle
    App.ITask();
}

//if(ImgList.TryToConfig("config.ini") == OK) ImgList.Start();

__noreturn
void App_t::ITask() {
    while(true) {
        __unused uint32_t EvtMsk = chEvtWaitAny(ALL_EVENTS);
#if 0 // ==== USB ====
        if(EvtMsk & EVTMSK_USB_CONNECTED) {
            Uart.Printf("5v is here\r");
            chThdSleepMilliseconds(270);
            // Enable HSI48
            chSysLock();
            Clk.SetupBusDividers(ahbDiv2, apbDiv1);
            while(Clk.SwitchTo(csHSI48) != OK) {
                Uart.PrintfI("Hsi48 Fail\r");
                chThdSleepS(MS2ST(207));
            }
            Clk.UpdateFreqValues();
            chSysUnlock();
            Clk.PrintFreqs();
            Clk.SelectUSBClock_HSI48();
            Clk.EnableCRS();
            UsbMsd.Connect();
        }

        if(EvtMsk & EVTMSK_USB_DISCONNECTED) {
            Uart.Printf("5v off\r");
            // Disable Usb & HSI48
            UsbMsd.Disconnect();
            chSysLock();
            Clk.SetupBusDividers(AHB_DIVIDER, apbDiv1);
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
#if 1 // ==== Button ====
        if(EvtMsk & EVTMSK_BUTTONS) {
            BtnEvtInfo_t EInfo;
            while(BtnGetEvt(&EInfo) == OK) {
                if(EInfo.Type == bePress) OnBtnPress();
//                else if(EInfo.Type == beLongPress) Shutdown();
            } // while getinfo ok
        } // EVTMSK_BTN_PRESS
#endif
#if 1   // ==== ADC ====
        if(EvtMsk & EVTMSK_SAMPLING) OnAdcSamplingTime();
        if(EvtMsk & EVTMSK_ADC_DONE) OnAdcDone(lhpDoNotHide);
#endif

#if 1 // ==== ImgList ====
        if(EvtMsk & EVTMSK_IMGLIST_TIME) ImgList.OnTime();
#endif
    } // while true
}

void App_t::OnBtnPress() {
    // Uart.Printf("Btn\r");
    // Try to mount FS again if not mounted
    if(FATFS_IS_OK()) DrawNext();
    else {
        if(TryInitFS() == OK) DrawNext();
        else {
            if(IsDisplayingBattery) {
                IsDisplayingBattery = false;
                Lcd.DrawNoImage();
            }
            else {
                Lcd.DrawBattery(BatteryPercent, (IsCharging()? bstCharging : bstDischarging), lhpHide);
                IsDisplayingBattery = true;
            }
        } // FS error
    } // if FAT ok
}

void App_t::OnAdcSamplingTime() {
    Adc.EnableVRef();
    PinClear(BAT_SW_GPIO, BAT_SW_PIN);  // Connect R divider to GND
    chThdSleepMicroseconds(99);
    Adc.StartMeasurement();
}

void App_t::OnAdcDone(LcdHideProcess_t Hide) {
    PinSet(BAT_SW_GPIO, BAT_SW_PIN);    // Disconnect R divider to GND
    Adc.DisableVRef();
    uint32_t BatAdc = 2 * Adc.GetResult(BAT_CHNL); // to count R divider
    uint32_t VRef = Adc.GetResult(ADC_VREFINT_CHNL);
    uint32_t BatVoltage = Adc.Adc2mV(BatAdc, VRef);
    uint8_t NewBatPercent = mV2Percent(BatVoltage);
//    Uart.Printf("mV=%u; percent=%u\r", BatVoltage, NewBatPercent);

    // If not charging: if voltage is too low - display discharged battery and shutdown
    if(!IsCharging() and (BatVoltage < BAT_ZERO_mV)) {
        Uart.Printf("LowBat\r");
        Lcd.DrawBattery(NewBatPercent, bstDischarging, lhpHide);
        chThdSleepMilliseconds(1800);
        Shutdown();
    } // if not charging
    // Redraw battery charge if displaying battery now
    if(IsDisplayingBattery and NewBatPercent != BatteryPercent) { // Redraw if changed
        Lcd.DrawBattery(NewBatPercent, (IsCharging()? bstCharging : bstDischarging), Hide);
    }
    BatteryPercent = NewBatPercent;
}

void App_t::Shutdown() {
    Uart.PrintfNow("Shutdown\r");
    Lcd.Shutdown();
    Mem.PowerDown();
    Sleep::EnableWakeup1Pin();
    Sleep::EnterStandby();
}

#if UART_RX_ENABLED // ======================= Command processing ============================
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

uint8_t GetNextImg() {
    while(true) {
        uint8_t rslt = f_readdir(&Dir, &FileInfo);  // Get item in dir
        if(rslt == FR_OK and FileInfo.fname[0]) {   // Something found
            Uart.Printf("1> %S; attrib=%X\r", FileInfo.fname, FileInfo.fattrib);
            if(FileInfo.fattrib & (AM_HID | AM_DIR)) continue; // Ignore hidden files and dirs
            else {
                if(strstr(FileInfo.fname, ".BMP") != nullptr) return OK;
            }
        }
        else return FAILURE;
    }
}

// Single-level structure only
enum Action_t {IteratingImgs, IteratingDirs};
Action_t Action = IteratingImgs;
char DirName[13] = { 0 };
bool CurrentDirIsRoot = true;
bool StartOver = true;

uint8_t GetNextDir() {
    bool OldDirFound = false;
    while(true) {
        uint8_t rslt = f_readdir(&Dir, &FileInfo);
        if(rslt == FR_OK and FileInfo.fname[0]) {   // File found
            Uart.Printf("2> %S; attrib=%X\r", FileInfo.fname, FileInfo.fattrib);
            if(FileInfo.fattrib & AM_HID) continue;
            if(FileInfo.fattrib & AM_DIR) {
                if(*DirName == '\0' or OldDirFound) {
                    strcpy(DirName, FileInfo.fname);
                    return OK;
                }
                else {
                    if(strcasecmp(FileInfo.fname, DirName) == 0) OldDirFound = true; // Take next dir after that
                }
            }
        } // if rslt ok
        else return FAILURE;
    } // while true
}


void App_t::DrawNext() {
    uint8_t rslt;
    bool WrapAround = false;
    ImgList.Stop();

    while(true) {
        Uart.Printf("A\r");
        if(StartOver) {
            Uart.Printf("StartOver\r");
            f_opendir(&Dir, "/");   // Open root dir
            f_chdir("/");
            StartOver = false;
            CurrentDirIsRoot = true;
        }

        if(Action == IteratingImgs) {
            Uart.Printf("II\r");
            rslt = GetNextImg();
            if(rslt == OK) {
                Uart.Printf("GotImg\r");
                if(Lcd.DrawBmpFile(0,0, FileInfo.fname, &File) == OK) {
                    IsDisplayingBattery = false;
                    break;
                }
            }
            else { // No more image found
                Uart.Printf("NoImg\r");
                f_closedir(&Dir);
                Action = IteratingDirs;
                if(CurrentDirIsRoot) *DirName = '\0';  // Start searching subdirs from beginning
                StartOver = true;   // Anyway, open root dir
            }
        }
        // Iterating dirs
        else {
            Uart.Printf("IDir\r");
            rslt = GetNextDir();
            if(rslt == OK) {
                Uart.Printf("GotDir\r");
                f_opendir(&Dir, DirName);
                f_chdir(DirName);
                CurrentDirIsRoot = false;
                Action = IteratingImgs;
            }
            // No dir, end of root
            else {
                Uart.Printf("NoDir\r");
                if(IsDisplayingBattery) {
                    if(WrapAround) {    // No good files
                        Lcd.DrawNoImage();
                        IsDisplayingBattery = false;
                        return;
                    }
                    else { // Start over
                        WrapAround = true;
                        StartOver = true;
                        Action = IteratingImgs;
                    }
                }
                else { // Display battery
                    Lcd.DrawBattery(BatteryPercent, (IsCharging()? bstCharging : bstDischarging), lhpHide);
                    IsDisplayingBattery = true;
                    return;
                }
            } // No dir, end of root
        } // Iterating dirs
    } // while true

//    bool WrapAround = false;
//    while(true) {
//        rslt = f_findnext(&Dir, &FileInfo);
//        if(rslt == FR_OK and FileInfo.fname[0]) {   // File found
//            Uart.Printf("1> %S; attrib=%X\r", FileInfo.fname, FileInfo.fattrib);
//            if(FileInfo.fattrib & AM_HID) continue; // Ignore hidden files
//            else goto lbl_Found;                    // Correct file found
//        }
//        else { // Not found, or dir closed
//            // Display battery if not displayed yet
//            if(IsDisplayingBattery) {
//                f_closedir(&Dir);
//                if(WrapAround) {    // Dir does not contain good files
//                    Lcd.DrawNoImage();
//                    IsDisplayingBattery = false;
//                    return;
//                }
//                // Reread dir
//                rslt = f_findfirst(&Dir, &FileInfo, "", Extension);
//                WrapAround = true;
//                if(rslt == FR_OK and FileInfo.fname[0]) {   // File found
//                    Uart.Printf("2> %S; attrib=%X\r", FileInfo.fname, FileInfo.fattrib);
//                    if(FileInfo.fattrib & AM_HID) continue; // Ignore hidden files
//                    else goto lbl_Found;                    // Correct file found
//                }
//            } // if(IsDisplayingBattery)
//            else {
//                Lcd.DrawBattery(BatteryPercent, (IsCharging()? bstCharging : bstDischarging), lhpHide);
//                IsDisplayingBattery = true;
//                return;
//            }
//        } // not found
//    } // while true
//    lbl_Found:
//    IsDisplayingBattery = false;
//    Uart.PrintfNow("%S\r", FileInfo.fname);
//    Lcd.DrawBmpFile(0, 0, FileInfo.fname, &File);
}

// 5v Sns
void Process5VSns(PinSnsState_t *PState, uint32_t Len) {
    if(PState[0] == pssRising) App.SignalEvt(EVTMSK_USB_CONNECTED);
    else if(PState[0] == pssFalling) App.SignalEvt(EVTMSK_USB_DISCONNECTED);
}
