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
    ISpi.Enable();
    chBSemObjectInit(&ISemaphore, NOT_TAKEN);

    // DMA
    dmaStreamAllocate     (SPI1_DMA_RX, IRQ_PRIO_LOW, MemDmaEndIrq, NULL);
    dmaStreamSetPeripheral(SPI1_DMA_RX, &MEM_SPI->DR);

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
    ISendCmdAndAddr(0x03, Addr);    // Cmd Read
    // ==== Read Data ====
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
    ISpi.SetFullDuplex();   // Remove read-only mode
    ISpi.ClearRxBuf();

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
        ISendCmdAndAddr(0x02, Addr);    // Cmd Write
        // Write data
        for(uint32_t j=0; j < MEM_PAGE_SZ; j++) {
            ISpi.ReadWriteByte(*PBuf);
            PBuf++;
        }
        CsHi();
        ISpi.ClearRxBuf();
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
void FlashW25Q64_t::ISendCmdAndAddr(uint8_t Cmd, uint32_t Addr) {
    Convert::DWordBytes_t dwb;
    dwb.DWord = Addr;
    ReverseByteOrder32(dwb.DWord);
    dwb.b[0] = Cmd;
    for(uint8_t i=0; i<4; i++) ISpi.ReadWriteByte(dwb.b[i]);
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
    ISendCmdAndAddr(0x20, Addr);
    CsHi();
    return BusyWait(); // Wait completion
}

void FlashW25Q64_t::WriteEnable() {
    CsLo();
    ISpi.ReadWriteByte(0x06);
    CsHi();
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
#endif // Inner
