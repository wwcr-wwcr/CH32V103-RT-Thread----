/*******************************************************************************
 * Size: 20 px
 * Bpp: 1
 * Opts: --font C:/Windows/Fonts/simhei.ttf --symbols 温度光照度 --size 20 --bpp 1 --format lvgl -o test.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef TEST
#define TEST 1
#endif

#if TEST

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+5149 "光" */
    0x0, 0xc0, 0x0, 0x30, 0x0, 0xc, 0x20, 0xc3,
    0xc, 0x18, 0xc6, 0x7, 0x31, 0x80, 0xcc, 0x40,
    0x3, 0x0, 0xff, 0xff, 0xc0, 0xcc, 0x0, 0x33,
    0x0, 0xc, 0xc0, 0x6, 0x30, 0x1, 0x8c, 0x0,
    0xc3, 0xc, 0xe0, 0xc3, 0xf0, 0x3f, 0x80, 0x0,
    0x0,

    /* U+5EA6 "度" */
    0x0, 0xc0, 0x0, 0x18, 0x3, 0xff, 0xf8, 0x80,
    0x0, 0x23, 0x4, 0x8, 0xc1, 0x2, 0xff, 0xf8,
    0x8c, 0x10, 0x23, 0x4, 0x8, 0xff, 0x2, 0x0,
    0x0, 0xbf, 0xf8, 0x23, 0x4, 0x18, 0x63, 0x4,
    0xd, 0x81, 0x1, 0xe0, 0xcf, 0xdf, 0xd7, 0x0,
    0x60,

    /* U+6E29 "温" */
    0x63, 0xfe, 0xe, 0xc0, 0x81, 0x30, 0x20, 0xf,
    0xf8, 0xc3, 0x2, 0x18, 0xc0, 0x82, 0x3f, 0xe0,
    0x0, 0x0, 0x0, 0x0, 0xd, 0xff, 0xc2, 0x64,
    0x90, 0x99, 0x24, 0x66, 0x49, 0x11, 0x92, 0x44,
    0x64, 0x93, 0x19, 0x24, 0xdf, 0xff, 0xc0,

    /* U+7167 "照" */
    0x7e, 0xff, 0x98, 0x86, 0x66, 0x21, 0x19, 0x88,
    0xc6, 0x62, 0x61, 0x9f, 0xb1, 0xe6, 0x28, 0x21,
    0x89, 0xfe, 0x62, 0x61, 0x98, 0x98, 0x66, 0x26,
    0x19, 0xf9, 0xfe, 0x0, 0x0, 0xc, 0x84, 0x46,
    0x21, 0x19, 0x8c, 0x62, 0xc1, 0x18, 0xd0, 0x0,
    0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 320, .box_w = 18, .box_h = 18, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 41, .adv_w = 320, .box_w = 18, .box_h = 18, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 82, .adv_w = 320, .box_w = 18, .box_h = 17, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 121, .adv_w = 320, .box_w = 18, .box_h = 18, .ofs_x = 1, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0xd5d, 0x1ce0, 0x201e
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 20809, .range_length = 8223, .glyph_id_start = 1,
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
const lv_font_t test = {
#else
lv_font_t test = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 19,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if TEST*/

