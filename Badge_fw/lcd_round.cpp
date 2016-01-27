#include "lcd_round.h"
#include "kl_lib.h"
#include <string.h>
#include <stdarg.h>
#include "core_cmInstr.h"
#include "uart.h"

//#include "lcdFont8x8.h"
#include <string.h>

// Variables
Lcd_t Lcd;

struct RegData_t {
    uint16_t Reg, Data;
    uint32_t Delay;
};
const RegData_t InitData[] = {
//        {0xA4, 0x0001, 10}, // NVM calibration
//        {0x60, 0x2700, 0},  // (def) Driver output control: number of lines = 320, gate start = 0
//        {0x08, 0x0808, 0},  // (def) front and back porch period
        // Gamma correction
 /*       {0x30, 0x0214, 0},
        {0x31, 0x3715, 0},
        {0x32, 0x0604, 0},
        {0x33, 0x0E16, 0},
        {0x34, 0x2211, 0},
        {0x35, 0x1500, 0},
        {0x36, 0x8507, 0},
        {0x37, 0x1407, 0},
        {0x38, 0x1403, 0},
        {0x39, 0x0020, 0},*/

//        {0x90, 0x0015, 0},  // (def 0x0111) division 1/1, line period 21clk
//        {0x10, 0x0410, 0},
//        {0x11, 0x0237, 0},

        {0x29, 0x0046, 0},
        {0x2A, 0x0046, 0},
        {0x07, 0x0000, 0},
        {0x12, 0x0189, 0},
        {0x13, 0x1100, 150},

        {0x12, 0x01B9, 0},
        {0x01, 0x0100, 0},
        {0x02, 0x0200, 0},
        {0x03, 0x1030, 0},
        {0x09, 0x0001, 0},
        {0x0A, 0x0000, 0},
        {0x0D, 0x0000, 0},
        {0x0E, 0x0030, 0},
        {0x50, 0x0000, 0},
        {0x51, 0x00EF, 0},
        {0x52, 0x0000, 0},
        {0x53, 0x013F, 0},
        {0x61, 0x0001, 0},
        {0x6A, 0x0000, 0},
        {0x80, 0x0000, 0},
        {0x81, 0x0000, 0},
        {0x82, 0x005F, 0},
        {0x92, 0x0100, 0},
        {0x93, 0x0701, 80},
        {0x07, 0x0100, 0},
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
__attribute__ ((always_inline)) static inline void WriteByte(uint8_t Byte) {
        LCD_GPIO->BRR  = 0xFF;        /* Clear bus */
        LCD_GPIO->BSRR = Byte;        /* Place data on bus */
        LCD_GPIO->BRR  = (1<<LCD_WR); /* WR Low */
        LCD_GPIO->BSRR = (1<<LCD_WR); /* WR high */
}
#endif

void Lcd_t::Init() {
    // Backlight
    Led1.Init();
    Led2.Init();
    SetBrightness(100);
    // Pins
    PinSetupOut(LCD_GPIO, LCD_RST, omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_CS,  omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_RS,  omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_WR,  omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_RD,  omPushPull, pudNone, psMedium);
    PinSetupOut(LCD_GPIO, LCD_IMO, omPushPull, pudNone, psMedium);
    // Configure data bus as outputs
    for(uint8_t i=0; i<8; i++) PinSetupOut(LCD_GPIO, i, omPushPull, pudNone, psHigh);

    // ======= Init LCD =======
    // Initial signals
    PinSet(LCD_GPIO, LCD_IMO);  // Select 8-bit parallel bus
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
        if(InitData[i].Delay > 0) chThdSleepMilliseconds(InitData[i].Delay);
    }

//    Cls(clGreen);

//    GoTo(0, 0);
    SetBounds(80, 100, 20, 100);
    WriteReg(0x21, 20);
//    GoTo(80, 20);

    uint8_t ClrUpper = 0xF0;
    uint8_t ClrLower = 0x0F;
    // Fill LCD
    PrepareToWriteGRAM();
    for(uint32_t i=0; i<(100 * 100); i++) {
        WriteByte(ClrUpper);
        WriteByte(ClrLower);
    }
}

void Lcd_t::Shutdown(void) {
//    XRES_Lo();
//    XCS_Lo();
//    BckLt.Off();
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

/*
// ================================= Printf ====================================
void Lcd_t::PutChar(char c) {
    char *PFont = (char*)Font8x8;  // Font to use
    // Read font params
    uint8_t nCols = PFont[0];
    uint8_t nRows = PFont[1];
    uint16_t nBytes = PFont[2];
    SetBounds(IX, nCols, IY, nRows);
    // Get pointer to the first byte of the desired character
    const char *PChar = Font8x8 + (nBytes * (c - 0x1F));
    // Write RAM
    WriteByte(0x2C);    // Memory write
    DC_Hi();
    // Iterate rows of the char
    uint8_t row, col;
    for(row = 0; row < nRows; row++) {
        if((IY+row) >= LCD_H) break;
        uint8_t PixelRow = *PChar++;
        // Loop on each pixel in the row (left to right)
#ifdef LCD_12BIT
        for(col=0; col < nCols; col+=2) {
            if((IX+col) >= LCD_W) break;
            // Two pixels at one time
            uint16_t Clr1 = (PixelRow & 0x80)? IForeClr : IBckClr;
            PixelRow <<= 1;
            uint16_t Clr2 = (PixelRow & 0x80)? IForeClr : IBckClr;
            PixelRow <<= 1;
            uint8_t b1 = (uint8_t)(Clr1 >> 4);       // RRRR-GGGG
            uint8_t b2 = (uint8_t)(((Clr1 & 0x00F) << 4) | (Clr2 >> 8));  // BBBB-RRRR
            uint8_t b3 = (uint8_t)(Clr2 & 0x0FF);    // GGGG-BBBB
            WriteByte(b1);
            WriteByte(b2);
            WriteByte(b3);
        } // col
#else
        for(col=0; col < nCols; col++) {
            if((IX+col) >= LCD_W) break;
            uint16_t Clr = (PixelRow & 0x80)? IForeClr : IBckClr;
            PixelRow <<= 1;
            WriteByte(Clr >> 8);    // RRRRR-GGG
            WriteByte(Clr & 0xFF);  // GGG-BBBBB
        } // col
#endif
    } // row
    DC_Lo();
    IX += nCols;
}

static inline void FLcdPutChar(char c) { Lcd.PutChar(c); }

void Lcd_t::Printf(uint8_t x, uint8_t y, const Color_t ForeClr, const Color_t BckClr, const char *S, ...) {
    IX = x;
    IY = y;
    IForeClr = ForeClr;
    IBckClr = BckClr;
    va_list args;
    va_start(args, S);
    kl_vsprintf(FLcdPutChar, 20, S, args);
    va_end(args);
}
*/
//#if 1 // ============================= Graphics ================================
void Lcd_t::GoTo(uint16_t x, uint16_t y) {
    WriteReg(0x20, x);     // GRAM Address Set (Horizontal Address) (R20h)
    WriteReg(0x21, y);     // GRAM Address Set (Vertical Address) (R21h)
}

void Lcd_t::SetBounds(uint16_t Left, uint16_t Width, uint16_t Top, uint16_t Height) {
    uint16_t XEndAddr = Left + Width  - 1;
    uint16_t YEndAddr = Top  + Height - 1;

    Uart.Printf("xs=%u, xe=%u; ys=%u, ye=%u\r", Left, XEndAddr, Top, YEndAddr);
//    if(XEndAddr > 0xEF) XEndAddr = 0xEF;
//    if(YEndAddr > 0x13F) YEndAddr = 0x13F;
    // Write bounds
    WriteReg(0x50, Left);
    WriteReg(0x51, XEndAddr);
    WriteReg(0x52, Top);
    WriteReg(0x53, YEndAddr);
}

void Lcd_t::Cls(Color_t Color) {
    GoTo(0, 0);
    // Prepare variables
    uint8_t ClrUpper = Color.RGBTo565_HiByte();
    uint8_t ClrLower = Color.RGBTo565_LoByte();
//    Uart.Printf("%X %X\r", ClrUpper, ClrLower);
    // Fill LCD
    PrepareToWriteGRAM();
    for(uint32_t i=0; i<(LCD_H * LCD_W); i++) {
        WriteByte(ClrUpper);
        WriteByte(ClrLower);
    }
}
/*
void Lcd_t::GetBitmap(uint8_t x0, uint8_t y0, uint8_t Width, uint8_t Height, uint16_t *PBuf) {
    SetBounds(x0, x0+Width, y0, y0+Height);
    // Prepare variables
    uint32_t Cnt = Width * Height;
    uint16_t R, G, B;
    // Read RAM
    WriteByte(0x2E);    // RAMRD
    DC_Hi();
    ModeRead();
    ReadByte();         // Dummy read
    for(uint32_t i=0; i<Cnt; i++) {
        R = ReadByte(); // }
        G = ReadByte(); // }
        B = ReadByte(); // } Inside LCD, data is always in 18bit format.
        // Produce 4R-4G-4B from 6R-6G-6B
        *PBuf++ = ((R & 0xF0) << 4) | (G & 0xF0) | ((B & 0xF0) >> 4);
    }
    ModeWrite();
    DC_Lo();
}

void Lcd_t::PutBitmap(uint8_t x0, uint8_t y0, uint8_t Width, uint8_t Height, uint16_t *PBuf) {
    //Uart.Printf("%u %u %u %u %u\r", x0, y0, Width, Height, *PBuf);
    SetBounds(x0, x0+Width, y0, y0+Height);
    // Prepare variables
#ifdef LCD_18BIT

#elif defined LCD_16BIT
    uint16_t Clr;
    uint32_t Cnt = (uint32_t)Width * (uint32_t)Height;    // One pixel at one time
    // Write RAM
    WriteByte(0x2C);    // Memory write
    DC_Hi();
    for(uint32_t i=0; i<Cnt; i++) {
        Clr = *PBuf++;
        uint16_t R = (Clr >> 8) & 0x000F;
        uint16_t G = (Clr >> 4) & 0x000F;
        uint16_t B = (Clr     ) & 0x000F;
        R = (R << 4) | (G >> 1);
        G = (G << 7) | (B << 1);
        WriteByte(R & 0x0F);   // RRRR0-GGG
        WriteByte(G & 0x0F);   // G00-BBBB0
    }
    DC_Lo();
#else
    uint16_t Clr1, Clr2;
    uint32_t Cnt = (uint32_t)Width * Height / 2;      // Two pixels at one time
    // Write RAM
    WriteByte(0x2C);    // Memory write
    DC_Hi();
    for(uint32_t i=0; i<Cnt; i++) {
        Clr1 = (*PBuf++) & 0x0FFF;
        Clr2 = (*PBuf++) & 0x0FFF;
        WriteByte(Clr1 >> 4);    // RRRR-GGGG
        WriteByte(((Clr1 & 0x00F) << 4) | (Clr2 >> 8));   // BBBB-RRRR
        WriteByte(Clr2 & 0x0FF); // GGGG-BBBB
    }
    DC_Lo();
#endif
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
    uint32_t Height;
    uint16_t Planes;
    uint16_t BitCnt;
    uint32_t Compression;
    uint32_t SzImage;
    uint32_t XPelsPerMeter, YPelsPerMeter;
    uint32_t ClrUsed, ClrImportant;
    // v4 begins. Only Adobe version here
    uint32_t RedMsk, GreenMsk, BlueMsk, AlphaMsk;
} __packed;


void Lcd_t::DrawBmpFile(uint8_t x0, uint8_t y0, const char *Filename) {
    Uart.Printf("Draw %S\r", Filename);
    // Open file
    FRESULT rslt = f_open(&IFile, Filename, FA_READ+FA_OPEN_EXISTING);
    if(rslt != FR_OK) {
        if (rslt == FR_NO_FILE) Uart.Printf("%S: not found\r", Filename);
        else Uart.Printf("OpenFile error: %u", rslt);
        return;
    }
    // Check if zero file
    if(IFile.fsize == 0) {
        Uart.Printf("Empty file\r");
        f_close(&IFile); return;
    }

    unsigned int RCnt=0;
    // Read BITMAPFILEHEADER
    if((rslt = f_read(&IFile, IBuf, sizeof(BmpHeader_t), &RCnt)) != 0) {
        f_close(&IFile); return;
    }
    BmpHeader_t *PHdr = (BmpHeader_t*)IBuf;
    Uart.Printf("T=%X; Sz=%u; Off=%u\r", PHdr->bfType, PHdr->bfSize, PHdr->bfOffBits);
    uint32_t FOff = PHdr->bfOffBits;

    // ==== Read BITMAPINFO ====
    // Get struct size => version
    uint32_t Sz=0;
    if((rslt = f_read(&IFile, (uint8_t*)&Sz, 4, &RCnt)) != 0) {
        f_close(&IFile); return;
    }
    if((Sz == 40) or (Sz == 52) or (Sz == 56)) {  // V3 or V4 adobe
        BmpInfo_t Info;
        if((rslt = f_read(&IFile, (uint8_t*)&Info, Sz-4, &RCnt)) != 0) {
            f_close(&IFile); return;
        }
        Uart.Printf("W=%u; H=%u; BitCnt=%u; Cmp=%u; Sz=%u;  MskR=%X; MskG=%X; MskB=%X; MskA=%X\r",
                Info.Width, Info.Height, Info.BitCnt, Info.Compression,
                Info.SzImage, Info.RedMsk, Info.GreenMsk, Info.BlueMsk, Info.AlphaMsk);

        // Move to pixel data
        if((rslt = f_lseek(&IFile, FOff)) != 0) {
            f_close(&IFile); return;
        }

        SetBounds(x0, x0+Info.Width, y0, y0+Info.Height);
        // Write RAM
        WriteByte(0x2C);    // Memory write
        DC_Hi();
        while(Info.SzImage) {
            if((rslt = f_read(&IFile, IBuf, BUF_SZ, &RCnt)) != 0) break;
            for(uint32_t i=0; i<RCnt; i+=2) {
                WriteByte(IBuf[i+1]);
                WriteByte(IBuf[i]);
            }
            Info.SzImage -= RCnt;
        }
        DC_Lo();
    }

    else {
        Uart.Printf("Core, V4 or V5");
    }


//    if(strncmp(IBuf, PngSignature, 8) != 0) {
//        Uart.Printf("SigErr\r");
//        f_close(&IFile); return;
//    }



    f_close(&IFile);
    Uart.Printf("Done\r");
}


#endif

*/
