/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --font C:\Users\FIREBAT\Desktop\CH32V103-RT-Thread----\tools\fusion-pixel-12px-monospaced-zh_hans.ttf --symbols 南京信息职业技术学院张瑞敏 --size 12 --bpp 1 --format lvgl --lv-font-name lv_font_cn12 -o C:\Users\FIREBAT\Desktop\CH32V103-RT-Thread----\LVGL\GUI_APP\lv_font_cn12.c --lv-fallback lv_font_montserrat_14
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_CN12
#define LV_FONT_CN12 1
#endif

#if LV_FONT_CN12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+4E1A "业" */
    0x11, 0x2, 0x20, 0x44, 0x48, 0x99, 0x12, 0xa2,
    0x94, 0x50, 0x88, 0x11, 0x2, 0x23, 0xff, 0x80,

    /* U+4EAC "京" */
    0x4, 0x1f, 0xfc, 0x0, 0x3f, 0xe4, 0x4, 0x80,
    0x9f, 0xf0, 0x20, 0x24, 0x98, 0x8c, 0x70, 0x0,

    /* U+4FE1 "信" */
    0x22, 0x4, 0x21, 0x7f, 0xa0, 0xc, 0xfc, 0x80,
    0x13, 0xf2, 0x0, 0x4f, 0xc9, 0x9, 0x3f, 0x0,

    /* U+5357 "南" */
    0x4, 0x1f, 0xfc, 0x10, 0x7f, 0xf9, 0x13, 0x14,
    0x6f, 0xec, 0x21, 0xbf, 0xb0, 0x86, 0x13, 0x80,

    /* U+5B66 "学" */
    0x48, 0x44, 0x93, 0xff, 0xc0, 0x1b, 0xfa, 0x2,
    0x1, 0x87, 0xff, 0x4, 0x0, 0x80, 0x70, 0x0,

    /* U+5F20 "张" */
    0xe4, 0x44, 0x88, 0x92, 0x72, 0x88, 0x41, 0x3f,
    0xf9, 0x41, 0x24, 0x24, 0x84, 0xab, 0x18, 0x80,

    /* U+606F "息" */
    0x8, 0x7, 0xf0, 0x82, 0x1f, 0xc2, 0x8, 0x7f,
    0x8, 0x21, 0xfc, 0x54, 0x4a, 0x56, 0x3e, 0x80,

    /* U+6280 "技" */
    0x41, 0x8, 0x23, 0xbf, 0xa0, 0x84, 0x10, 0xdf,
    0xb2, 0x12, 0x42, 0x44, 0x88, 0x63, 0x73, 0x80,

    /* U+654F "敏" */
    0x41, 0xf, 0xa2, 0x7, 0xbf, 0x25, 0x54, 0x9a,
    0xbf, 0xd2, 0xaa, 0x4c, 0x8f, 0xa8, 0x18, 0x80,

    /* U+672F "术" */
    0x4, 0x80, 0x88, 0x10, 0x7f, 0xf1, 0x50, 0x2a,
    0x9, 0x21, 0x24, 0x44, 0x50, 0x84, 0x10, 0x0,

    /* U+745E "瑞" */
    0x9, 0x3d, 0x25, 0x3f, 0xa0, 0x4, 0xff, 0xc2,
    0x13, 0xfa, 0x55, 0x6a, 0xb9, 0x54, 0x21, 0x80,

    /* U+804C "职" */
    0xfb, 0xea, 0x45, 0xc8, 0xa9, 0x15, 0x22, 0xe7,
    0xd4, 0x2, 0x80, 0x7a, 0x5a, 0x44, 0x50, 0x80,

    /* U+9662 "院" */
    0xe1, 0x15, 0xfe, 0xa0, 0xd3, 0xec, 0x1, 0x40,
    0x2b, 0xfd, 0x14, 0xe2, 0x90, 0x96, 0x63, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 16, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 32, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 48, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 64, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 80, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 96, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 112, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 128, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 144, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 160, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 176, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 192, .adv_w = 192, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x92, 0x1c7, 0x53d, 0xd4c, 0x1106, 0x1255, 0x1466,
    0x1735, 0x1915, 0x2644, 0x3232, 0x4848
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 19994, .range_length = 18505, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 13, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};

extern const lv_font_t lv_font_montserrat_14;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_cn12 = {
#else
lv_font_t lv_font_cn12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 11,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_montserrat_14,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_CN12*/

