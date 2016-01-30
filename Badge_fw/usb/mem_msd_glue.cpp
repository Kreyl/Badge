/*
 * mem_msd_glue.cpp
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#include "mem_msd_glue.h"
#include "uart.h"

uint8_t MSDRead(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt) {
    for(uint32_t i=0; i < 4096; i++) {
        *Ptr = (uint8_t)i;
        Ptr++;
    }
    return OK;
}

uint8_t MSDWrite(uint32_t BlockAddress, uint8_t *Ptr, uint32_t BlocksCnt) {
    for(uint32_t i = 0; i < (BlocksCnt*4096/128); i++) {
        Uart.Printf("WR: %A\r", Ptr, 128, ' ');
        Ptr += 128;
    }
    return OK;
}
