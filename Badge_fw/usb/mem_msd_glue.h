/*
 * mem_msd_glue.h
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#pragma once

#include "FlashW25Q64t.h"

#define MSD_BLOCK_CNT   MEM_BLOCK_CNT
#define MSD_BLOCK_SZ    MEM_BLOCK_SZ

uint8_t MSDRead(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt);
uint8_t MSDWrite(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt);
