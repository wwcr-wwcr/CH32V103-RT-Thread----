#ifndef BSP_FSMC_H
#define BSP_FSMC_H
#include "bsp_types.h"

/* 启用 Bank1 异步寄存器总线。width=8/16，address_setup=0..15，data_setup=1..255，
 * 两项为 FSMC 时序寄存器值。NE1=PD7，地址选择使用 A17=PD12。现有屏幕占用 Bank1。 */
bsp_status_t bsp_fsmc_init(uint8_t width, uint8_t address_setup, uint8_t data_setup);
/* 写指定寄存器窗口。select=0/1 指 A17 电平，value 为 8 位或 16 位总线数据。 */
bsp_status_t bsp_fsmc_write(uint8_t select, uint16_t value);
/* 读指定寄存器窗口，结果写入 value。外部器件应满足已配置的读时序。 */
bsp_status_t bsp_fsmc_read(uint8_t select, uint16_t *value);
/* 关闭 Bank1 并释放本模块使用的控制、地址和数据引脚。 */
bsp_status_t bsp_fsmc_de_init(void);

typedef struct {
    uint8_t setup, wait, hold, high_z; /* FSMC 时序寄存器的 HCLK 周期字段。 */
} bsp_fsmc_nand_timing_t;
typedef enum {
    BSP_NAND_COMMAND, BSP_NAND_ADDRESS, BSP_NAND_WRITE, BSP_NAND_READ, BSP_NAND_WAIT_READY
} bsp_fsmc_nand_phase_t;
typedef struct {
    bsp_fsmc_nand_phase_t phase;
    union { const uint8_t *tx; uint8_t *rx; } buffer;
    size_t count; /* 读写的字节数，或 WAIT_READY 的非零轮询上限。 */
} bsp_fsmc_nand_step_t;

/* 初始化 Bank2 的 8 位异步 NAND 总线。timing 为空时采用 EVT 的 2/3/2/1 时序。
 * CLE=PD11、ALE=PD12、NCE=PD7、R/B=PD6，D0..7 与 Bank1 共用。 */
bsp_status_t bsp_fsmc_nand_init(const bsp_fsmc_nand_timing_t *timing);
/* 依序完成一组命令、地址、读写或 R/B 等待，整个序列持有总线锁。steps 指向 n 项。
 * 按 NAND 型号编排指令、地址周期及就绪等待。出错时已发出的周期不会回滚。 */
bsp_status_t bsp_fsmc_nand_exec(const bsp_fsmc_nand_step_t *steps, size_t n);
/* 停用 Bank2，释放 8 位总线和 R/B 引脚。 */
bsp_status_t bsp_fsmc_nand_de_init(void);
#endif
