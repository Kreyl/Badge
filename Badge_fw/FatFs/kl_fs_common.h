/*
 * kl_fs_common.h
 *
 *  Created on: 30 џэт. 2016 у.
 *      Author: Kreyl
 */

#pragma once

#include "ff.h"

extern FATFS FatFS;
extern FIL File;
extern DIR Dir;
extern FILINFO FileInfo;

#define FATFS_IS_OK()   (FatFS.fs_type != 0)

uint8_t TryInitFS();
uint8_t TryOpenFileRead(const char *Filename, FIL *PFile);
uint8_t CheckFileNotEmpty(FIL *PFile);
uint8_t TryRead(FIL *PFile, void *Ptr, uint32_t Sz);

uint8_t ReadLine(FIL *PFile, char* S, uint32_t MaxLen);
