/*
 * File:   lcd_round.h
 * Author: Kreyl Laurelindo
 *
 * Created on 26.01.2016
 *
 * Round LCD has R61505V controller (its ID reads B505).
 *
 */

#pragma once

#include "kl_lib.h"
#include <string.h>
#include "board.h"
#include "color.h"
#include "ff.h"

#define LCD_X_0             10  // Zero pixels are shifted
#define LCD_W               220 // }
#define LCD_H               220 // } Pixels count
#define LCD_TOP_BRIGHTNESS  100 // i.e. 100%

#define BUF_SZ              128

class Lcd_t {
private:
    PinOutputPWM_t<100, invInverted, omPushPull> Led1 {GPIOB, 14, TIM15, 1};
    PinOutputPWM_t<LCD_TOP_BRIGHTNESS, invInverted, omPushPull> Led2 {LCD_BCKLT_GPIO, LCD_BCKLT_PIN2, LCD_BCKLT_TMR, LCD_BCKLT_CHNL2};
//    uint16_t IX, IY;
//    Color_t IForeClr, IBckClr;
    void WriteReg(uint8_t AReg, uint16_t AData);
    uint16_t ReadReg(uint8_t AReg);
    void GoTo(uint16_t x, uint16_t y);
    void PrepareToWriteGRAM();
    void SetBounds(uint16_t Left, uint16_t Width, uint16_t Top, uint16_t Height);
    uint8_t IBuf[BUF_SZ];
    uint16_t IBrightness;
public:
    // General use
    void Init();
    void Shutdown();
    void SetBrightness(uint16_t ABrightness);
    uint16_t GetBrightness() { return IBrightness; }
    // Direction & Origin
    void SetDirHOrigTopLeft()    { WriteReg(0x03, 0x12B0); } // HWM=1, ORG=1, ID=11=>origin top left
    void SetDirHOrigBottomLeft() { WriteReg(0x03, 0x1290); } // HWM=1, ORG=1, ID=01=>origin bottom left

    // High-level
//    void PutChar(char c);
//    void Printf(uint8_t x, uint8_t y, Color_t ForeClr, Color_t BckClr, const char *S, ...);
    void Cls(Color_t Color);
//    void GetBitmap(uint8_t x0, uint8_t y0, uint8_t Width, uint8_t Height, uint16_t *PBuf);
    void PutBitmap(uint16_t x0, uint16_t y0, uint16_t Width, uint16_t Height, uint16_t *PBuf);
//    void DrawImage(const uint8_t x, const uint8_t y, const uint8_t *Img);
//    void DrawSymbol(const uint8_t x, const uint8_t y, const uint8_t ACode);
    void DrawBmpFile(uint8_t x0, uint8_t y0, const char *Filename, FIL *PFile);
};

extern Lcd_t Lcd;
