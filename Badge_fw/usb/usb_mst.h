/*
 * usb_mst.h
 *
 *  Created on: 2016/01/29 ã.
 *      Author: Kreyl
 */

#pragma once

#include "hal.h"

class UsbMst_t {
private:
public:
    void Init();
    void Connect();
    void Disconnect();
    // Inner use
};

extern UsbMst_t UsbMst;
