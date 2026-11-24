#include "rtthread.h"
#include "lcd.h"
#include "key.h"
#include "i2c.h"
#include "kernel.h"
#include "router.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
#include "tasks.h"

/**
 * @brief  按键线程的回调：转成一个 type=0 的按键事件
 */
static void on_key(key_id_t key)
{
    kernel_post(0, (uint8_t)key, 0);
}

int main(void)
{
    SystemCoreClockUpdate();

    /* 1. 板级外设 */
    lcd_init();
    i2c_init(I2C_DEFAULT_SPEED);

    /* 2. 渲染通道：显示 + 触摸（GT911） */
    lv_port_disp_init();
    lv_port_indev_init();

    /* 3. 输入：按键线程只投事件 */
    key_init();
    key_event_register(on_key);
    key_start();

    /* 4. 页面路由：记下首页（首页线程由第一拍 router_render 打开） */
    router_init(&task_menu);

    /* 5. 内核线程进主循环，不返回 */
    kernel_run(router_dispatch, router_render);

    return 0;
}
