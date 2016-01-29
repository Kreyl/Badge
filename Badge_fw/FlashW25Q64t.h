/*
 * FlashW25Q64t.h
 *
 *  Created on: 28 џэт. 2016 у.
 *      Author: Kreyl
 */

#pragma once

#include "ch.h"
#include "hal.h"
#include "kl_lib.h"
#include "board.h"

#define MEM_PAGE_SZ     256 // Defined in datasheet
#define MEM_TIMEOUT     45000000

class FlashW25Q64_t {
private:
    Spi_t ISpi;
    uint8_t ReleasePWD();
    void WriteEnable();
    uint8_t BusyWait();

public:
    uint8_t Init();
    uint8_t Read(uint32_t Addr, uint8_t *PBuf, uint32_t ALen);
    uint8_t WritePage(uint32_t Addr, uint8_t *PBuf, uint32_t ALen);
    uint8_t EraseBlock4k(uint32_t Addr);

    uint8_t ReadStatusReg1();
    void ReadJEDEC();
    void ReadManufDevID();
};

extern FlashW25Q64_t Mem;
