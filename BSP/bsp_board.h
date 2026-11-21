#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "bsp_types.h"

/* ESAD2025，CH32V307VCT6，U10/LQFP100。值来自配套 PCB 的 MCU 焊盘网络。 */
#ifndef BSP_BOARD_LCD_ACTIVE
#define BSP_BOARD_LCD_ACTIVE 1
#endif
#ifndef BSP_BOARD_TOUCH_ACTIVE
#define BSP_BOARD_TOUCH_ACTIVE BSP_BOARD_LCD_ACTIVE
#endif
#if BSP_BOARD_TOUCH_ACTIVE && !BSP_BOARD_LCD_ACTIVE
#error "The LVGL touch driver requires the display."
#endif

#define BSP_PIN(port, bit) ((bsp_pin_t){ GPIO##port, GPIO_Pin_##bit })
#define BSP_PIN_BUTTON_1       BSP_PIN(E, 2)
#define BSP_PIN_BUTTON_2       BSP_PIN(E, 3)
#define BSP_PIN_BUTTON_3       BSP_PIN(E, 4)
#define BSP_PIN_BUTTON_4       BSP_PIN(E, 5)
#define BSP_PIN_ELED1          BSP_PIN(E, 6)
#define BSP_PIN_ELED2          BSP_PIN(C, 13)
#define BSP_PIN_X32KI          BSP_PIN(C, 14)
#define BSP_PIN_X32KO          BSP_PIN(C, 15)
#define BSP_PIN_VOICE_TX       BSP_PIN(C, 0)
#define BSP_PIN_VOICE_RX       BSP_PIN(C, 1)
#define BSP_PIN_OPA_CHN        BSP_PIN(C, 2)
#define BSP_PIN_MIC_ADC        BSP_PIN(C, 3)
#define BSP_PIN_FAN_CONTROL    BSP_PIN(A, 0)
#define BSP_PIN_OPA_OUT        BSP_PIN(A, 1)
#define BSP_PIN_BEEP_CONTROL   BSP_PIN(A, 2)
#define BSP_PIN_HEAT_CONTROL   BSP_PIN(A, 3)
#define BSP_PIN_SPK_DAC        BSP_PIN(A, 4)
#define BSP_PIN_MOTOR_H_A      BSP_PIN(A, 6)
#define BSP_PIN_MOTOR_H_B      BSP_PIN(A, 7)
#define BSP_PIN_LCD_BLK        BSP_PIN(C, 4)
#define BSP_PIN_OPA_CHP        BSP_PIN(C, 5)
#define BSP_PIN_NTC_ADC        BSP_PIN(B, 0)
#define BSP_PIN_BOOT1          BSP_PIN(B, 2)
#define BSP_PIN_LCD_D4         BSP_PIN(E, 7)
#define BSP_PIN_LCD_D5         BSP_PIN(E, 8)
#define BSP_PIN_LCD_D6         BSP_PIN(E, 9)
#define BSP_PIN_LCD_D7         BSP_PIN(E, 10)
#define BSP_PIN_LCD_D8         BSP_PIN(E, 11)
#define BSP_PIN_LCD_D9         BSP_PIN(E, 12)
#define BSP_PIN_LCD_D10        BSP_PIN(E, 13)
#define BSP_PIN_LCD_D11        BSP_PIN(E, 14)
#define BSP_PIN_LCD_D12        BSP_PIN(E, 15)
#define BSP_PIN_BLE_TX         BSP_PIN(B, 10)
#define BSP_PIN_BLE_RX         BSP_PIN(B, 11)
#define BSP_PIN_NFC_NSS        BSP_PIN(B, 12)
#define BSP_PIN_NFC_SCK        BSP_PIN(B, 13)
#define BSP_PIN_NFC_MISO       BSP_PIN(B, 14)
#define BSP_PIN_NFC_MOSI       BSP_PIN(B, 15)
#define BSP_PIN_LCD_D13        BSP_PIN(D, 8)
#define BSP_PIN_LCD_D14        BSP_PIN(D, 9)
#define BSP_PIN_LCD_D15        BSP_PIN(D, 10)
#define BSP_PIN_LCD_CS         BSP_PIN(D, 11)
#define BSP_PIN_LCD_DC         BSP_PIN(D, 12)
#define BSP_PIN_LCD_RES        BSP_PIN(D, 13)
#define BSP_PIN_LCD_D0         BSP_PIN(D, 14)
#define BSP_PIN_LCD_D1         BSP_PIN(D, 15)
#define BSP_PIN_ERX_P          BSP_PIN(C, 6)
#define BSP_PIN_ERX_N          BSP_PIN(C, 7)
#define BSP_PIN_ETX_P          BSP_PIN(C, 8)
#define BSP_PIN_ETX_N          BSP_PIN(C, 9)
#define BSP_PIN_MOTOR_INA      BSP_PIN(A, 8)
#define BSP_PIN_LINK_TX        BSP_PIN(A, 9)
#define BSP_PIN_LINK_RX        BSP_PIN(A, 10)
#define BSP_PIN_MOTOR_INB      BSP_PIN(A, 11)
#define BSP_PIN_LINK_SWDIO     BSP_PIN(A, 13)
#define BSP_PIN_LINK_SWCLK     BSP_PIN(A, 14)
#define BSP_PIN_DIST_TRIG      BSP_PIN(C, 10)
#define BSP_PIN_DIST_ECHO      BSP_PIN(C, 11)
#define BSP_PIN_LCD_D2         BSP_PIN(D, 0)
#define BSP_PIN_LCD_D3         BSP_PIN(D, 1)
#define BSP_PIN_LCD_CINT       BSP_PIN(D, 2)
#define BSP_PIN_LCD_CRES       BSP_PIN(D, 3)
#define BSP_PIN_LCD_RD         BSP_PIN(D, 4)
#define BSP_PIN_LCD_WR         BSP_PIN(D, 5)
#define BSP_PIN_LCD_CSDA       BSP_PIN(D, 6)
#define BSP_PIN_LCD_CSCL       BSP_PIN(D, 7)
#define BSP_PIN_RGB_RED        BSP_PIN(B, 3)
#define BSP_PIN_RGB_GREEN      BSP_PIN(B, 4)
#define BSP_PIN_RGB_BLUE       BSP_PIN(B, 5)
#define BSP_PIN_HUB_UD_N       BSP_PIN(B, 6)
#define BSP_PIN_HUB_UD_P       BSP_PIN(B, 7)
#define BSP_PIN_I2C_SCL        BSP_PIN(B, 8)
#define BSP_PIN_I2C_SDA        BSP_PIN(B, 9)
#define BSP_PIN_LED_SCL        BSP_PIN(E, 0)
#define BSP_PIN_LED_SDA        BSP_PIN(E, 1)

#define BSP_ADC_NTC_CHANNEL ADC_Channel_8   /* PB0 */
#define BSP_ADC_MIC_CHANNEL ADC_Channel_13  /* PC3 */
#endif
