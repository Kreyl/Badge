/*
 * evt_mask.h
 *
 *  Created on: Apr 12, 2013
 *      Author: g.kruglov
 */

#pragma once

// Event masks
#define EVT_UART_NEW_CMD     EVENT_MASK(1)

#define EVT_BUTTONS          EVENT_MASK(2)

#define EVT_LCD_DRAW_DONE    EVENT_MASK(3)

#define EVT_SAMPLING         EVENT_MASK(4)
#define EVT_ADC_DONE         EVENT_MASK(5)

#define EVT_IMGLIST_TIME     EVENT_MASK(6)

#define EVT_USB_READY        EVENT_MASK(10)
#define EVT_USB_RESET        EVENT_MASK(11)
#define EVT_USB_SUSPEND      EVENT_MASK(12)
#define EVT_USB_CONNECTED    EVENT_MASK(13)
#define EVT_USB_DISCONNECTED EVENT_MASK(14)
#define EVT_USB_IN_DONE      EVENT_MASK(15)
#define EVT_USB_OUT_DONE     EVENT_MASK(16)
