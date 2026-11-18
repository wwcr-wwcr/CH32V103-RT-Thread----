/********************************** (C) COPYRIGHT *******************************
*     main ──> kernel ──> event.h / lvgl / rt-thread
*       │ └──> app   ──> event.h / key.h / rt-thread
*       │       └─────> (事件由 kernel 调进 app，app 不认识 kernel)
*       └──> key / bsp / lv_port（HAL 层，各管各的）
*********************************************************************************/

#include "rtthread.h"
#include "lcd.h"
#include "key.h"
#include "i2c.h"
#include "kernel.h"
#include "app.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"

/* 首页应用，定义在 task/menu.c 里 */
extern const app_task_t task_menu;

/**
 * @brief  按键线程的回调：转成一个 type=0 的事件（0 = 按键）
 */
static void on_key(key_id_t key)
{
    kernel_post(0, (uint8_t)key, 0);
}

int main(void)
{
    SystemCoreClockUpdate();

    lcd_init();
    i2c_init(I2C_DEFAULT_SPEED);

    /* 2. 渲染通道：显示 + 触摸（GT911） */
    lv_port_disp_init();
    lv_port_indev_init();
    
    key_init();                         //注册按键
    key_event_register(on_key);
    key_start();

    app_init(&task_menu);       //开始页

    kernel_init(app_handle, app_render);
    kernel_run();

    return 0;
}
