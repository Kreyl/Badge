/*
 * kl_fs_common.h
 *
 *  Created on: 30 ���. 2016 �.
 *      Author: Kreyl
 */

#pragma once

#include "ff.h"

extern FATFS FatFS;
extern FIL File;

#define FATFS_IS_OK()   (FatFS.fs_type != 0)
