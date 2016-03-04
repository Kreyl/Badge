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
    if(rslt == FR_OK) {
        // Check if zero file
        if(PFile->fsize == 0) {
            f_close(PFile);
            Uart.Printf("Empty file %S\r", Filename);
            return EMPTY;
        }
        return OK;
    }
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

uint8_t TryRead(FIL *PFile, void *Ptr, uint32_t Sz) {
    uint32_t ReadSz=0;
    uint8_t r = f_read(PFile, Ptr, Sz, &ReadSz);
    return (r == FR_OK and ReadSz == Sz)? OK : FAILURE;
}

uint8_t ReadLine(FIL *PFile, char* S, uint32_t MaxLen) {
    uint32_t Len = 0, Rcv;
    char c, str[2];
    while(Len < MaxLen-1) {
        f_read(PFile, str, 1, &Rcv);
        if(Rcv != 1) break;
        c = str[0];
        if(c == '\r' or c == '\n') {    // End of line
            *S = '\0';
            return OK;
        }
        else {
            *S++ = c;
            Len++;
        }
    } // while
    *S = '\0';
    return (Len > 0)? OK : FAILURE;
}
