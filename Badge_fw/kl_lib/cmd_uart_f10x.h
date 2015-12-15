/*
 * cmd_uart.h
 *
 *  Created on: 15.04.2013
 *      Author: kreyl
 */

#ifndef CMD_UART_H_
#define CMD_UART_H_

#include "stm32f10x.h"
#include "ch.h"
#include "hal.h"
#include "kl_lib_f100.h"
#include "kl_sprintf.h"
#include "cmd.h"

// Set to true if RX needed
#define UART_RX_ENABLED     FALSE

// UART
#define UART_TXBUF_SIZE     369
#define UART                USART1
#define UART_GPIO           GPIOA
#define UART_TX_PIN         9
#define UART_DMA_CHNL       4
#define UART_RCC_ENABLE()   rccEnableUSART1(FALSE)

#define UART_DMA_TX         STM32_DMA1_STREAM4
#define UART_DMA_TX_MODE    STM32_DMA_CR_CHSEL(UART_DMA_CHNL) | \
                            DMA_PRIORITY_LOW | \
                            STM32_DMA_CR_MSIZE_BYTE | \
                            STM32_DMA_CR_PSIZE_BYTE | \
                            STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                            STM32_DMA_CR_DIR_M2P |    /* Direction is memory to peripheral */ \
                            STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */

#if UART_RX_ENABLED
#define UART_RX_PIN         10
#define UART_RX_REG         UART->DR
#define UART_RXBUF_SZ       387 // unprocessed bytes

#define UART_RX_POLLING_MS  99
#define UART_DMA_RX         STM32_DMA1_STREAM5
#define UART_DMA_RX_MODE    DMA_PRIORITY_LOW | \
                            STM32_DMA_CR_MSIZE_BYTE | \
                            STM32_DMA_CR_PSIZE_BYTE | \
                            STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                            STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                            STM32_DMA_CR_CIRC         /* Circular buffer enable */

// Cmd related
#define CDC_CMD_BUF_SZ  378 // payload bytes
typedef Cmd_t<CDC_CMD_BUF_SZ> UartCmd_t;
#endif

class CmdUart_t {
private:
    char TXBuf[UART_TXBUF_SIZE];
    char *PWrite, *PRead;
    bool IDmaIsIdle;
    uint32_t IFullSlotsCount, ITransSize;
    void IPrintf(const char *format, va_list args);
    void ISendViaDMA();
#if UART_RX_ENABLED
    int32_t SzOld=0, RIndx=0;
    uint8_t IRxBuf[UART_RXBUF_SZ];
#endif
public:
    void Printf(const char *S, ...);
    void PrintfI(const char *S, ...);
    void FlushTx() { while(!IDmaIsIdle); }  // wait DMA
    void PrintNow(const char *S) {
        while(*S != 0) {
            while(!(UART->SR & USART_SR_TXE));
            UART->DR = *S++;
        }
    }
    void Init(uint32_t ABaudrate);
    // Inner use
    void IRQDmaTxHandler();
    void IPutChar(char c);
#if UART_RX_ENABLED
    void PollRx(ftVoidVoid FCallBack);
    UartCmd_t Cmd;
    void Ack(uint8_t rslt) { Printf("#Ack %u\r\n", rslt); }
#endif
};

extern CmdUart_t Uart;

#endif /* CMD_UART_H_ */
