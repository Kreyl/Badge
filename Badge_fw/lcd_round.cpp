#include "lcd_round.h"
#include "kl_lib.h"
#include <string.h>
#include <stdarg.h>
#include "core_cmInstr.h"
#include "uart.h"
#include "main.h"
#include "pics.h"
#include "kl_fs_common.h"

//#include "lcdFont8x8.h"
//#include <string.h>

// Variables
Lcd_t Lcd;

struct RegData_t {
    uint16_t Reg, Data;
};
const RegData_t InitData[] = {
//        {0xA4, 0x0001}, // NVM calibration
//        {0x60, 0x2700},  // (def) Driver output control: number of lines = 320, gate start = 0
//        {0x08, 0x0808},  // (def) front and back porch period
        // Gamma correction
 /*       {0x30, 0x0214},
        {0x31, 0x3715},
        {0x32, 0x0604},
        {0x33, 0x0E16},
        {0x34, 0x2211},
        {0x35, 0x1500},
        {0x36, 0x8507},
        {0x37, 0x1407},
        {0x38, 0x1403},
        {0x39, 0x0020},*/

//        {0x90, 0x0015},  // (def 0x0111) division 1/1, line period 21clk
//        {0x10, 0x0410},
//        {0x11, 0x0237},

        {0x29, 0x0046},
        {0x2A, 0x0046},
        {0x07, 0x0000},
        {0x12, 0x0189},
        {0x13, 0x1100},

        {0x12, 0x01B9},
        {0x01, 0x0100},
        {0x02, 0x0200},
        {0x03, 0x12B0},  // HWM=1, ORG=1, ID=11=>origin top left
        {0x09, 0x0001},
        {0x0A, 0x0000},
        {0x0D, 0x0000},
        {0x0E, 0x0030},
        {0x50, 0x0000},
        {0x51, 0x00EF},
        {0x52, 0x0000},
        {0x53, 0x013F},
        {0x61, 0x0001},
        {0x6A, 0x0000},
        {0x80, 0x0000},
        {0x81, 0x0000},
        {0x82, 0x005F},
        {0x92, 0x0100},
        {0x93, 0x0701},
        {0x07, 0x0100},
};
#define INIT_SEQ_CNT    countof(InitData)

#if 1 // ==== Pin driving functions ====
#define RstHi() { PinSet(LCD_GPIO, LCD_RST);   }
#define RstLo() { PinClear(LCD_GPIO, LCD_RST); }
#define CsHi()  { PinSet(LCD_GPIO, LCD_CS);    }
#define CsLo()  { PinClear(LCD_GPIO, LCD_CS);  }
#define RsHi()  { PinSet(LCD_GPIO, LCD_RS);    }
#define RsLo()  { PinClear(LCD_GPIO, LCD_RS);  }
#define WrHi()  { PinSet(LCD_GPIO, LCD_WR);    }
#define RdHi()  { PinSet(LCD_GPIO, LCD_RD);    }
#define RdLo()  { PinClear(LCD_GPIO, LCD_RD);  }

#define SetReadMode()   LCD_GPIO->MODER &= LCD_MODE_MSK_READ
#define SetWriteMode()  LCD_GPIO->MODER |= LCD_MODE_MSK_WRITE

// Bus operations
__attribute__ ((always_inline)) static inline
void WriteByte(uint8_t Byte) {
        LCD_GPIO->BRR  = (0xFF | (1<<LCD_WR));  // Clear bus and set WR low
        LCD_GPIO->BSRR = Byte;                  // Place data on bus
        LCD_GPIO->BSRR = (1<<LCD_WR);           // WR high
}

__RAMFUNC
void WriteBuf(uint8_t *PBuf, uint32_t Len) {
    for(uint32_t i=0; i<Len; i+=2) {
        WriteByte(PBuf[i+1]);
        WriteByte(PBuf[i]);
    }
}
#endif

void Lcd_t::Init() {
    // Backlight
    Led1.Init();
    Led2.Init();
    // Pins
    PinSetupOut(LCD_GPIO, LCD_RST, omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_CS,  omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_RS,  omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_WR,  omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_RD,  omPushPull, pudNone, psMedium);
    PinSetupAnalog(LCD_GPIO, LCD_IMO);
    // Configure data bus as outputs
    for(uint8_t i=0; i<8; i++) PinSetupOut(LCD_GPIO, i, omPushPull, pudNone, psHigh);

    // ======= Init LCD =======
    // Initial signals
    RstHi();
    CsHi();
    RdHi();
    WrHi();
    RsHi();
    // Reset LCD
    RstLo();
    chThdSleepMilliseconds(54);
    RstHi();
    CsLo(); // Stay selected forever
    // Let it to wake up
    chThdSleepMilliseconds(306);
    // p.107 "Make sure to execute data transfer synchronization after reset operation before transferring instruction"
    RsLo();
    WriteByte(0x00);
    WriteByte(0x00);
    WriteByte(0x00);
    WriteByte(0x00);
    RsHi();
    // Read ID
//    uint16_t r = ReadReg(0x00);
//    Uart.Printf("rslt=%X\r", r);
    // Send init Cmds
    for(uint32_t i=0; i<INIT_SEQ_CNT; i++) {
        WriteReg(InitData[i].Reg, InitData[i].Data);
//        Uart.Printf("%X %X\r", InitData[i].Reg, InitData[i].Data);
    }
//    Cls(clBlack);
}

void Lcd_t::Shutdown(void) {
    Led1.Deinit();
    Led2.Deinit();
    WriteReg(0x07, 0x0072);
    chThdSleepMicroseconds(450);
    WriteReg(0x07, 0x0001);
    chThdSleepMicroseconds(450);
    WriteReg(0x07, 0x0000);
    chThdSleepMicroseconds(450);
    WriteReg(0x12, 0);  // PON=0, PSON=0
    WriteReg(0x10, 0x0004);  // enter deepsleep
}

void Lcd_t::SetBrightness(uint16_t ABrightness) {
    Led1.Set(ABrightness);
    Led2.Set(ABrightness);
    IBrightness = ABrightness;
}

#if 1 // ============================ Local use ================================
void Lcd_t::WriteReg(uint8_t AReg, uint16_t AData) {
    // Write register addr
    RsLo();
    WriteByte(0);   // No addr > 0xFF
    WriteByte(AReg);
    RsHi();
    // Write data
    WriteByte((uint8_t)(AData >> 8));
    WriteByte((uint8_t)(AData & 0xFF));
}

uint16_t Lcd_t::ReadReg(uint8_t AReg) {
    // Write register addr
    RsLo();
    WriteByte(0);
    WriteByte(AReg);
    RsHi();
    // Read data
    SetReadMode();
    RdLo();
    uint16_t rUp = LCD_GPIO->IDR;
    RdHi();
    rUp <<= 8;
    RdLo();
    uint16_t rLo = LCD_GPIO->IDR;
    RdHi();
    rLo &= 0x00FF;
    SetWriteMode();
    return (rUp | rLo);
}

void Lcd_t::PrepareToWriteGRAM() {  // Write RegID = 0x22
    RsLo();
    WriteByte(0);
    WriteByte(0x22);
    RsHi();
}
#endif

#if 1 // ============================= Graphics ================================
void Lcd_t::GoTo(uint16_t x, uint16_t y) {
    WriteReg(0x20, x);     // GRAM Address Set (Horizontal Address) (R20h)
    WriteReg(0x21, y);     // GRAM Address Set (Vertical Address) (R21h)
}

void Lcd_t::SetBounds(uint16_t Left, uint16_t Width, uint16_t Top, uint16_t Height) {
    uint16_t XEndAddr = LCD_X_0 + Left + Width  - 1;
    uint16_t YEndAddr = Top  + Height - 1;
    if(XEndAddr > 239) XEndAddr = 239;
    if(YEndAddr > 319) YEndAddr = 319;
    // Write bounds
    WriteReg(0x50, LCD_X_0 + Left);
    WriteReg(0x51, XEndAddr);
    WriteReg(0x52, Top);
    WriteReg(0x53, YEndAddr);
    // Move origin to zero: required if ORG==1
    WriteReg(0x20, 0);
    WriteReg(0x21, 0);
}

void Lcd_t::Cls(Color_t Color) {
    SetBounds(0, LCD_W, 0, LCD_H);
    // Prepare variables
    uint8_t ClrUpper = Color.RGBTo565_HiByte();
    uint8_t ClrLower = Color.RGBTo565_LoByte();
    // Fill LCD
    PrepareToWriteGRAM();
    for(uint32_t i=0; i<(LCD_H * LCD_W); i++) {
        WriteByte(ClrUpper);
        WriteByte(ClrLower);
    }
}

void Lcd_t::PutBitmap(uint16_t x0, uint16_t y0, uint16_t Width, uint16_t Height, uint16_t *PBuf) {
    //Uart.Printf("%u %u %u %u %u\r", x0, y0, Width, Height, *PBuf);
    SetBounds(x0, Width, y0, Height);
    // Prepare variables
    Convert::WordBytes_t TheWord;
    uint32_t Cnt = (uint32_t)Width * (uint32_t)Height;    // One pixel at a time
    // Write RAM
    PrepareToWriteGRAM();
    for(uint32_t i=0; i<Cnt; i++) {
        TheWord.Word = i;  //*PBuf++;
        WriteByte(TheWord.b[1]);
        WriteByte(TheWord.b[0]);
    }
}
#endif

#if 1 // ============================= BMP =====================================
struct BmpHeader_t {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t Reserved[2];
    uint32_t bfOffBits;
} __packed;

struct BmpInfo_t {  // Length is absent as read first
    uint32_t Width;
    int32_t Height;
    uint16_t Planes;
    uint16_t BitCnt;
    uint32_t Compression;
    uint32_t SzImage;
    uint32_t XPelsPerMeter, YPelsPerMeter;
    uint32_t ClrUsed, ClrImportant;
    // v4 begins. Only Adobe version here
    uint32_t RedMsk, GreenMsk, BlueMsk, AlphaMsk;
} __packed;


void Lcd_t::DrawBmpFile(uint8_t x0, uint8_t y0, const char *Filename, FIL *PFile) {
//    Uart.Printf("Draw %S\r", Filename);
    uint32_t RCnt=0, Sz=0, FOffset;
    BmpHeader_t *PHdr;
    if(TryOpenFileRead(Filename, PFile) != OK) return;
    // Switch off backlight to save power
//    Led1.Set(0);
//    Led2.Set(0);

    Clk.SwitchToHsi48();    // Increase MCU freq
    uint32_t tics = TIM2->CNT;

    // Check if zero file
    if(PFile->fsize == 0) {
        Uart.Printf("Empty file\r");
        goto end;
    }

    // ==== Read BITMAPFILEHEADER ====
    if(f_read(PFile, IBuf, sizeof(BmpHeader_t), &RCnt) != FR_OK) goto end;
    PHdr = (BmpHeader_t*)IBuf;
//    Uart.Printf("T=%X; Sz=%u; Off=%u\r", PHdr->bfType, PHdr->bfSize, PHdr->bfOffBits);
    FOffset = PHdr->bfOffBits;

    // ==== Read BITMAPINFO ====
    // Get struct size => version
    if(f_read(PFile, (uint8_t*)&Sz, 4, &RCnt) != FR_OK) goto end;
    if((Sz == 40) or (Sz == 52) or (Sz == 56)) {  // V3 or V4 adobe
        // Read Info
        if(f_read(PFile, IBuf, Sz-4, &RCnt) != FR_OK) goto end;
        BmpInfo_t *PInfo = (BmpInfo_t*)IBuf;
//        Uart.Printf("W=%u; H=%u; BitCnt=%u; Cmp=%u; Sz=%u;  MskR=%X; MskG=%X; MskB=%X; MskA=%X\r",
//                PInfo->Width, PInfo->Height, PInfo->BitCnt, PInfo->Compression,
//                PInfo->SzImage, PInfo->RedMsk, PInfo->GreenMsk, PInfo->BlueMsk, PInfo->AlphaMsk);
        Sz = PInfo->SzImage;

        // Check row order
        if(PInfo->Height < 0) { // Top to bottom, normal order. Just remove sign.
            PInfo->Height = -PInfo->Height;
        }
        else SetDirHOrigBottomLeft(); // Bottom to top, set origin to bottom

        // Move file cursor to pixel data
        if(f_lseek(PFile, FOffset) != FR_OK) goto end;

        // Setup window
        SetBounds(x0, PInfo->Width, y0, PInfo->Height);
        // ==== Write RAM ====
        PrepareToWriteGRAM();
        while(Sz) {
            if(f_read(PFile, IBuf, BUF_SZ, &RCnt) != FR_OK) goto end;
            WriteBuf(IBuf, RCnt);
            Sz -= RCnt;
        } // while Sz
        // Restore normal origin and direction
        SetDirHOrigTopLeft();
    } // if Sz
    else Uart.Printf("Core, V4 or V5");
    end:
    f_close(PFile);

    tics = TIM2->CNT - tics;
    // Switch back low freq
    Clk.SwitchToHsi();
//    Clk.PrintFreqs();
//    Uart.Printf("cr23=%X\r", RCC->CR2);
    Uart.Printf("tics=%u\r", tics);
    // Restore backlight
    Led1.Set(IBrightness);
    Led2.Set(IBrightness);

    // Signal Draw Completed
    App.SignalEvt(EVTMSK_LCD_DRAW_DONE);
}
#endif

#if 1 // ============================= GIF =====================================
struct LogicalScrDesc_t {
    uint16_t Width;
    uint16_t Height;
    union {
        uint8_t bFields;
        struct {
            uint8_t SizeOfGlobalClrTable : 3; // Least significant
            uint8_t SortFlag : 1;
            uint8_t ClrResolution : 3;
            uint8_t GlobalClrTableFlag : 1; // Most significant
        };
    };
    uint8_t ClrIndx;
    uint8_t PixelAspectRatio;
} __PACKED;
#define LOGICAL_SCR_DESC_SZ     sizeof(LogicalScrDesc_t)

struct RGB_t {
    uint8_t R, G, B;
} __PACKED;
struct ClrTable_t {
    RGB_t Clr[256];
} __PACKED;

struct ImgDesc_t {
    uint8_t Separator;
    uint16_t Left;
    uint16_t Top;
    uint16_t Width;
    uint16_t Height;
    union {
        uint8_t bFields;
        struct {
            uint8_t SzOfLocalTable: 3; // Least significant
            uint8_t Reserved: 2;
            uint8_t SortFlag: 1;
            uint8_t InterlaceFlag: 1;
            uint8_t LocalClrTableFlag: 1; // Most significant
        };
    };
} __PACKED;
#define IMG_DESC_SZ     sizeof(ImgDesc_t)

void Lcd_t::DrawGifFile(uint8_t x0, uint8_t y0, const char *Filename, FIL *PFile) {
    Uart.Printf("Draw %S\r", Filename);
    uint32_t RCnt=0, ClrTblLen;
    uint8_t *PBuf = IBuf;
    ClrTable_t *PTbl;
    LogicalScrDesc_t *PDsc;
    ImgDesc_t ImgDsc;

    if(TryOpenFileRead(Filename, PFile) != OK) return;
    Clk.SwitchToHsi48();    // Increase MCU freq
//    uint32_t tics = TIM2->CNT;
    if(CheckFileNotEmpty(PFile) != OK) goto end;  // Check if zero file
    // Check signature
    if(f_read(PFile, IBuf, 6, &RCnt) != FR_OK) goto end;
    IBuf[6] = 0;
    Uart.Printf("%S\r", IBuf);

    // Read Logical Screen Descriptor
    if(f_read(PFile, IBuf, LOGICAL_SCR_DESC_SZ, &RCnt) != FR_OK) goto end;
    PDsc = (LogicalScrDesc_t*)IBuf;
    Uart.Printf("W=%u; H=%u; ClrTblFlg=%u; ClrRes=%u; SortFlg=%u; TblSz=%u; ClrIndx=%u\r", PDsc->Width, PDsc->Height, PDsc->GlobalClrTableFlag, PDsc->ClrResolution+1, PDsc->SortFlag, PDsc->SizeOfGlobalClrTable, PDsc->ClrIndx);
    // Read global color table if exists
    if(PDsc->GlobalClrTableFlag == 1) {
        ClrTblLen = 1 << (PDsc->SizeOfGlobalClrTable + 1);
        Uart.Printf("GlobTblSz: %u\r", ClrTblLen);
        uint32_t Sz = 3 * ClrTblLen;    // 3 bytes per pixel
        if(f_read(PFile, IBuf, Sz, &RCnt) != FR_OK) goto end;
        PTbl = (ClrTable_t*)IBuf;
        PBuf = IBuf + Sz;
    }

    // Read Img descriptor
    if(f_read(PFile, &ImgDsc, IMG_DESC_SZ, &RCnt) != FR_OK) goto end;
    Uart.Printf("ImgL=%u; ImgT=%u; ImgW=%u; ImgH=%u; ClrTblFlg=%u; InterlaceFlag=%u; SortFlg=%u; TblSz=%u\r", ImgDsc.Left, ImgDsc.Top, ImgDsc.Width, ImgDsc.Height, ImgDsc.LocalClrTableFlag, ImgDsc.InterlaceFlag, ImgDsc.SortFlag, ImgDsc.SzOfLocalTable);
    // Read Local Color Table if exists
    if(ImgDsc.LocalClrTableFlag == 1) {
        ClrTblLen = 1 << (ImgDsc.SzOfLocalTable + 1);
        Uart.Printf("LocalTblSz: %u\r", ClrTblLen);
        uint32_t Sz = 3 * ClrTblLen;    // 3 bytes per pixel
        if(f_read(PFile, IBuf, Sz, &RCnt) != FR_OK) goto end;
        PTbl = (ClrTable_t*)IBuf;
        PBuf = IBuf + Sz;
    }



    end:
    f_close(PFile);

//    tics = TIM2->CNT - tics;
    // Switch back low freq
    Clk.SwitchToHsi();
//    Clk.PrintFreqs();
//    Uart.Printf("cr23=%X\r", RCC->CR2);
//    Uart.Printf("tics=%u\r", tics);
    // Restore backlight
//    Led1.Set(IBrightness);
//    Led2.Set(IBrightness);
}
#endif

void Lcd_t::DrawBattery(uint8_t Percent, BatteryState_t State, LcdHideProcess_t Hide) {
    // Switch off backlight to save power if needed
    if(Hide == lhpHide) {
        Led1.Set(0);
        Led2.Set(0);
    }
    if(Percent > 0) Clk.SwitchToHsi48();    // Increase MCU freq

    const uint8_t *PPic = PicBattery;
    const uint8_t *PLght = PicLightning;
    uint32_t ChargeYTop = PIC_CHARGE_YB - Percent;

    // Select charging fill color
    uint8_t ChargeHi, ChargeLo;
    if(Percent > 20 or State == bstCharging) {
        ChargeHi = CHARGE_HI_CLR.RGBTo565_HiByte();
        ChargeLo = CHARGE_HI_CLR.RGBTo565_LoByte();
    }
    else if(Percent > 10) {
        ChargeHi = CHARGE_MID_CLR.RGBTo565_HiByte();
        ChargeLo = CHARGE_MID_CLR.RGBTo565_LoByte();
    }
    else {
        ChargeHi = CHARGE_LO_CLR.RGBTo565_HiByte();
        ChargeLo = CHARGE_LO_CLR.RGBTo565_LoByte();
    }

    // Redraw full screen if need to hide
    if(Hide == lhpHide) {
        SetBounds(0, LCD_W, 0, LCD_H);
        PrepareToWriteGRAM();
        for(uint16_t y=0; y<LCD_H; y++) {
            for(uint16_t x=0; x<LCD_W; x++) {
                uint8_t bHi, bLo;
                // Draw either battery or charge value
                if(x >= PIC_BATTERY_XL and x < PIC_BATTERY_XR and
                   y >= PIC_BATTERY_YT and y < PIC_BATTERY_YB) {
                    bHi = *PPic++; // }
                    bLo = *PPic++; // } read pic_battery

                    // Draw lightning if charging
                    if(State == bstCharging and
                            x >= PIC_LIGHTNING_XL and x < PIC_LIGHTNING_XR and
                            y >= PIC_LIGHTNING_YT and y < PIC_LIGHTNING_YB) {
                        bHi = *PLght++; // }
                        bLo = *PLght++; // } Read pic_lightning
                        if(bHi == 0 and bLo == 0) {
                            if(y >= ChargeYTop) {
                                bHi = ChargeHi; // }
                                bLo = ChargeLo; // } Fill transparent backcolor
                            }
                        }
                    } // if inside lightning

                    // Inside battery and (not charging or not inside lightning)
                    else {
                        if(x >= PIC_CHARGE_XL and x < PIC_CHARGE_XR and
                           y >= ChargeYTop    and y < PIC_CHARGE_YB and
                           bHi == 0 and bLo == 0) { // Check if battery is not touched
                            bHi = ChargeHi;
                            bLo = ChargeLo;
                        }
                    } // if inside charge
                } // if inside battery
                // Clear screen where out of battery
                else {
                    bHi = 0;
                    bLo = 0;
                }
                WriteByte(bHi);
                WriteByte(bLo);
            } // for x
        } // for y
    } // if hide

    // Redraw only battery
    else {
        SetBounds(PIC_BATTERY_XL, PIC_BATTERY_W, PIC_BATTERY_YT, PIC_BATTERY_H);
        PrepareToWriteGRAM();
        for(uint16_t y=PIC_BATTERY_YT; y<PIC_BATTERY_YB; y++) {
            for(uint16_t x=PIC_BATTERY_XL; x<PIC_BATTERY_XR; x++) {
                uint8_t bHi, bLo;
                // Draw either battery or charge value
                bHi = *PPic++; // }
                bLo = *PPic++; // } read pic_battery

                // Draw lightning if charging
                if(State == bstCharging and
                        x >= PIC_LIGHTNING_XL and x < PIC_LIGHTNING_XR and
                        y >= PIC_LIGHTNING_YT and y < PIC_LIGHTNING_YB) {
                    bHi = *PLght++; // }
                    bLo = *PLght++; // } Read pic_lightning
                    if(bHi == 0 and bLo == 0) {
                        if(y >= ChargeYTop) {
                            bHi = ChargeHi; // }
                            bLo = ChargeLo; // } Fill transparent backcolor
                        }
                    }
                } // if inside lightning

                // Inside battery and (not charging or not inside lightning)
                else {
                    if(x >= PIC_CHARGE_XL and x < PIC_CHARGE_XR and
                       y >= ChargeYTop    and y < PIC_CHARGE_YB and
                       bHi == 0 and bLo == 0) { // Check if battery is not touched
                        bHi = ChargeHi;
                        bLo = ChargeLo;
                    }
                } // if inside charge
                WriteByte(bHi);
                WriteByte(bLo);
            } // for x
        } // for y
    } // if not hide

    if(Percent > 0) Clk.SwitchToHsi();
    // Restore backlight
    if(Hide == lhpHide) {
        Led1.Set(IBrightness);
        Led2.Set(IBrightness);
    }
}

void Lcd_t::DrawNoImage() {
    // Switch off backlight to save power if needed
    Led1.Set(0);
    Led2.Set(0);
    Clk.SwitchToHsi48();    // Increase MCU freq

    const uint8_t *PPic = PicNoImage;
    SetBounds(0, LCD_W, 0, LCD_H);
    PrepareToWriteGRAM();
    for(uint16_t y=0; y<LCD_H; y++) {
        for(uint16_t x=0; x<LCD_W; x++) {
            if(x >= PIC_NOIMAGE_XL and x < PIC_NOIMAGE_XR and
               y >= PIC_NOIMAGE_YT and y < PIC_NOIMAGE_YB) {
                WriteByte(*PPic++);
                WriteByte(*PPic++);
            }
            else {
                WriteByte(0);
                WriteByte(0);
            }
        } // for x
    } // for y

    // Restore backlight
    Clk.SwitchToHsi();
    Led1.Set(IBrightness);
    Led2.Set(IBrightness);
}
