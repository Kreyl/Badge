/*
 * FlashW25Q64t.cpp
 *
 *  Created on: 28 џэт. 2016 у.
 *      Author: Kreyl
 */

#include <FlashW25Q64t.h>
#include "uart.h"

FlashW25Q64_t Flash;

// GPIO
#define CsHi()      PinSet(MEM_GPIO, MEM_CS)
#define CsLo()      PinClear(MEM_GPIO, MEM_CS)
#define WpHi()      PinSet(MEM_GPIO, MEM_WP)
#define WpLo()      PinClear(MEM_GPIO, MEM_WP)
#define HoldHi()    PinSet(MEM_GPIO, MEM_HOLD)
#define HoldLo()    PinClear(MEM_GPIO, MEM_HOLD)

void FlashW25Q64_t::Init() {
    // GPIO
    PinSetupOut(MEM_GPIO, MEM_CS, omPushPull);
    PinSetupOut(MEM_GPIO, MEM_WP, omPushPull);
    PinSetupOut(MEM_GPIO, MEM_HOLD, omPushPull);
    PinSetupAlterFunc(MEM_GPIO, MEM_CLK,  omPushPull, pudNone, MEM_SPI_AF);
    PinSetupAlterFunc(MEM_GPIO, MEM_DO, omPushPull, pudNone,   MEM_SPI_AF);
    PinSetupAlterFunc(MEM_GPIO, MEM_DI, omPushPull, pudNone,   MEM_SPI_AF);
    // Initial values
    CsHi();     // Disable IC
    WpHi();     // Write protect disable
    HoldHi();   // Hold disable
    // ==== SPI ====    MSB first, master, ClkLowIdle, FirstEdge, Baudrate=f/2
    ISpi.Setup(MEM_SPI, boMSB, cpolIdleLow, cphaFirstEdge, sbFdiv2);
    ISpi.Enable();

    ReleasePWD();



}

uint8_t FlashW25Q64_t::ReleasePWD() {
    CsLo();
    ISpi.ReadWriteByte(0xAB);   // Send cmd code
    ISpi.ReadWriteByte(0x00);   // }
    ISpi.ReadWriteByte(0x00);   // }
    ISpi.ReadWriteByte(0x00);   // } Three dummy bytes
    uint8_t id = ISpi.ReadWriteByte(0x00);
    CsHi();
    chThdSleepMilliseconds(1);  // Let it to wake
    if(id == 0x16) return OK;
    else {
        Uart.Printf("Flash ID Error(0x%X)\r", id);
        return FAILURE;
    }
}
