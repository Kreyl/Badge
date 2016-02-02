/*
 * FlashW25Q64t.h
 *
 *  Created on: 28 ���. 2016 �.
 *      Author: Kreyl
 */

#pragma once

#include "ch.h"
#include "hal.h"
#include "kl_lib.h"
#include "board.h"

#define MEM_TIMEOUT     45000000

#define MEM_PAGE_SZ     256  // Defined in datasheet
#define MEM_PAGE_CNT    32768
#define MEM_PAGES_IN_SECTOR_CNT  16
#define MEM_SECTOR_SZ   4096 // 16 pages
#define MEM_SECTOR_CNT  2048 // 2048 blocks of 16 pages each

// DMA
#define MEM_RX_DMA_MODE STM32_DMA_CR_CHSEL(0) |   /* dummy */ \
                        DMA_PRIORITY_HIGH | \
                        STM32_DMA_CR_MSIZE_BYTE | \
                        STM32_DMA_CR_PSIZE_BYTE | \
                        STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                        STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */

#define MEM_TX_DMA_MODE STM32_DMA_CR_CHSEL(0) |   /* dummy */ \
                        DMA_PRIORITY_HIGH | \
                        STM32_DMA_CR_MSIZE_BYTE | \
                        STM32_DMA_CR_PSIZE_BYTE | \
                        STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_M2P | \
                        STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */

class FlashW25Q64_t {
private:
    Spi_t ISpi;
    binary_semaphore_t ISemaphore;
    uint8_t ReleasePWD();
    void WriteEnable();
    uint8_t BusyWait();
    uint8_t WritePage(uint32_t Addr, uint8_t *PBuf, uint32_t ALen);
    uint8_t EraseSector4k(uint32_t Addr);
    uint8_t ReadStatusReg1();
    void ReadJEDEC();
    void ReadManufDevID();
    // Data
    void ITxData(uint8_t *Ptr, uint32_t Len);
    void IRxData(uint8_t *Ptr, uint32_t Len);
    void ISendCmdAndAddr(uint8_t Cmd, uint32_t Addr);
public:
    bool IsReady = false;
    uint8_t Init();
    uint8_t Read(uint32_t Addr, uint8_t *PBuf, uint32_t ALen);
    uint8_t EraseAndWriteSector4k(uint32_t Addr, uint8_t *PBuf);
};

extern FlashW25Q64_t Mem;
