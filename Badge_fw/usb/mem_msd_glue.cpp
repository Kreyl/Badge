/*
 * mem_msd_glue.cpp
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#include "mem_msd_glue.h"
#include "uart.h"

//static uint8_t BufSector[MEM_SECTOR_SZ];
#define BLOCK_SZ_32     (512/4)

uint8_t MSDRead(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt) {
    //Uart.Printf("R Addr: %u; Cnt: %u\r", BlockAddress, BlocksCnt);
    Mem.Read(BlockAddress * MSD_BLOCK_SZ, Ptr, BlocksCnt * MSD_BLOCK_SZ);
    return OK;
}

uint8_t MSDWrite(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt) {
    Uart.Printf("WRT Addr: %u; Cnt: %u\r", BlockAddress, BlocksCnt);
    while(BlocksCnt != 0) {
        // Determine Mem Sector addr
        uint32_t SectorStartAddr = BlockAddress * MEM_SECTOR_SZ;
//        uint32_t ByteIndx = (BlockAddress * MSD_BLOCK_SZ) - SectorStartAddr; // Byte indx in buffer
//        Uart.Printf("SSA=%u; BI=%u\r", SectorStartAddr, ByteIndx);
//        // Read Mem Sector
//        Mem.Read(SectorStartAddr, BufSector, MEM_SECTOR_SZ);
//        // Modify buffer
//        for(uint32_t i=0; i < MSD_BLOCK_SZ; i++) {
//            BufSector[ByteIndx + i] = *Ptr++;
//        }
        // Write renewed sector
        if(Mem.EraseWriteSector4k(SectorStartAddr, Ptr) != OK) return FAILURE;
        // Process variables
        BlockAddress += MSD_BLOCK_SZ;
        BlocksCnt--;
    }
    return OK;
}
