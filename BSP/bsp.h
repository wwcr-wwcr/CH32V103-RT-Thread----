#ifndef BSP_H
#define BSP_H

/* CH32V307VCT6 板级驱动公共入口。上层 APP 也可只包含所需外设的头文件。 */
#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_board.h"
#include "bsp_gpio.h"
#include "bsp_exti.h"
#include "bsp_adc.h"
#include "bsp_dac.h"
#include "bsp_opa.h"
#include "bsp_tim.h"
#include "bsp_dma.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_can.h"
#include "bsp_rtc.h"
#include "bsp_bkp.h"
#include "bsp_pwr.h"
#include "bsp_rcc.h"
#include "bsp_iwdg.h"
#include "bsp_wwdg.h"
#include "bsp_flash.h"
#include "bsp_crc.h"
#include "bsp_rng.h"
#include "bsp_dbgmcu.h"
#include "bsp_fsmc.h"
#include "bsp_sdio.h"
#include "bsp_dvp.h"
#include "bsp_usb.h"
#include "bsp_eth.h"

#ifdef __cplusplus
}
#endif
#endif
