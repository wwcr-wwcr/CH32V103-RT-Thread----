#include "kernel.h"
#include "app.h"
#include "ui/menu_ui.h"
#include "rtthread.h"

extern const app_task_t task_lux;
static const app_task_t *const g_app_list[] =
{
    &task_lux,
};

/* 一共几个任务：sizeof 算出来，加行不用改别的地方 */
static const uint8_t g_app_num = (uint8_t)(sizeof(g_app_list) / sizeof(g_app_list[0]));

static uint8_t s_cursor = 0;

static void menu_setup(void)
{
   
}

static void menu_on_set(uint8_t id, int32_t value)
{
    switch(id)
    {
        case KEY_ID_1:                          /* K1 上一项，到头回绕到最后一个 */
            s_cursor = (uint8_t)((s_cursor + g_app_num - 1) % g_app_num);
            break;

        case KEY_ID_2:                          /* K2 下一项 */
            s_cursor = (uint8_t)((s_cursor + 1) % g_app_num);
            break;

        case KEY_ID_3:                          /* K3 进当前任务：建线程、切页都交给 kernel */
            app_request(g_app_list[s_cursor]);
            break;

        default:
            return;
    }
}

static void menu_on_key(key_id_t key)
{
    switch(key)
    {
        case KEY_ID_1:                          /* K1 上一项，到头回绕到最后一个 */
            s_cursor = (uint8_t)((s_cursor + g_app_num - 1) % g_app_num);
            break;

        case KEY_ID_2:                          /* K2 下一项 */
            s_cursor = (uint8_t)((s_cursor + 1) % g_app_num);
            break;

        case KEY_ID_3:                          /* K3 进当前任务：建线程、切页都交给 kernel */
            app_request(g_app_list[s_cursor]);
            break;

        default:
            return;
    }
}


static void menu_render(void)
{
    menu_ui_render(s_cursor, g_app_num, g_app_list[s_cursor]->name);
}

const app_task_t task_menu =
{
    .name       = "MENU",           /* 线程名（<= 7 字符） */
    .stack_size = 1024,             /* 只做光标加减和切页请求，1024 够 */
    .priority   = 12,               /* key=8、ui=9，任务统一 12 */

    .setup      = menu_setup,       /* 逻辑线程：注册触摸回调 */
    .update     = RT_NULL,          /* 静态页面，不需要周期处理 */
    .on_key     = menu_on_key,      /* 逻辑线程：K1~K3 */
    .on_set     = menu_on_set,          /* 触摸是合成为按键的，不走控件事件 */
    .render     = menu_render,      /* UI 线程：只调一个 menu_ui_render */
    .teardown   = RT_NULL,          /* 没占硬件资源，退页不用收尾 */
    .ui_destroy = menu_ui_destroy,  /* UI 线程：把菜单页还给 LVGL 堆 */
};
