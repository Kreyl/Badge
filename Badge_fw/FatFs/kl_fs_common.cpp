/*
 * kl_fs_common.cpp
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#include "kl_fs_common.h"
#include "kl_lib.h"
#include "FlashW25Q64t.h"
#include "uart.h"

FATFS FatFS;
FIL File;
DIR Dir;
FILINFO FileInfo;

uint8_t TryInitFS() {
    Mem.Reset();
    FRESULT rslt = f_mount(&FatFS, "", 1); // Mount it now
    if(rslt == FR_OK)  {
        Uart.Printf("FS OK\r");
        return OK;
    }
    else {
        Uart.Printf("FS mount error: %u\r", rslt);
        return FAILURE;
    }
}

uint8_t TryOpenFileRead(const char *Filename, FIL *PFile) {
    FRESULT rslt = f_open(PFile, Filename, FA_READ);
    if(rslt == FR_OK) return OK;
    else {
        if (rslt == FR_NO_FILE) Uart.Printf("%S: not found\r", Filename);
        else Uart.Printf("OpenFile error: %u\r", rslt);
        return FAILURE;
    }
}

uint8_t CheckFileNotEmpty(FIL *PFile) {
    if(PFile->fsize == 0) {
        Uart.Printf("Empty file\r");
        return EMPTY;
    }
    else return OK;
}
