/*
 * FlashW25Q64t.cpp
 *
 *  Created on: 28 џэт. 2016 у.
 *      Author: Kreyl
 */

#include <FlashW25Q64t.h>
#include "uart.h"
#include "kl_lib.h"

FlashW25Q64_t Mem;

// GPIO
#define CsHi()      PinSet(MEM_GPIO, MEM_CS)
#define CsLo()      PinClear(MEM_GPIO, MEM_CS)
#define WpHi()      PinSet(MEM_GPIO, MEM_WP)
#define WpLo()      PinClear(MEM_GPIO, MEM_WP)
#define HoldHi()    PinSet(MEM_GPIO, MEM_HOLD)
#define HoldLo()    PinClear(MEM_GPIO, MEM_HOLD)

uint8_t FlashW25Q64_t::Init() {
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
    // Initialization cmds
    if(ReleasePWD() != OK) return FAILURE;
    return OK;
}

uint8_t FlashW25Q64_t::ReleasePWD() {
    CsLo();
    ISpi.ReadWriteByte(0xAB);   // Send cmd code
    ISpi.ReadWriteByte(0x00);   // }
    ISpi.ReadWriteByte(0x00);   // }
    ISpi.ReadWriteByte(0x00);   // } Three dummy bytes
    uint8_t id = ISpi.ReadWriteByte(0x00);
    CsHi();
    chThdSleepMilliseconds(1);  // Let it wake
    if(id == 0x16) return OK;
    else {
        Uart.Printf("Flash ID Error(0x%X)\r", id);
        return FAILURE;
    }
}

uint8_t FlashW25Q64_t::Read(uint32_t Addr, uint8_t *PBuf, uint32_t ALen) {
    CsLo();
    ISpi.ReadWriteByte(0x03);   // Send cmd code
    // Send addr
    ISpi.ReadWriteByte((Addr >> 16) & 0xFF);
    ISpi.ReadWriteByte((Addr >> 8) & 0xFF);
    ISpi.ReadWriteByte(Addr & 0xFF);
    // Read data
    for(uint32_t i=0; i < ALen; i++) {
        *PBuf = ISpi.ReadWriteByte(0);
        PBuf++;
    }
    CsHi();
    return OK;
}

uint8_t FlashW25Q64_t::WritePage(uint32_t Addr, uint8_t *PBuf, uint32_t ALen) {
    WriteEnable();
    CsLo();
    ISpi.ReadWriteByte(0x02);   // Send cmd code
    // Send addr
    ISpi.ReadWriteByte((Addr >> 16) & 0xFF);
    ISpi.ReadWriteByte((Addr >> 8) & 0xFF);
    ISpi.ReadWriteByte(Addr & 0xFF);
    // Write data
    for(uint32_t i=0; i < ALen; i++) {
        ISpi.ReadWriteByte(*PBuf);
        PBuf++;
    }
    CsHi();
    return BusyWait(); // Wait completion
}

uint8_t FlashW25Q64_t::EraseBlock4k(uint32_t Addr) {
    WriteEnable();
    CsLo();
    ISpi.ReadWriteByte(0x20);   // Send cmd code
    // Send addr
    ISpi.ReadWriteByte((Addr >> 16) & 0xFF);
    ISpi.ReadWriteByte((Addr >> 8) & 0xFF);
    ISpi.ReadWriteByte(Addr & 0xFF);
    CsHi();
    return BusyWait(); // Wait completion
}

void FlashW25Q64_t::WriteEnable() {
    CsLo();
    ISpi.ReadWriteByte(0x06);
    CsHi();
}

uint8_t FlashW25Q64_t::ReadStatusReg1() {
    CsLo();
    ISpi.ReadWriteByte(0x05);
    uint8_t r = ISpi.ReadWriteByte(0);
    CsHi();
    return r;
}

uint8_t FlashW25Q64_t::BusyWait() {
    uint32_t t = MEM_TIMEOUT;
    CsLo();
    ISpi.ReadWriteByte(0x05);   // Read status reg
    while(t--) {
        uint8_t r = ISpi.ReadWriteByte(0);
//        Uart.Printf(">%X\r", r);
        if((r & 0x01) == 0) break;  // BUSY bit == 0
    }
    CsHi();
    if(t == 0) return TIMEOUT;
    else return OK;
}


void FlashW25Q64_t::ReadJEDEC() {
    uint8_t r;
    CsLo();
    ISpi.ReadWriteByte(0x9F);
    r = ISpi.ReadWriteByte(0);
    Uart.Printf("ManufID: 0x%X\r", r);
    r = ISpi.ReadWriteByte(0);
    Uart.Printf("MemType: 0x%X\r", r);
    r = ISpi.ReadWriteByte(0);
    Uart.Printf("Capacity: 0x%X\r", r);
    CsHi();
}

void FlashW25Q64_t::ReadManufDevID() {
    uint8_t r;
    CsLo();
    ISpi.ReadWriteByte(0x90);   // Cmd
    ISpi.ReadWriteByte(0);      // }
    ISpi.ReadWriteByte(0);      // }
    ISpi.ReadWriteByte(0);      // } Addr == 0
    r = ISpi.ReadWriteByte(0);
    Uart.Printf("ManufID: 0x%X\r", r);
    r = ISpi.ReadWriteByte(0);
    Uart.Printf("MemType: 0x%X\r", r);
    CsHi();
}
