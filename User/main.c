/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Description        : 应用入口 —— 全工程唯一把各模块串起来的地方
*
* 装配顺序（不能换）：
*     1. 板级外设      lcd / i2c
*     2. 渲染通道      lv_port_disp_init / lv_port_indev_init
*     3. 输入          key 线程（只往事件队列投键）
*     4. 页面路由      router_init(首页)
*     5. 跑起来        kernel_run(事件入口, 渲染入口)，不返回
*
* 模块依赖（箭头 = 只准往下依赖）：
*     main ──> kernel ──> event.h / lvgl / rt-thread
*       │ └──> router  ──> event.h / key.h / rt-thread
*       └──> key / bsp / lv_port（HAL 层，各管各的）
*
* 每个任务的逻辑（循环、节奏、按键处理）都在 task/xxx.c 里，main 不碰。
*********************************************************************************/

#include "rtthread.h"
#include "lcd.h"
#include "key.h"
#include "i2c.h"
#include "kernel.h"
#include "router.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"

/* 首页任务，定义在 task/menu.c 里 */
extern const task_t task_menu;

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
