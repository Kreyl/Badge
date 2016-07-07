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
#define CsHi()          PinSet(MEM_GPIO, MEM_CS)
#define CsLo()          PinClear(MEM_GPIO, MEM_CS)
#define WpHi()          PinSet(MEM_GPIO, MEM_WP)
#define WpLo()          PinClear(MEM_GPIO, MEM_WP)
#define HoldHi()        PinSet(MEM_GPIO, MEM_HOLD)
#define HoldLo()        PinClear(MEM_GPIO, MEM_HOLD)
#define MemPwrOn()      PinClear(MEM_PWR_GPIO, MEM_PWR)
#define MemPwrOff()     PinSet(MEM_PWR_GPIO, MEM_PWR)
#define MemIsPwrOn()    PinIsClear(MEM_PWR_GPIO, MEM_PWR)

// Wrapper for RX IRQ
extern "C" {
void MemDmaEndIrq(void *p, uint32_t flags) {
    chSysLockFromISR();
    chThdResumeI(&trp, (msg_t)0);
    chSysUnlockFromISR();
}
}

void FlashW25Q64_t::Init() {
    // GPIO
    PinSetupOut(MEM_GPIO, MEM_CS, omPushPull, pudNone, psHigh);
    PinSetupOut(MEM_GPIO, MEM_WP, omPushPull, pudNone, psHigh);
    PinSetupOut(MEM_GPIO, MEM_HOLD, omPushPull, pudNone, psHigh);
    PinSetupOut(MEM_PWR_GPIO, MEM_PWR, omPushPull, pudNone, psHigh);
    PinSetupAlterFunc(MEM_GPIO, MEM_CLK, omPushPull, pudNone, MEM_SPI_AF, psHigh);
    PinSetupAlterFunc(MEM_GPIO, MEM_DO,  omPushPull, pudNone, MEM_SPI_AF, psHigh);
    PinSetupAlterFunc(MEM_GPIO, MEM_DI,  omPushPull, pudNone, MEM_SPI_AF, psHigh);

    chBSemObjectInit(&BSemaphore, NOT_TAKEN);
    // ==== SPI ====    MSB first, master, ClkLowIdle, FirstEdge, Baudrate=f/2
    ISpi.Setup(MEM_SPI, boMSB, cpolIdleLow, cphaFirstEdge, sbFdiv2);
    ISpi.Enable();
    // DMA
    dmaStreamAllocate     (SPI1_DMA_RX, IRQ_PRIO_LOW, MemDmaEndIrq, NULL);
    dmaStreamSetPeripheral(SPI1_DMA_RX, &MEM_SPI->DR);

    MemPwrOn();
    CsHi();     // Disable IC
    WpLo();     // Write protect enable
    HoldHi();   // Hold disable
    chThdSleepMilliseconds(180);
    Reset();
}

// Actually, this is ReleasePWD command
void FlashW25Q64_t::Reset() {
    while(true) {
        // Reset
        chSysLock();
        CsLo();
        ISpi.ReadWriteByte(0x66);   // Enable Reset
        CsHi();
        __NOP(); __NOP(); __NOP(); __NOP();
        CsLo();
        ISpi.ReadWriteByte(0x99);   // Enable Reset
        CsHi();
        chSysUnlock();

        // Power on sequence
        chThdSleepMilliseconds(270);
        // Send initial cmds
        ISpi.ClearRxBuf();
        chSysLock();
        CsLo();
        ISpi.ReadWriteByte(0xAB);   // Send cmd code
        ISpi.ReadWriteByte(0x00);   // }
        ISpi.ReadWriteByte(0x00);   // }
        ISpi.ReadWriteByte(0x00);   // } Three dummy bytes
        uint8_t id = ISpi.ReadWriteByte(0x00);
        CsHi();
        chSysUnlock();
        chThdSleepMilliseconds(1);  // Let it wake
        Uart.Printf("MemID=%X\r", id);
        if(id == 0x16) {
            IsReady = true;
            ISpi.ClearRxBuf();
            return;
        }
        // ID does not match, retry
    } // while true
}

void FlashW25Q64_t::PowerDown() {
    IsReady = false;
    CsLo();
    WpLo();
    HoldLo();
    MemPwrOff();
}

#if 1 // ========================= Exported methods ============================
uint8_t FlashW25Q64_t::Read(uint32_t Addr, uint8_t *PBuf, uint32_t ALen) {
    chSysLock();
    msg_t msg = chBSemWaitS(&BSemaphore);
    if(msg != MSG_OK) {
        chSysUnlock();
        return FAILURE;
    }
    CsLo();
    // ==== Send Cmd & Addr ====
    ISendCmdAndAddr(0x03, Addr);    // Cmd Read
    // ==== Read Data ====
    ISpi.Disable();
    ISpi.SetRxOnly();   // Will not set if enabled
    ISpi.EnableRxDma();
    dmaStreamSetMemory0(SPI1_DMA_RX, PBuf);
    dmaStreamSetTransactionSize(SPI1_DMA_RX, ALen);
    dmaStreamSetMode   (SPI1_DMA_RX, MEM_RX_DMA_MODE);
    // Start
    dmaStreamEnable    (SPI1_DMA_RX);
    ISpi.Enable();
    chThdSuspendS(&trp);    // Wait IRQ
    dmaStreamDisable(SPI1_DMA_RX);
    CsHi();
    ISpi.Disable();
    ISpi.SetFullDuplex();   // Remove read-only mode
    ISpi.DisableRxDma();
    ISpi.Enable();
    ISpi.ClearRxBuf();
    chBSemSignalI(&BSemaphore);
    chSysUnlock();
    return OK;
}

// Len = MEM_SECTOR_SZ = 4096
uint8_t FlashW25Q64_t::EraseAndWriteSector4k(uint32_t Addr, uint8_t *PBuf) {
    chSysLock();
    msg_t msg = chBSemWaitS(&BSemaphore);
    if(msg != MSG_OK) {
        chSysUnlock();
        return FAILURE;
    }
    WpHi();     // Write protect disable
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
    WpLo();     // Write protect enable
    chBSemSignalI(&BSemaphore);
    chSysUnlock();
    return rslt;
}
#endif // Exported

#if 1 // =========================== Inner methods =============================
void FlashW25Q64_t::ISendCmdAndAddr(uint8_t Cmd, uint32_t Addr) {
    uint8_t *p = (uint8_t*)&Addr;
//    Uart.PrintfI("%X %X %X %X\r", Cmd, p[2], p[1], p[0]);
    ISpi.ReadWriteByte(Cmd);
    ISpi.ReadWriteByte(p[2]);
    ISpi.ReadWriteByte(p[1]);
    ISpi.ReadWriteByte(p[0]);
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
