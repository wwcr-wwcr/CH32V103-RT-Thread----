/*******************************************************************************
 * Size: 20 px
 * Bpp: 1
 * Opts: --font C:\Users\FIREBAT\Desktop\CH32V103-RT-Thread----\tools\fusion-pixel-12px-monospaced-zh_hans.ttf --symbols 我的世界 --size 20 --bpp 1 --format lvgl --lv-font-name lv_font_my -o C:\Users\FIREBAT\Desktop\CH32V103-RT-Thread----\LVGL\GUI_APP\lv_font_my.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_MY
#define LV_FONT_MY 1
#endif

#if LV_FONT_MY

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+4E16 "世" */
    0x18, 0xc6, 0x6, 0x31, 0x81, 0x8c, 0x60, 0x63,
    0x18, 0x18, 0xc6, 0x3f, 0xff, 0xf1, 0x8c, 0x60,
    0x63, 0x18, 0x18, 0xc6, 0x6, 0x31, 0x81, 0x8c,
    0x60, 0x63, 0x18, 0x18, 0xfe, 0x6, 0x0, 0x1,
    0x80, 0x0, 0x60, 0x0, 0x18, 0x0, 0x7, 0xff,
    0xf0,

    /* U+6211 "我" */
    0x7, 0x19, 0x0, 0x6, 0xf, 0x81, 0x8c, 0x60,
    0x60, 0x18, 0x18, 0x3f, 0xff, 0xf1, 0x81, 0x80,
    0x60, 0x60, 0x18, 0x19, 0x7, 0xc6, 0x41, 0xf1,
    0x90, 0x60, 0x0, 0xf8, 0x6, 0x6, 0x1, 0xb1,
    0x80, 0x6c, 0x60, 0x67, 0x38, 0x39, 0xce, 0x8,
    0x30,

    /* U+754C "界" */
    0x7f, 0xff, 0x98, 0x30, 0x66, 0xc, 0x19, 0x83,
    0x6, 0x7f, 0xff, 0x98, 0x30, 0x66, 0xc, 0x19,
    0xff, 0xfe, 0x6, 0x18, 0x1, 0x86, 0x1, 0x80,
    0x60, 0x0, 0x0, 0xe6, 0x19, 0xf9, 0x86, 0x70,
    0x61, 0x80, 0x60, 0x60, 0x0, 0x18, 0x38, 0x6,
    0x0,

    /* U+7684 "的" */
    0x18, 0x30, 0x0, 0x6, 0x0, 0x80, 0xc0, 0x7f,
    0xdf, 0xff, 0xfb, 0xff, 0x83, 0xc0, 0xf0, 0x60,
    0x1e, 0xc, 0x3, 0xc1, 0x8c, 0x78, 0x31, 0x8f,
    0xfe, 0xd, 0xe0, 0xc1, 0xbc, 0x18, 0x37, 0x83,
    0x0, 0xf0, 0x60, 0x1e, 0xc, 0x3, 0xc1, 0x80,
    0x7f, 0xf0, 0x70
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 320, .box_w = 18, .box_h = 18, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 41, .adv_w = 320, .box_w = 18, .box_h = 18, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 82, .adv_w = 320, .box_w = 18, .box_h = 18, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 123, .adv_w = 320, .box_w = 19, .box_h = 18, .ofs_x = 0, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x13fb, 0x2736, 0x286e
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 19990, .range_length = 10351, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 4, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
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



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_my = {
#else
lv_font_t lv_font_my = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 18,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -3,
    .underline_thickness = 2,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_MY*/

