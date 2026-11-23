/******************************************************************************
 * 文件名   : led_fx.h
 * 说明     : RGB 灯效 —— 在 HAL/led 之上管"颜色 / 模式"
 *
 * 设计：**不用任务去驱动**，动态模式内部用 rt_timer 自己跑（10ms 一步），
 *       所以任务里只要调一次 xxx() 就行，不需要 tick，也不会阻塞。
 *
 * 四种用法各一个函数：
 *     led_fx_steady()        常亮当前颜色
 *     led_fx_breath(2000)    2 秒一次呼吸
 *     led_fx_blink(400)      0.4 秒一个周期的闪烁
 *     led_fx_rainbow(6000)   6 秒转一圈彩虹
 *
 * 亮度：直接写在颜色值里（0xRRGGBB），想暗一点就写小一点，
 *       例如 0x660000 是 40% 的红。
 ******************************************************************************/

#ifndef __LED_FX_H
#define __LED_FX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LED_FX_PALETTE_NUM   10U

/**************************************************************************************************
 *  ★ 预设颜色队列：改颜色、调顺序都直接改这里
 *    （用 LED_FX_PALETTE_DEFINE 包着，只有 led_fx.c 会实例化，不会重复定义）
 **************************************************************************************************/
#ifdef LED_FX_PALETTE_DEFINE
static const uint32_t led_fx_palette[LED_FX_PALETTE_NUM] =
{
    0xff0000,   /* 红 */
    0x00ff00,   /* 绿 */
    0x0000ff,   /* 蓝 */
    0xffff00,   /* 黄 */
    0x00ffff,   /* 青 */
    0xff00ff,   /* 品红 */
    0xffffff,   /* 白 */
    0xffb060,   /* 暖白 */
    0xff8000,   /* 琥珀 */
    0x8000ff,   /* 紫 */
};
#endif

/**
 * @brief  初始化：把当前颜色推给 LED（进页面调一次，顺便预热 PWM）
 */
void led_fx_init(void);

/**
 * @brief  立即换成指定颜色（当前模式不变）
 * @param  rgb  0xRRGGBB
 */
void led_fx_set_color(uint32_t rgb);

/**
 * @brief  按序号取预设色（0..LED_FX_PALETTE_NUM-1，超范围回绕）
 */
void led_fx_set_color_index(uint8_t idx);

/**
 * @brief  切到预设队列里的下一个颜色（循环）
 */
void led_fx_next_color(void);

/**
 * @brief  从当前颜色渐变到目标颜色
 * @param  rgb  目标色
 * @param  ms   过渡时长（0 = 立即切换）
 */
void led_fx_fade_to(uint32_t rgb, uint16_t ms);

/**
 * @brief  常亮（停止任何动态模式）
 */
void led_fx_steady(void);

/**
 * @brief  呼吸：亮度 0 → 满 → 0 循环
 * @param  period_ms  一个来回的时长（小于 200 按 200 处理）
 */
void led_fx_breath(uint16_t period_ms);

/**
 * @brief  闪烁：前半周期亮、后半周期灭
 * @param  period_ms  一个亮灭周期的时长（小于 100 按 100 处理）
 */
void led_fx_blink(uint16_t period_ms);

/**
 * @brief  彩虹：色相循环，亮度保持满
 * @param  period_ms  转一圈的时长（小于 500 按 500 处理）
 */
void led_fx_rainbow(uint16_t period_ms);

/**
 * @brief  熄灭（颜色和模式保留，下次 steady/breath 会恢复）
 */
void led_fx_off(void);

#ifdef __cplusplus
}
#endif

#endif
