/*
 * board.h
 *
 *  Created on: 12 ����. 2015 �.
 *      Author: Kreyl
 */

#pragma once

#include <inttypes.h>

// ==== General ====
#define BOARD_NAME          "Badge3"
// MCU type as defined in the ST header.
#define STM32F072xB

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ     12000000

#define SYS_TIM_CLK         (Clk.APBFreqHz * Clk.TimerClkMulti)

#if 1 // ========================== GPIO =======================================
// UART
#define UART_GPIO       GPIOA
#define UART_TX_PIN     9
#define UART_RX_PIN     10
#define UART_AF         AF1

// USB
#define USB_GPIO		GPIOA
#define USB_DM_PIN		11
#define USB_DP_PIN		12
#define SNS_5V_GPIO     GPIOF
#define SNS_5V_PIN      0

// Button
#define BTN_GPIO        GPIOA
#define BTN_PIN         0

// Battery Management
#define BAT_MEAS_GPIO   GPIOA
#define BAT_MEAS_PIN    1
#define BAT_SW_GPIO     GPIOA
#define BAT_SW_PIN      15
#define BAT_CHARGE_GPIO GPIOC
#define BAT_CHARGE_PIN  14

#if 1 // ==== LCD ====
#define LCD_GPIO        GPIOB
#define LCD_RST         8
#define LCD_CS          9
#define LCD_RS          10
#define LCD_WR          11
#define LCD_RD          12
#define LCD_PWR         13
#define LCD_MODE_MSK_READ   0xFFFF0000
#define LCD_MODE_MSK_WRITE  0x00005555
#define LCD_BCKLT_GPIO  GPIOB
#define LCD_BCKLT_PIN1  14
#define LCD_BCKLT_PIN2  15
#endif // LCD

#if 1 // ==== Flash W25Q64 ====
#define MEM_GPIO        GPIOA
#define MEM_CS          4
#define MEM_CLK         5
#define MEM_DI          7
#define MEM_DO          6
#define MEM_WP          3
#define MEM_HOLD        2
#define MEM_PWR_GPIO    GPIOC
#define MEM_PWR         13
#define MEM_SPI_AF      AF0
#endif // Flash

#endif // GPIO

#if 1 // ========================= Timer =======================================
// LCD
#define LCD_BCKLT_TMR   TIM15
#define LCD_BCKLT_CHNL1 1
#define LCD_BCKLT_CHNL2 2
#endif // Timer

#if 1 // =========================== SPI =======================================
#define MEM_SPI         SPI1
#endif

#if 1 // ========================== USART ======================================
#define UART            USART1
#define UART_TX_REG     UART->TDR
#define UART_RX_REG     UART->RDR
#endif

#if 1 // ========================= Inner ADC ===================================
#define ADC_REQUIRED        TRUE
// Clock divider: clock is generated from the APB2
#define ADC_CLK_DIVIDER		adcDiv2

// ADC channels
#define BAT_CHNL 	        1

#define ADC_VREFINT_CHNL    17  // All 4xx and F072 devices. Do not change.
#define ADC_CHANNELS        { BAT_CHNL, ADC_VREFINT_CHNL }
#define ADC_CHANNEL_CNT     2   // Do not use countof(AdcChannels) as preprocessor does not know what is countof => cannot check
#define ADC_SAMPLE_TIME     ast55d5Cycles
#define ADC_SAMPLE_CNT      8   // How many times to measure every channel

#define ADC_MAX_SEQ_LEN     16  // 1...16; Const, see ref man
#define ADC_SEQ_LEN         (ADC_SAMPLE_CNT * ADC_CHANNEL_CNT)
#if (ADC_SEQ_LEN > ADC_MAX_SEQ_LEN) || (ADC_SEQ_LEN == 0)
#error "Wrong ADC channel count and sample count"
#endif
#endif

#if 1 // ========================== USB ========================================
#define USBDrv          USBD1   // USB driver to use

// CRS
#define CRS_PRESCALER   RCC_CRS_SYNC_DIV1
#define CRS_SOURCE      RCC_CRS_SYNC_SOURCE_USB
#define CRS_POLARITY    RCC_CRS_SYNC_POLARITY_RISING
#define CRS_RELOAD_VAL  ((48000000 / 1000) - 1) // Ftarget / Fsync - 1
#define CRS_ERROR_LIMIT 34
#define HSI48_CALIBRATN 32
#endif // USB

#if 1 // =========================== DMA =======================================
#define STM32_DMA_REQUIRED  TRUE
// ==== Uart ====
// Remap is made automatically if required
#define UART_DMA_TX     STM32_DMA1_STREAM4
#define UART_DMA_RX     STM32_DMA1_STREAM5
#define UART_DMA_CHNL   0   // Dummy

// ==== Memory ====
#define SPI1_DMA_RX     STM32_DMA1_STREAM2

#define MEMCPY_DMA      STM32_DMA1_STREAM3
#define MEM_CPY_DMA_MODE STM32_DMA_CR_CHSEL(0) |    /* dummy */ \
                        STM32_DMA_CR_PL(0b10) |     /* DMA_PRIORITY_HIGH */ \
                        STM32_DMA_CR_MSIZE_BYTE | \
                        STM32_DMA_CR_PSIZE_BYTE

#if ADC_REQUIRED
/* DMA request mapped on this DMA channel only if the corresponding remapping bit is cleared in the SYSCFG_CFGR1
 * register. For more details, please refer to Section10.1.1: SYSCFG configuration register 1 (SYSCFG_CFGR1) on
 * page173 */
#define ADC_DMA         STM32_DMA1_STREAM1
#define ADC_DMA_MODE    STM32_DMA_CR_CHSEL(0) |   /* DMA2 Stream4 Channel0 */ \
                        DMA_PRIORITY_LOW | \
                        STM32_DMA_CR_MSIZE_HWORD | \
                        STM32_DMA_CR_PSIZE_HWORD | \
                        STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                        STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */
#endif // ADC

#endif // DMA
