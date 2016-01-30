/*
 * mem_msd_glue.cpp
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#include "mem_msd_glue.h"
#include "uart.h"

uint8_t MSDRead(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt) {
//    Uart.Printf("R Addr: %u; Cnt: %u\r", BlockAddress, BlocksCnt);
    Mem.Read(BlockAddress * MEM_BLOCK_SZ, Ptr, BlocksCnt * MEM_BLOCK_SZ);
    return OK;
}

uint8_t MSDWrite(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt) {
    Uart.Printf("WRT Addr: %u; Cnt: %u\r", BlockAddress, BlocksCnt);
//    for(uint32_t i = 0; i < (BlocksCnt*4096/128); i++) {
//        Uart.Printf("WR: %A\r", Ptr, 128, ' ');
//        Ptr += 128;
//    }
    for(uint32_t i=0; i<BlocksCnt; i++) {   // One block at a time
        uint32_t ByteAddr = (BlockAddress + i) * MEM_BLOCK_SZ;
        // Erase block
        if(Mem.EraseBlock4k(ByteAddr) != OK) return FAILURE;
        // Write pages of the block
        for(uint32_t j=0; j<MEM_PAGES_IN_BLOCK_CNT; j++) {
            if(Mem.WritePage(ByteAddr, Ptr, MEM_PAGE_SZ) != OK) return FAILURE;
            ByteAddr += MEM_PAGE_SZ;
            Ptr += MEM_PAGE_SZ;
        } // j
    } // i
    return OK;
}
