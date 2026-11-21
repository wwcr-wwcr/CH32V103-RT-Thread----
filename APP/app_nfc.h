#ifndef APP_NFC_H
#define APP_NFC_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define APP_NFC_BLOCK_BYTES 16U

/* 初始化套件 SPI NFC 模块并开启射频，在线程中调用。 */
app_status_t app_nfc_init(void);
/* 设置或读取模块射频开关。输入/输出 0 关、1 开，交换一次约 10 ms。 */
app_status_t app_nfc_set_enabled(uint8_t enabled);
app_status_t app_nfc_get_enabled(uint8_t *enabled);
/* 单次寻卡约 100 ms。找到返回 OK，未找到返回 NOT_FOUND。 */
app_status_t app_nfc_scan(void);
/* 在射频已开启时寻卡并读一个 M1 1K 卡块。block 为绝对块号 0~63；
 * data 指向 16 字节缓冲区，操作成功后更新。 */
app_status_t app_nfc_read_block(uint8_t block, uint8_t data[APP_NFC_BLOCK_BYTES]);
/* 寻卡并写一个 16 字节数据块。block 为 1~62，排除每扇区尾块
 * （3、7、11……）与制造商块 0。只在需要改写卡片时调用。 */
app_status_t app_nfc_write_data_block(uint8_t block, const uint8_t data[APP_NFC_BLOCK_BYTES]);
/* 尝试关闭射频并释放 SPI2、PB12~15。拔掉模块导致的应答错误会返回给调用方。 */
app_status_t app_nfc_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
