#include "bsp_fsmc.h"
#include "bsp_ahb_internal.h"
#include "bsp_board.h"

static const bsp_pin_t pins[] = {
    {GPIOD, GPIO_Pin_4}, {GPIOD, GPIO_Pin_5}, {GPIOD, GPIO_Pin_7}, {GPIOD, GPIO_Pin_12},
    {GPIOD, GPIO_Pin_14}, {GPIOD, GPIO_Pin_15}, {GPIOD, GPIO_Pin_0}, {GPIOD, GPIO_Pin_1},
    {GPIOE, GPIO_Pin_7}, {GPIOE, GPIO_Pin_8}, {GPIOE, GPIO_Pin_9}, {GPIOE, GPIO_Pin_10},
    {GPIOE, GPIO_Pin_11}, {GPIOE, GPIO_Pin_12}, {GPIOE, GPIO_Pin_13}, {GPIOE, GPIO_Pin_14},
    {GPIOE, GPIO_Pin_15}, {GPIOD, GPIO_Pin_8}, {GPIOD, GPIO_Pin_9}, {GPIOD, GPIO_Pin_10}
};
static uint8_t bus_width;
static uint8_t nand_active;
static volatile uint8_t lock;

bsp_status_t bsp_fsmc_init(uint8_t width, uint8_t address_setup, uint8_t data_setup)
{
    if ((width != 8 && width != 16) || address_setup > 15 || !data_setup) return BSP_ERR_ARG;
    if (BSP_BOARD_LCD_ACTIVE) return BSP_ERR_BUSY;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = bus_width || (RCC->AHBPCENR & RCC_AHBPeriph_FSMC) ? BSP_ERR_BUSY :
                          bsp_pins_claim(&bus_width, pins, width + 4);
    if (status == BSP_OK) {
        for (size_t i = 0; i < width + 4u; ++i) bsp_pin_config(pins[i], GPIO_Mode_AF_PP);
        FSMC_NORSRAMTimingInitTypeDef timing = {0};
        FSMC_NORSRAMInitTypeDef cfg = {0};
        timing.FSMC_AddressSetupTime = address_setup;
        timing.FSMC_DataSetupTime = data_setup;
        timing.FSMC_CLKDivision = 1;
        cfg.FSMC_Bank = FSMC_Bank1_NORSRAM1;
        cfg.FSMC_MemoryType = FSMC_MemoryType_SRAM;
        cfg.FSMC_MemoryDataWidth = width == 16 ? FSMC_MemoryDataWidth_16b : FSMC_MemoryDataWidth_8b;
        cfg.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
        cfg.FSMC_ReadWriteTimingStruct = &timing;
        cfg.FSMC_WriteTimingStruct = &timing;
        bsp_ahb_clock(RCC_AHBPeriph_FSMC, ENABLE);
        FSMC_NORSRAMCmd(cfg.FSMC_Bank, DISABLE);
        FSMC_NORSRAMInit(&cfg);
        FSMC_NORSRAMCmd(cfg.FSMC_Bank, ENABLE);
        bus_width = width;
    }
    bsp_unlock(&lock);
    return status;
}

static bsp_status_t access_register(uint8_t select, uint16_t *value, int write)
{
    if (select > 1 || !value) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (bus_width) {
        uintptr_t addr = 0x60000000u + ((uintptr_t)select << (bus_width == 16 ? 18 : 17));
        status = BSP_OK;
        if (bus_width == 16) {
            if (write) *(volatile uint16_t *)addr = *value;
            else *value = *(volatile uint16_t *)addr;
        } else if (write && *value > UINT8_MAX) status = BSP_ERR_ARG;
        else if (write) *(volatile uint8_t *)addr = (uint8_t)*value;
        else *value = *(volatile uint8_t *)addr;
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_fsmc_write(uint8_t select, uint16_t value)
{
    return access_register(select, &value, 1);
}

bsp_status_t bsp_fsmc_read(uint8_t select, uint16_t *value)
{
    return access_register(select, value, 0);
}

bsp_status_t bsp_fsmc_de_init(void)
{
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (bus_width) {
        FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, DISABLE);
        /* 官方反初始化把 Bank1 控制寄存器恢复为带 MBKEN 的复位值，之后需显式关闭。 */
        FSMC_NORSRAMDeInit(FSMC_Bank1_NORSRAM1);
        FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, DISABLE);
        if (!(FSMC_Bank2->PCR2 & (1u << 2))) bsp_ahb_clock(RCC_AHBPeriph_FSMC, DISABLE);
        bsp_ahb_release_pins(&bus_width, pins, bus_width + 4);
        bus_width = 0;
        status = BSP_OK;
    }
    bsp_unlock(&lock);
    return status;
}

static const bsp_pin_t nand_pins[] = {
    {GPIOD, GPIO_Pin_4}, {GPIOD, GPIO_Pin_5}, {GPIOD, GPIO_Pin_7},
    {GPIOD, GPIO_Pin_11}, {GPIOD, GPIO_Pin_12},
    {GPIOD, GPIO_Pin_14}, {GPIOD, GPIO_Pin_15}, {GPIOD, GPIO_Pin_0}, {GPIOD, GPIO_Pin_1},
    {GPIOE, GPIO_Pin_7}, {GPIOE, GPIO_Pin_8}, {GPIOE, GPIO_Pin_9}, {GPIOE, GPIO_Pin_10},
    {GPIOD, GPIO_Pin_6}
};
#define NAND_PIN_COUNT (sizeof(nand_pins) / sizeof(nand_pins[0]))
#define NAND_DATA_ADDR 0x70000000u
#define NAND_CMD_ADDR  (NAND_DATA_ADDR | (1u << 16))
#define NAND_ALE_ADDR  (NAND_DATA_ADDR | (1u << 17))

bsp_status_t bsp_fsmc_nand_init(const bsp_fsmc_nand_timing_t *timing)
{
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = nand_active || bus_width || (RCC->AHBPCENR & RCC_AHBPeriph_FSMC) ?
                          BSP_ERR_BUSY : bsp_pins_claim(&nand_active, nand_pins, NAND_PIN_COUNT);
    if (status == BSP_OK) {
        FSMC_NAND_PCCARDTimingInitTypeDef bus = {2, 3, 2, 1};
        if (timing) {
            bus.FSMC_SetupTime = timing->setup;
            bus.FSMC_WaitSetupTime = timing->wait;
            bus.FSMC_HoldSetupTime = timing->hold;
            bus.FSMC_HiZSetupTime = timing->high_z;
        }
        for (size_t i = 0; i + 1 < NAND_PIN_COUNT; ++i) bsp_pin_config(nand_pins[i], GPIO_Mode_AF_PP);
        bsp_pin_config(nand_pins[NAND_PIN_COUNT - 1], GPIO_Mode_IPU);
        bsp_ahb_clock(RCC_AHBPeriph_FSMC, ENABLE);
        FSMC_NANDInitTypeDef cfg = {0};
        cfg.FSMC_Bank = FSMC_Bank2_NAND;
        cfg.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_8b;
        cfg.FSMC_TARSetupTime = 1;
        cfg.FSMC_CommonSpaceTimingStruct = &bus;
        cfg.FSMC_AttributeSpaceTimingStruct = &bus;
        FSMC_NANDInit(&cfg);
        FSMC_NANDCmd(cfg.FSMC_Bank, ENABLE);
        nand_active = 1;
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_fsmc_nand_exec(const bsp_fsmc_nand_step_t *steps, size_t n)
{
    if (!steps || !n) return BSP_ERR_ARG;
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned)steps[i].phase > BSP_NAND_WAIT_READY || !steps[i].count) return BSP_ERR_ARG;
        if (steps[i].phase == BSP_NAND_READ && !steps[i].buffer.rx) return BSP_ERR_ARG;
        if (steps[i].phase < BSP_NAND_READ && !steps[i].buffer.tx) return BSP_ERR_ARG;
    }
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = nand_active ? BSP_OK : BSP_ERR_STATE;
    for (size_t i = 0; i < n && status == BSP_OK; ++i) {
        const bsp_fsmc_nand_step_t *s = &steps[i];
        if (s->phase == BSP_NAND_WAIT_READY) {
            size_t polls = s->count;
            status = BSP_ERR_TIMEOUT;
            while (polls--) {
                if (GPIOD->INDR & GPIO_Pin_6) { status = BSP_OK; break; }
            }
        } else if (s->phase == BSP_NAND_READ) {
            for (size_t j = 0; j < s->count; ++j) s->buffer.rx[j] = *(volatile uint8_t *)NAND_DATA_ADDR;
        } else {
            uintptr_t addr = s->phase == BSP_NAND_COMMAND ? NAND_CMD_ADDR :
                             s->phase == BSP_NAND_ADDRESS ? NAND_ALE_ADDR : NAND_DATA_ADDR;
            for (size_t j = 0; j < s->count; ++j) *(volatile uint8_t *)addr = s->buffer.tx[j];
        }
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_fsmc_nand_de_init(void)
{
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (nand_active) {
        FSMC_NANDCmd(FSMC_Bank2_NAND, DISABLE);
        FSMC_NANDDeInit(FSMC_Bank2_NAND);
        bsp_ahb_clock(RCC_AHBPeriph_FSMC, DISABLE);
        bsp_ahb_release_pins(&nand_active, nand_pins, NAND_PIN_COUNT);
        nand_active = 0;
        status = BSP_OK;
    }
    bsp_unlock(&lock);
    return status;
}
