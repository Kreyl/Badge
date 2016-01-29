/*
 * usb_cdc.cpp
 *
 *  Created on: 03 сент. 2015 г.
 *      Author: Kreyl
 */

#include "descriptors_mst.h"
#include "usb_mst.h"
#include "usb.h"
#include "main.h"
#include "board.h"

UsbMst_t UsbMst;

static bool OnSetupPkt(USBDriver *usbp);
static void OnDataTransmitted(USBDriver *usbp, usbep_t ep) { }
static void OnDataReceived(USBDriver *usbp, usbep_t ep) { }


#if 1 // ========================== Endpoints ==================================
// ==== EP1 ====
static USBInEndpointState ep1instate;
static USBOutEndpointState ep1outstate;

// EP1 initialization structure (both IN and OUT).
static const USBEndpointConfig ep1config = {
    USB_EP_MODE_TYPE_BULK,
    NULL,                   // setup_cb
    OnDataTransmitted,      // in_cb
    OnDataReceived,         // out_cb
    64,                     // in_maxsize
    64,                     // out_maxsize
    &ep1instate,            // in_state
    &ep1outstate,           // out_state
    2,                      // in_multiplier: Determines the space allocated for the TXFIFO as multiples of the packet size
    NULL                    // setup_buf: Pointer to a buffer for setup packets. Set this field to NULL for non-control endpoints
};
#endif

#if 1 // ============================ Events ===================================
static void usb_event(USBDriver *usbp, usbevent_t event) {
    switch (event) {
        case USB_EVENT_RESET:
            return;
        case USB_EVENT_ADDRESS:
            return;
        case USB_EVENT_CONFIGURED:
            chSysLockFromISR();
            /* Enable the endpoints specified in the configuration.
            Note, this callback is invoked from an ISR so I-Class functions must be used.*/
            usbInitEndpointI(usbp, EP_DATA_IN_ID,  &ep1config);
            usbInitEndpointI(usbp, EP_DATA_OUT_ID, &ep1config);
            App.SignalEvtI(EVTMSK_USB_READY);
            chSysUnlockFromISR();
            return;
        case USB_EVENT_SUSPEND:
            return;
        case USB_EVENT_WAKEUP:
            return;
        case USB_EVENT_STALLED:
            return;
    } // switch
}

#endif

#if 1  // ==== USB driver configuration ====
const USBConfig UsbCfg = {
    usb_event,          // This callback is invoked when an USB driver event is registered
    GetDescriptor,      // Device GET_DESCRIPTOR request callback
    OnSetupPkt,         // This hook allows to be notified of standard requests or to handle non standard requests
    NULL                // Start Of Frame callback
};
#endif

struct SetupPkt_t {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

/* ==== Setup Packet handler ====
 * true         Message handled internally.
 * false        Message not handled. */
bool OnSetupPkt(USBDriver *usbp) {
//    SetupPkt_t *Setup = (SetupPkt_t*)usbp->setup;
//    Uart.PrintfI("%X %X %X %X %X\r", Setup->bmRequestType, Setup->bRequest, Setup->wValue, Setup->wIndex, Setup->wLength);
    return false;
}


#if 0 // ========================== RX Thread ==================================
static THD_WORKING_AREA(waThdCDCRX, 128);
static THD_FUNCTION(ThdCDCRX, arg) {
    chRegSetThreadName("CDCRX");
    while (true) {
        if(UsbCDC.IsActive()) {
            msg_t m = UsbCDC.SDU1.vmt->get(&UsbCDC.SDU1);
            if(m > 0) {
//                UsbCDC.SDU1.vmt->put(&UsbCDC.SDU1, (uint8_t)m);   // repeat what was sent
                if(UsbCDC.Cmd.PutChar((char)m) == pdrNewCmd) {
                    chSysLock();
                    App.SignalEvtI(EVTMSK_USB_NEW_CMD);
                    chSchGoSleepS(CH_STATE_SUSPENDED); // Wait until cmd processed
                    chSysUnlock();  // Will be here when application signals that cmd processed
                }
            } // if >0
        } // if active
        else chThdSleepMilliseconds(540);
    } // while true
}

#endif

void UsbMst_t::Init() {
    PinSetupAnalog(GPIOA, 11);
    PinSetupAnalog(GPIOA, 12);
    // Objects
    usbInit();
}

void UsbMst_t::Connect() {
    usbDisconnectBus(&USBDrv);
    chThdSleepMilliseconds(1500);
    usbStart(&USBDrv, &UsbCfg);
    usbConnectBus(&USBDrv);
}
