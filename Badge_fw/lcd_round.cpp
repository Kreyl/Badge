#include "lcd_round.h"
#include "kl_lib.h"
#include <string.h>
#include <stdarg.h>
#include "core_cmInstr.h"
#include "uart.h"
#include "main.h"
#include "pics.h"
#include "kl_fs_common.h"
#include "led.h"

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
#define RstHi()  { PinSet(LCD_GPIO, LCD_RST);   }
#define RstLo()  { PinClear(LCD_GPIO, LCD_RST); }
#define CsHi()   { PinSet(LCD_GPIO, LCD_CS);    }
#define CsLo()   { PinClear(LCD_GPIO, LCD_CS);  }
#define RsHi()   { PinSet(LCD_GPIO, LCD_RS);    }
#define RsLo()   { PinClear(LCD_GPIO, LCD_RS);  }
#define WrHi()   { PinSet(LCD_GPIO, LCD_WR);    }
#define WrLo()   { PinClear(LCD_GPIO, LCD_WR);  }
#define RdHi()   { PinSet(LCD_GPIO, LCD_RD);    }
#define RdLo()   { PinClear(LCD_GPIO, LCD_RD);  }
#define PwrOn()  { PinSet(LCD_GPIO, LCD_PWR);   }
#define PwrOff() { PinClear(LCD_GPIO, LCD_PWR); }

#define SetReadMode()   LCD_GPIO->MODER &= LCD_MODE_MSK_READ
#define SetWriteMode()  LCD_GPIO->MODER |= LCD_MODE_MSK_WRITE

// Bus operations
__always_inline static inline
void WriteByte(uint8_t Byte) {
        LCD_GPIO->BRR  = (0xFF | (1<<LCD_WR));  // Clear bus and set WR low
        LCD_GPIO->BSRR = Byte;                  // Place data on bus
        LCD_GPIO->BSRR = (1<<LCD_WR);           // WR high
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
    PinSetupOut(LCD_GPIO, LCD_PWR, omPushPull, pudNone, psMedium);
    PwrOn();
    chThdSleepMilliseconds(270);

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
    uint16_t r = ReadReg(0x00);
    Uart.Printf("LcdID=%X\r", r);
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
    RstLo();
    CsLo();
    RsLo();
    RdLo();
    WrLo();
    PwrOff();
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
// Length of next structure BmpInfo added to optimize reading
struct BmpHeader_t {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t Reserved[2];
    uint32_t bfOffBits;
} __packed;
#define BMP_HDR_SZ      sizeof(BmpHeader_t)     // 14

struct BmpInfo_t {
    uint32_t BmpInfoSz;
    int32_t Width;
    int32_t Height;
    uint16_t Planes;
    uint16_t BitCnt;
    uint32_t Compression;
    uint32_t SzImage;
    int32_t XPelsPerMeter, YPelsPerMeter;
    uint32_t ClrUsed, ClrImportant;
    // End of Bmp info min
    uint32_t RedMsk, GreenMsk, BlueMsk, AlphaMsk;
    uint32_t CsType;
    uint32_t Endpoints[9];
    uint32_t GammaRed;
    uint32_t GammaGreen;
    uint32_t GammaBlue;
} __packed;
#define BMP_INFO_SZ         sizeof(BmpInfo_t)
#define BMP_MIN_INFO_SZ     40

// Color table
struct BGR_t {
    uint8_t B, G, R, A;
} __packed;

static BGR_t ColorTable[256];

__ramfunc
static inline void PutTablePixel(uint8_t id) {
    uint8_t R = ColorTable[id].R;
    uint8_t G = ColorTable[id].G;
    uint8_t B = ColorTable[id].B;
    // High byte
    uint8_t byte = R & 0b11111000;
    byte |= G >> 5;
    WriteByte(byte);
    // Low byte
    byte = (G << 3) & 0b11100000;
    byte |= B >> 3;
    WriteByte(byte);
}

//__ramfunc
void WriteLine1(uint8_t *PBuf, int32_t Width) {
    int32_t Cnt = 0, Top = MIN(Width, LCD_W);
    while(true) {
        uint8_t Indx = *PBuf++;
        for(uint32_t k=0; k<8; k++) {
            PutTablePixel(Indx & 0x80 ? 1 : 0);
            Indx <<= 1;
            Cnt++;
            if(Cnt >= Top) return;
        }
    } // while(true)
}

//__ramfunc
void WriteLine4(uint8_t *PBuf, int32_t Width) {
    int32_t Cnt = 0, Top = MIN(Width, LCD_W);
    while(true) {
        uint8_t Indx = *PBuf++;
        PutTablePixel((Indx >> 4) & 0x0F);
        Cnt++;
        if(Cnt >= Top) break;
        PutTablePixel(Indx & 0x0F);
        Cnt++;
        if(Cnt >= Top) break;
    } // while(true)
}

//__ramfunc
void WriteLine8(uint8_t *PBuf, int32_t Width) {
    int32_t Cnt = 0, Top = MIN(Width, LCD_W);
    while(Cnt < Top) {
        uint8_t Indx = *PBuf++;
        PutTablePixel(Indx);
        Cnt++;
    } // while(true)
}

__ramfunc
void WriteLine16(uint8_t *PBuf, int32_t Width) {
    int32_t Cnt = 0, Top = MIN(Width, LCD_W);
    while(Cnt < Top) {
        WriteByte(PBuf[1]);
        WriteByte(PBuf[0]);
        PBuf += 2;
        Cnt++;
    }
}

__ramfunc
void WriteLine24(uint8_t *PBuf, int32_t Width) {
    int32_t Cnt = 0, Top = MIN(Width, LCD_W);
    while(Cnt < Top) {
        uint8_t B = *PBuf++;
        uint8_t G = *PBuf++;
        uint8_t R = *PBuf++;
        // High byte
        uint8_t byte = R & 0b11111000;
        byte |= G >> 5;
        WriteByte(byte);
        // Low byte
        byte = (G << 3) & 0b11100000;
        byte |= B >> 3;
        WriteByte(byte);
        Cnt++;
    }
}

void WriteLine32(uint8_t *PBuf, int32_t Width) {
    int32_t Cnt = 0, Top = MIN(Width, LCD_W);
    while(Cnt < Top) {
        uint8_t B = *PBuf++;
        uint8_t G = *PBuf++;
        uint8_t R = *PBuf++;
        PBuf++; // Discard alpha channel
        // High byte
        uint8_t byte = R & 0b11111000;
        byte |= G >> 5;
        WriteByte(byte);
        // Low byte
        byte = (G << 3) & 0b11100000;
        byte |= B >> 3;
        WriteByte(byte);
        Cnt++;
    }
}

uint8_t Lcd_t::DrawBmpFile(uint8_t x0, uint8_t y0, const char *Filename, FIL *PFile, uint16_t FadeIn, uint16_t FadeOut) {
//    Uart.Printf("Draw %S\r", Filename);
    uint32_t RCnt=0, FOffset, ColorTableSize = 0, BitCnt;
    int32_t Width, Height, LineSz;
    BmpHeader_t *PHdr;
    BmpInfo_t *PInfo;
    if(TryOpenFileRead(Filename, PFile) != OK) return FAILURE;
    uint8_t Rslt = FAILURE;
    // Process backlight
    if(FadeOut != 0) {
        for(int i=IBrightness; i>0; i--) {
            Led1.Set(i);
            Led2.Set(i);
            uint32_t Delay = ICalcDelay(i, FadeOut);
            // Get out if button pressed
            if(chEvtWaitAnyTimeout(EVT_BUTTONS, Delay) == EVT_BUTTONS) {
                App.SignalEvt(EVT_BUTTONS); // Restore button event
                Led1.Set(IBrightness);
                Led2.Set(IBrightness);
                return OK;
            }
        }
    }
    else {
        Led1.Set(0);
        Led2.Set(0);
    }

    Clk.SwitchToHsi48();    // Increase MCU freq
    uint32_t tics = TIM2->CNT;

    // ==== BITMAPFILEHEADER ====
    if(f_read(PFile, IBuf, BMP_HDR_SZ, &RCnt) != FR_OK) goto end;
    PHdr = (BmpHeader_t*)IBuf;
//    Uart.Printf("T=%X; Sz=%u; Off=%u\r", PHdr->bfType, PHdr->bfSize, PHdr->bfOffBits);
    if(PHdr->bfType != 0x4D42) goto end;    // Wrong file type
    FOffset = PHdr->bfOffBits;

    // ==== BITMAPINFO ====
    if(f_read(PFile, IBuf, BMP_MIN_INFO_SZ, &RCnt) != FR_OK) goto end;
    PInfo = (BmpInfo_t*)IBuf;
//    Uart.Printf("BmpInfoSz=%u; W=%d; H=%d; BitCnt=%u; Cmp=%u; Sz=%u;  ColorsInTbl=%u\r", PInfo->BmpInfoSz, PInfo->Width, PInfo->Height, PInfo->BitCnt, PInfo->Compression, PInfo->SzImage, PInfo->ClrUsed);
    Width = PInfo->Width;
    Height = PInfo->Height;
    BitCnt = PInfo->BitCnt;

    // Check row order
    if(Height < 0) Height = -Height; // Top to bottom, normal order. Just remove sign.
    else SetDirHOrigBottomLeft();    // Bottom to top, set origin to bottom
    TRIM_VALUE(Height, LCD_H);

    // ==== Color table ====
    if(PInfo->ClrUsed == 0) {
        if     (BitCnt == 1) ColorTableSize = 2;
        else if(BitCnt == 4) ColorTableSize = 16;
        else if(BitCnt == 8) ColorTableSize = 256;
    }
    else ColorTableSize = PInfo->ClrUsed;
    if(ColorTableSize > 256) goto end;
    if(ColorTableSize != 0) {
        // Move file cursor to color table data if needed
        if(PInfo->BmpInfoSz != BMP_MIN_INFO_SZ) {
            uint32_t ClrTblOffset = BMP_HDR_SZ + PInfo->BmpInfoSz;
            if(f_lseek(PFile, ClrTblOffset) != FR_OK) goto end;
        }
        // Read color table
        if(f_read(PFile, ColorTable, (ColorTableSize * 4), &RCnt) != FR_OK) goto end;
    }

    // Move file cursor to pixel data
    if(f_lseek(PFile, FOffset) != FR_OK) goto end;
    // Setup window
    if(Width < LCD_W) x0 = (LCD_W - Width) / 2;     // }
    if(Height < LCD_W) y0 = (LCD_H - Height) / 2;   // } Put image to center
    SetBounds(x0, MIN(Width, LCD_W), y0, Height);
    PrepareToWriteGRAM();

    // ==== Draw pic line by line ====
    LineSz = (((Width * BitCnt) / 8) + 3) & ~3;
    if(LineSz > BUF_SZ) goto end;
    for(int32_t i=0; i<Height; i++) {
        if(f_read(PFile, IBuf, LineSz, &RCnt) != FR_OK) goto end;
        // Select method of drawing depending on bits per pixel
        switch(BitCnt) {
            case 1:  WriteLine1 (IBuf, Width); break;
            case 4:  WriteLine4 (IBuf, Width); break;
            case 8:  WriteLine8 (IBuf, Width); break;
            case 16: WriteLine16(IBuf, Width); break;
            case 24: WriteLine24(IBuf, Width); break;
            case 32: WriteLine32(IBuf, Width); break;
            default: break;
        }
    } // for i
    Rslt = OK;

    end:
    f_close(PFile);
    SetDirHOrigTopLeft();   // Restore normal origin and direction

    tics = TIM2->CNT - tics;
    // Switch back low freq
    Clk.SwitchToHsi();
//    Clk.PrintFreqs();
//    Uart.Printf("cr23=%X\r", RCC->CR2);
//    Uart.Printf("tics=%u\r", tics);
    // Restore backlight
    if(FadeIn != 0 and Rslt == OK) {
        for(uint8_t i=0; i<IBrightness; i++) {
            Led1.Set(i);
            Led2.Set(i);
            uint32_t Delay = ICalcDelay(i, FadeIn);
            // Restore brightness immediately if button pressed
            if(chEvtWaitAnyTimeout(EVT_BUTTONS, Delay) == EVT_BUTTONS) {
                App.SignalEvt(EVT_BUTTONS); // Restore button event
                Led1.Set(IBrightness);
                Led2.Set(IBrightness);
                break;
            }
        }
    }
    else {
        Led1.Set(IBrightness);
        Led2.Set(IBrightness);
    }

    // Signal Draw Completed
    App.SignalEvt(EVT_LCD_DRAW_DONE);
    return Rslt;
}
#endif

void Lcd_t::DrawBattery(uint8_t Percent, BatteryState_t State, LcdHideProcess_t Hide) {
//    Uart.Printf("DrawBat\r");
    // Switch off backlight to save power if needed
    if(Hide == lhpHide) {
        Led1.Set(0);
        Led2.Set(0);
    }
    if(Percent > 0) Clk.SwitchToHsi48();    // Increase MCU freq

    RLE_Decoder_t DecBattery((uint8_t*)PicBattery);
    RLE_Decoder_t DecLightning((uint8_t*)PicLightning);
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
                    bHi = DecBattery.GetNext(); // }
                    bLo = DecBattery.GetNext(); // } read pic_battery

                    // Draw lightning if charging
                    if(State == bstCharging and
                            x >= PIC_LIGHTNING_XL and x < PIC_LIGHTNING_XR and
                            y >= PIC_LIGHTNING_YT and y < PIC_LIGHTNING_YB) {
                        bHi = DecLightning.GetNext(); // }
                        bLo = DecLightning.GetNext(); // } Read pic_lightning
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
                bHi = DecBattery.GetNext(); // }
                bLo = DecBattery.GetNext(); // } read pic_battery

                // Draw lightning if charging
                if(State == bstCharging and
                        x >= PIC_LIGHTNING_XL and x < PIC_LIGHTNING_XR and
                        y >= PIC_LIGHTNING_YT and y < PIC_LIGHTNING_YB) {
                    bHi = DecLightning.GetNext(); // }
                    bLo = DecLightning.GetNext(); // } Read pic_lightning
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

    RLE_Decoder_t Dec((uint8_t*)PicNoImage);
    SetBounds(0, LCD_W, 0, LCD_H);
    PrepareToWriteGRAM();
    for(uint16_t y=0; y<LCD_H; y++) {
        for(uint16_t x=0; x<LCD_W; x++) {
            if(x >= PIC_NOIMAGE_XL and x < PIC_NOIMAGE_XR and
               y >= PIC_NOIMAGE_YT and y < PIC_NOIMAGE_YB) {
                WriteByte(Dec.GetNext());
                WriteByte(Dec.GetNext());
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

#if 1 // =============================== RLE Decoder ===========================
RLE_Decoder_t::RLE_Decoder_t(uint8_t *p) {
    Ptr = p;
    Counter = 0;
    Rslt = 0;
    Mode = rlemRepeat;
}

uint8_t RLE_Decoder_t::GetNext() {
    if(Counter == 0) { // current row is over
        Counter = *Ptr++;
//        Uart.Printf("\rCnt1=0x%02X; ", Counter);
        if(Counter & 0x80) {
            Mode = rlemUniq;
            Counter = -Counter;
        }
        else {
            Mode = rlemRepeat;
            Rslt = *Ptr++;
        }
//        Uart.Printf("\rCnt=0x%02X; Mode=%u\r", Counter, Mode);
    }
    Counter--;
    if(Mode == rlemUniq) Rslt = *Ptr++;
    return Rslt;
}

#endif
