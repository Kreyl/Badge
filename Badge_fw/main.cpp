#include "descriptors_msd.h"
#include "usb_msd.h"
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

#define IsCharging()        (!PinIsSet(BAT_CHARGE_GPIO, BAT_CHARGE_PIN))
#define ButtonIsPressed()   (PinIsSet(BTN_GPIO, BTN_PIN))

#define AHB_DIVIDER ahbDiv2

int main(void) {
    // ==== Setup clock frequency ====
    Clk.EnablePrefetch();
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
    chEvtWaitAny(EVT_ADC_DONE);  // Wait AdcDone
    // Discard first measurement and restart measurement
    App.OnAdcSamplingTime();
    chEvtWaitAny(EVT_ADC_DONE);      // Wait AdcDone
    App.IsDisplayingBattery = true;     // Do not draw battery now
    App.OnAdcDone(lhpHide);             // Will draw battery and shutdown if discharged
    // Will proceed with init if not shutdown
    Lcd.SetBrightness(100);

    Mem.Init();

    UsbMsd.Init();

    // ==== FAT init ====
    // DMA-based MemCpy init
    dmaStreamAllocate(STM32_DMA1_STREAM3, IRQ_PRIO_LOW, NULL, NULL);
//    if(TryInitFS() == OK) Lcd.DrawBmpFile(0,0, "play_russian.bmp", &File);

    PinSensors.Init();
    TmrMeasurement.InitAndStart(chThdGetSelfX(), MS2ST(MEASUREMENT_PERIOD_MS), EVT_SAMPLING, tktPeriodic);
    // Main cycle
    App.ITask();
}

__noreturn
void App_t::ITask() {
    while(true) {
        __unused uint32_t EvtMsk = chEvtWaitAny(ALL_EVENTS);
#if 1 // ==== USB ====
        if(EvtMsk & EVT_USB_CONNECTED) {
            Uart.Printf("5v is here\r");
            Is5VConnected = true;
            // Signal battery charge status changed
            BatteryPercent = 255;
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

        if(EvtMsk & EVT_USB_DISCONNECTED) {
            Uart.Printf("5v off\r");
            Is5VConnected = false;
            // Signal battery charge status changed
            BatteryPercent = 255;
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
//                Uart.Printf("cr2=%X\r", RCC->CR2);
            }
            else Uart.Printf("Hsi Fail\r");
        }

        if(EvtMsk & EVT_USB_READY) {
            Uart.Printf("UsbReady\r");
        }
#endif
#if 1 // ==== Button ====
        if(EvtMsk & EVT_BUTTONS) {
            BtnEvtInfo_t EInfo;
            while(BtnGetEvt(&EInfo) == OK) {
                if(EInfo.Type == bePress) OnBtnPress();
                else if(EInfo.Type == beLongPress and !Is5VConnected) Shutdown();
            } // while getinfo ok
        } // EVTMSK_BTN_PRESS
#endif
#if 1   // ==== ADC ====
        if(EvtMsk & EVT_SAMPLING) OnAdcSamplingTime();
        if(EvtMsk & EVT_ADC_DONE) OnAdcDone(lhpDoNotHide);
#endif

#if 1 // ==== ImgList ====
        if(EvtMsk & EVT_IMGLIST_TIME) ImgList.OnTime();
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
    UsbMsd.Disconnect();
    Lcd.Shutdown();
    Mem.PowerDown();
    // Wait until button depressed
    while(ButtonIsPressed()) chThdSleepMilliseconds(27);
    chThdSleepMilliseconds(999);
    Sleep::EnableWakeup1Pin();
    Sleep::EnterStandby();
}

#if 1 // ============================ Image search etc =========================
#define IMG_SEARCH_DEBUG    FALSE

uint8_t GetNextImg() {
    while(true) {
        uint8_t rslt = f_readdir(&Dir, &FileInfo);  // Get item in dir
        if(rslt == FR_OK and FileInfo.fname[0]) {   // Something found
#if IMG_SEARCH_DEBUG
            Uart.Printf("1> %S; %S; attrib=%X\r", FileInfo.fname, FileInfo.lfname, FileInfo.fattrib);
#endif
            if(FileInfo.fattrib & (AM_HID | AM_DIR)) continue; // Ignore hidden files and dirs
            else {
                uint32_t Len = strlen(FileInfo.fname);
                if(Len >= 5) {
                    char *S = &FileInfo.fname[Len-4];
                    if(strcasecmp(S, ".BMP") == 0) return OK;
                } // if len > 5
            }
        }
        else return FAILURE;
    }
}

// Single-level structure only
enum Action_t {IteratingImgs, IteratingDirs};
static Action_t Action = IteratingImgs;
static char DirName[13] = { 0 };
static bool CurrentDirIsRoot = true;
static bool StartOver = true;

uint8_t GetNextDir() {
    bool OldDirFound = false;
    while(true) {
        uint8_t rslt = f_readdir(&Dir, &FileInfo);
        if(rslt == FR_OK and FileInfo.fname[0]) {   // File found
#if IMG_SEARCH_DEBUG
            Uart.Printf("2> %S; attrib=%X\r", FileInfo.fname, FileInfo.fattrib);
#endif
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
        else {
#if IMG_SEARCH_DEBUG
            Uart.Printf("2> ReadDir Err=%u\r", rslt);
#endif
            return FAILURE;
        }
    } // while true
}

void App_t::DrawNext() {
    uint8_t rslt;
    bool WrapAround = false;
    if(ImgList.IsActive()) {
        ImgList.Stop();
        StartOver = true;
    }

    while(true) {
        if(StartOver) {
#if IMG_SEARCH_DEBUG
            Uart.Printf("StartOver\r");
#endif
            f_mount(&FatFS, "", 1); // Reread filesystem
            f_opendir(&Dir, "/");   // Open root dir
            f_chdir("/");
            StartOver = false;
            CurrentDirIsRoot = true;
        }

        if(Action == IteratingImgs) {
#if IMG_SEARCH_DEBUG
            Uart.Printf("IImg\r");
#endif
            rslt = GetNextImg();
            if(rslt == OK) {
#if IMG_SEARCH_DEBUG
                Uart.Printf("GotImg\r");
#endif
                if(Lcd.DrawBmpFile(0,0, FileInfo.fname, &File) == OK) {
                    IsDisplayingBattery = false;
                    break;
                }
            }
            else { // No more image found
#if IMG_SEARCH_DEBUG
                Uart.Printf("NoImg\r");
#endif
                f_closedir(&Dir);
                Action = IteratingDirs;
                if(CurrentDirIsRoot) *DirName = '\0';  // Start searching subdirs from beginning
                StartOver = true;   // Anyway, open root dir
            }
        }
        // Iterating dirs
        else {
#if IMG_SEARCH_DEBUG
            Uart.Printf("IDir\r");
#endif
            rslt = GetNextDir();
            if(rslt == OK) {
#if IMG_SEARCH_DEBUG
                Uart.Printf("GotDir\r");
#endif
                f_opendir(&Dir, DirName);
                f_chdir(DirName);
                CurrentDirIsRoot = false;
                // Check if config presents
                if(ImgList.TryToConfig() == OK) {
                    ImgList.Start();
                    break;
                }
                else Action = IteratingImgs;
            }
            // No dir, end of root
            else {
#if IMG_SEARCH_DEBUG
                Uart.Printf("NoDir\r");
#endif
                if(IsDisplayingBattery) {
                    if(WrapAround) {    // No good files
                        Lcd.DrawNoImage();
                        IsDisplayingBattery = false;
                        break;
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
                    break;
                }
            } // No dir, end of root
        } // Iterating dirs
    } // while true
}
#endif

// 5v Sns
void Process5VSns(PinSnsState_t *PState, uint32_t Len) {
    if(PState[0] == pssRising) App.SignalEvt(EVT_USB_CONNECTED);
    else if(PState[0] == pssFalling) App.SignalEvt(EVT_USB_DISCONNECTED);
}
