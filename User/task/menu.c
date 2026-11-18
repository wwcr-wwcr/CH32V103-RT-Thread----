/******************************************************************************
 * 文件名   : menu.c
 * 说明     : MENU —— 选择页（首页）
 *
 * 页面内容（UI 在 ui/menu_ui.c）：
 *     一张卡片显示 "TASK n" 和任务名，左右两个箭头切换；
 *     点卡片或按 K3 进入当前任务，退回首页时停在原来那一项。
 *
 * 按键：
 *     K1 上一项 / K2 下一项 / K3 进入当前任务
 *     K4 是全局退出键，kernel 自己吃掉了，本文件收不到
 *
 * 线程模型（kernel.c 那套）：
 *     setup()      跑在本任务线程：注册触摸回调
 *     on_key()     跑在本任务线程：只改光标，切页请求交给 kernel
 *     render()     跑在 ui 线程：只把光标和任务名丢给 UI，不做业务
 *     ui_destroy() 跑在 ui 线程：把菜单页还给 LVGL 堆
 *
 * 它和 task_example 是同一个类型、同一套回调，唯一区别是被 main 里那句
 * app_init(&task_menu) 当成了上电第一屏。
 ******************************************************************************/

#include "kernel.h"
#include "app.h"
#include "ui/menu_ui.h"
#include "rtthread.h"

/**************************************************************************************************
 *                                        任务表
 **************************************************************************************************/
/* 任务实例由各自的 task_x.c 定义，这里用到哪个就 extern 哪个 */
extern const app_task_t task_lux;

/* 菜单里能进的任务：新增界面 = 写一个 task_xxx.c，这里 extern 一行 + 表里加一行 */
static const app_task_t *const g_app_list[] =
{
    &task_lux,
};

/* 一共几个任务：sizeof 算出来，加行不用改别的地方 */
static const uint8_t g_app_num = (uint8_t)(sizeof(g_app_list) / sizeof(g_app_list[0]));

/**************************************************************************************************
 *                                        数据
 **************************************************************************************************/
/* 当前选中第几个任务，0 开始。用 static 是为了退回首页时还停在原来那一项 */
static uint8_t s_cursor = 0;

/**************************************************************************************************
 *                                        触摸
 **************************************************************************************************/
/**
 * @brief  触摸左箭头：合成一个 K1
 * @note   回调跑在 ui 线程里，所以只做"投一个键"这种快操作；
 *         合成出来的键和实体键走同一条分发路径，菜单逻辑不用区分来源
 */
static void tap_prev(void)
{
    kernel_post(0, (uint8_t)KEY_ID_1, 0);       /* 0 = 按键：左箭头 = K1 上一项 */
}

/**
 * @brief  触摸右箭头：合成一个 K2
 */
static void tap_next(void)
{
    kernel_post(0, (uint8_t)KEY_ID_2, 0);       /* 0 = 按键：右箭头 = K2 下一项 */
}

/**
 * @brief  点卡片：合成一个 K3
 */
static void tap_enter(void)
{
    kernel_post(0, (uint8_t)KEY_ID_3, 0);       /* 0 = 按键：点卡片 = K3 进入 */
}

/**************************************************************************************************
 *                                        任务回调
 **************************************************************************************************/
/**
 * @brief  进入页面：注册触摸按钮
 * @note   跑在本任务线程里，这里不能碰 lv_
 */
static void menu_setup(void)
{
    menu_ui_set_tap_cb(tap_prev, tap_next, tap_enter);
}

/**
 * @brief  按键处理
 * @param  key  K1~K3，K4 不会送到这里
 * @note   只改光标：界面由 render 画，切页由 kernel 做，这里不碰线程也不碰 lv_
 */
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

/**
 * @brief  每帧渲染（ui 线程，10ms 一次）
 * @note   光标没动时 menu_ui_render 内部直接返回，不会白重绘
 */
static void menu_render(void)
{
    menu_ui_render(s_cursor, g_app_num, g_app_list[s_cursor]->name);
}

/**************************************************************************************************
 *                                        任务接口
 **************************************************************************************************/
/**
 * @brief 任务描述表，填法照着 task_example：七个回调按需给，没用的写 RT_NULL
 */
const app_task_t task_menu =
{
    .name       = "MENU",           /* 线程名（<= 7 字符） */
    .stack_size = 1024,             /* 只做光标加减和切页请求，1024 够 */
    .priority   = 12,               /* key=8、ui=9，任务统一 12 */

    .setup      = menu_setup,       /* 逻辑线程：注册触摸回调 */
    .update     = RT_NULL,          /* 静态页面，不需要周期处理 */
    .on_key     = menu_on_key,      /* 逻辑线程：K1~K3 */
    .on_set     = RT_NULL,          /* 触摸是合成为按键的，不走控件事件 */
    .render     = menu_render,      /* UI 线程：只调一个 menu_ui_render */
    .teardown   = RT_NULL,          /* 没占硬件资源，退页不用收尾 */
    .ui_destroy = menu_ui_destroy,  /* UI 线程：把菜单页还给 LVGL 堆 */
};
