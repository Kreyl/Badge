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
static thread_reference_t trp = NULL;

// GPIO
#define CsHi()      PinSet(MEM_GPIO, MEM_CS)
#define CsLo()      PinClear(MEM_GPIO, MEM_CS)
#define WpHi()      PinSet(MEM_GPIO, MEM_WP)
#define WpLo()      PinClear(MEM_GPIO, MEM_WP)
#define HoldHi()    PinSet(MEM_GPIO, MEM_HOLD)
#define HoldLo()    PinClear(MEM_GPIO, MEM_HOLD)

// Wrapper for RX IRQ
extern "C" {
void MemDmaEndIrq(void *p, uint32_t flags) {
    chSysLockFromISR();
    chThdResumeI(&trp, (msg_t)0);
    chSysUnlockFromISR();
}
}

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
    ISpi.EnableRxDma();
    ISpi.EnableTxDma();
    ISpi.Enable();
    chBSemObjectInit(&ISemaphore, NOT_TAKEN);

    // DMA
    dmaStreamAllocate     (SPI1_DMA_RX, IRQ_PRIO_LOW, MemDmaEndIrq, NULL);
    dmaStreamSetPeripheral(SPI1_DMA_RX, &MEM_SPI->DR);
    dmaStreamAllocate     (SPI1_DMA_TX, IRQ_PRIO_LOW, MemDmaEndIrq, NULL);
    dmaStreamSetPeripheral(SPI1_DMA_TX, &MEM_SPI->DR);

    // Initialization cmds
    if(ReleasePWD() != OK) return FAILURE;
    IsReady = true;
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

#if 1 // ========================= Exported methods ============================
uint8_t FlashW25Q64_t::Read(uint32_t Addr, uint8_t *PBuf, uint32_t ALen) {
    // Take semaphore
    if(chBSemWait(&ISemaphore) != MSG_OK) return FAILURE;
    // Proceed with reading
    CsLo();
    // ==== Send Cmd & Addr ====
    Convert::DWordBytes_t dwb;
    dwb.DWord = Addr;
    ReverseByteOrder32(dwb.DWord);
    dwb.b[0] = 0x03;    // Cmd Read
    ISpi.Enable();
    ITxData(dwb.b, 4);
    // ==== Read Data ====
    ISpi.ClearRxBuf();
    ISpi.Disable();
    ISpi.SetRxOnly();   // Will not set if enabled
    dmaStreamSetMemory0(SPI1_DMA_RX, PBuf);
    dmaStreamSetTransactionSize(SPI1_DMA_RX, ALen);
    dmaStreamSetMode   (SPI1_DMA_RX, MEM_RX_DMA_MODE);
    // Start
    chSysLock();
    dmaStreamEnable    (SPI1_DMA_RX);
    ISpi.Enable();
    chThdSuspendS(&trp);    // Wait IRQ
    chSysUnlock();
    dmaStreamDisable(SPI1_DMA_RX);
    CsHi();
    ISpi.Disable();
    ISpi.SetFullDuplex();

    chBSemSignal(&ISemaphore);  // Release semaphore
    return OK;
}

// Len = MEM_SECTOR_SZ = 4096
uint8_t FlashW25Q64_t::EraseAndWriteSector4k(uint32_t Addr, uint8_t *PBuf) {
    // Take semaphore
    if(chBSemWait(&ISemaphore) != MSG_OK) return FAILURE;
    // First, erase sector
    uint8_t rslt = EraseSector4k(Addr);
    if(rslt != OK) goto end;
    // Write 4k page by page
    for(uint32_t i=0; i < MEM_PAGES_IN_SECTOR_CNT; i++) {
        WriteEnable();
        CsLo();
        ISpi.ReadWriteByte(0x02);   // Send cmd code
        // Send addr
        ISpi.ReadWriteByte((Addr >> 16) & 0xFF);
        ISpi.ReadWriteByte((Addr >> 8) & 0xFF);
        ISpi.ReadWriteByte(Addr & 0xFF);
        // Write data
        for(uint32_t j=0; j < MEM_PAGE_SZ; j++) {
            ISpi.ReadWriteByte(*PBuf);
            PBuf++;
        }
        CsHi();
        // Wait completion
        rslt = BusyWait();
        if(rslt != OK) goto end;
        Addr += MEM_PAGE_SZ;
    } // for
    end:
    chBSemSignal(&ISemaphore);  // Release semaphore
    return rslt;
}
#endif // Exported

#if 1 // =========================== Inner methods =============================
void FlashW25Q64_t::ITxData(uint8_t *Ptr, uint32_t Len) {
    dmaStreamSetMemory0   (SPI1_DMA_TX, Ptr);
    dmaStreamSetTransactionSize(SPI1_DMA_TX, Len);
    dmaStreamSetMode      (SPI1_DMA_TX, MEM_TX_DMA_MODE);
    chSysLock();
    dmaStreamEnable       (SPI1_DMA_TX);
    chThdSuspendS(&trp);
    chSysUnlock();
    dmaStreamDisable(SPI1_DMA_TX);
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

uint8_t FlashW25Q64_t::EraseSector4k(uint32_t Addr) {
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
#endif // Inner
