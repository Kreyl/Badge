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

#define FATFS_IS_OK()   (FatFS.fs_type != 0)
