/*
 * evt_mask.h
 *
 *  Created on: Apr 12, 2013
 *      Author: g.kruglov
 */

#ifndef EVT_MASK_H_
#define EVT_MASK_H_

// Event masks
#define EVTMSK_UART_NEW_CMD     EVENT_MASK(1)

#define EVTMSK_BTN_PRESS        EVENT_MASK(2)

#define EVTMSK_LCD_DRAW_DONE    EVENT_MASK(3)

#define EVTMSK_USB_READY        EVENT_MASK(10)
#define EVTMSK_USB_SUSPEND      EVENT_MASK(11)
#define EVTMSK_USB_CONNECTED    EVENT_MASK(12)
#define EVTMSK_USB_DISCONNECTED EVENT_MASK(14)
#define EVTMSK_USB_IN_DONE      EVENT_MASK(15)
#define EVTMSK_USB_OUT_DONE     EVENT_MASK(16)

#endif /* EVT_MASK_H_ */
