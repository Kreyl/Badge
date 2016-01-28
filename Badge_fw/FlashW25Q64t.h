/*
 * FlashW25Q64t.h
 *
 *  Created on: 28 ���. 2016 �.
 *      Author: Kreyl
 */

#pragma once

#include "ch.h"
#include "hal.h"
#include "kl_lib.h"
#include "board.h"

class FlashW25Q64_t {
private:
    Spi_t ISpi;
    uint8_t ReleasePWD();
public:
    void Init();
};

extern FlashW25Q64_t Flash;
