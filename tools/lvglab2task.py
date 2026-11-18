#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LVGLab 导出 -> task_ui 结构 一键转换器

把 LVGLab / SquareLine 生成的 ui_<Screen>.c/.h 变成这套架构里的：
    User/ui/<prefix>_ui.c/.h     建页 / load / 销毁（只有 3 个函数）
    User/task/task_<prefix>.c   任务骨架（open/close/post/线程/render 全部就绪）
    User/ui/gen/                生成代码原样复制过去（不要手改）

同时检查 lv_conf.h：生成代码用到的控件宏没开的话，提前报出来。

用法：
    GUI  双击 lvglab2task.exe（或 python lvglab2task.py）
    CLI  python lvglab2task.py --src ../export/ui --screen Screen1 --task CHART --project ..
"""

import os
import re
import sys
import glob
import shutil
import argparse

# ------------------------------------------------------------------ 常量
VERSION = "1.0"

# LVGL 控件创建函数 -> 需要打开的 lv_conf 宏
API_TO_MACRO = {
    "animimg": "LV_USE_ANIMIMG",   "arc": "LV_USE_ARC",         "bar": "LV_USE_BAR",
    "btn": "LV_USE_BTN",           "btnmatrix": "LV_USE_BTNMATRIX",
    "calendar": "LV_USE_CALENDAR", "canvas": "LV_USE_CANVAS",   "chart": "LV_USE_CHART",
    "checkbox": "LV_USE_CHECKBOX", "colorwheel": "LV_USE_COLORWHEEL",
    "dropdown": "LV_USE_DROPDOWN", "img": "LV_USE_IMG",         "imgbtn": "LV_USE_IMGBTN",
    "keyboard": "LV_USE_KEYBOARD", "led": "LV_USE_LED",         "line": "LV_USE_LINE",
    "list": "LV_USE_LIST",         "menu": "LV_USE_MENU",       "meter": "LV_USE_METER",
    "msgbox": "LV_USE_MSGBOX",     "roller": "LV_USE_ROLLER",   "slider": "LV_USE_SLIDER",
    "span": "LV_USE_SPAN",         "spinbox": "LV_USE_SPINBOX", "spinner": "LV_USE_SPINNER",
    "switch": "LV_USE_SWITCH",     "table": "LV_USE_TABLE",     "tabview": "LV_USE_TABVIEW",
    "textarea": "LV_USE_TEXTAREA", "tileview": "LV_USE_TILEVIEW", "win": "LV_USE_WIN",
}

# 这些不是 screen 文件
LIB_FILES = {"ui", "ui_helpers", "ui_events", "ui_events_weak", "ui_themes",
             "ui_theme_manager", "ui_comp", "ui_comp_hook"}


# ------------------------------------------------------------------ 小工具
def read_text(path):
    raw = open(path, "rb").read()
    for enc in ("utf-8", "gbk", "latin-1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1")


def write_gbk(path, text):
    """项目源码是 GBK + CRLF，生成物保持一致（MRS 里才不会乱码）"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    text = text.replace("\r\n", "\n").replace("\n", "\r\n")
    data = text.encode("gbk", "replace")
    open(path, "wb").write(data)
    return len(data)


def find_src_dir(src):
    """允许传 export/ui 或 export/ui/src"""
    if os.path.isdir(os.path.join(src, "src")):
        return os.path.join(src, "src")
    return src


def list_screens(src_dir):
    """扫描 src 下所有 ui_<X>.h，返回 [(screen, 控件个数), ...]"""
    out = []
    for h in sorted(glob.glob(os.path.join(src_dir, "ui_*.h"))):
        name = os.path.splitext(os.path.basename(h))[0]
        if name in LIB_FILES or name.startswith("ui_comp_component"):
            continue
        t = read_text(h)
        screen = name[3:]
        if re.search(r"void\s+ui_%s_screen_init\s*\(" % re.escape(screen), t):
            out.append((screen, len(parse_widgets(src_dir, screen))))
    return out


def parse_widgets(src_dir, screen):
    """ui_Screen1.h -> ['ui_Chart1', ...]"""
    h = os.path.join(src_dir, "ui_%s.h" % screen)
    if not os.path.isfile(h):
        return []
    return re.findall(r"extern\s+lv_obj_t\s*\*\s*(ui_\w+)\s*;", read_text(h))


def scan_needed_macros(src_dir, screen):
    """扫 ui_<Screen>.c 里用到的 lv_xxx_create()，映射成需要的宏"""
    c = os.path.join(src_dir, "ui_%s.c" % screen)
    text = read_text(c) if os.path.isfile(c) else ""
    need = {}
    for fn in set(re.findall(r"\blv_(\w+)_create\s*\(", text)):
        macro = API_TO_MACRO.get(fn)
        if macro:
            need[macro] = "lv_%s_create()" % fn
    return need


def find_lv_conf(project):
    hits = glob.glob(os.path.join(project, "**", "lv_conf.h"), recursive=True)
    for h in hits:
        if "LV_COLOR_DEPTH" in read_text(h):
            return h
    return None


def conf_values(conf_path):
    if not conf_path or not os.path.isfile(conf_path):
        return {}
    t = read_text(conf_path)
    return {m.group(1): int(m.group(2))
            for m in re.finditer(r"#define\s+(LV_USE_\w+)\s+(\d+)", t)}


def fix_conf(conf_path, macros, log):
    """把需要的宏从 0 改成 1（GBK 安全，保留 CRLF）"""
    raw = open(conf_path, "rb").read()
    enc = "gbk"
    try:
        text = raw.decode("utf-8")
        enc = "utf-8"
    except UnicodeDecodeError:
        text = raw.decode("gbk")
    crlf = "\r\n" in text
    text = text.replace("\r\n", "\n")
    changed = []
    for macro in macros:
        pat = re.compile(r"(#define\s+%s\s+)0\b" % macro)
        if pat.search(text):
            text = pat.sub(r"\g<1>1", text, count=1)
            changed.append(macro)
    if changed:
        if crlf:
            text = text.replace("\n", "\r\n")
        open(conf_path, "wb").write(text.encode(enc))
    for m in changed:
        log("  lv_conf: %s -> 1" % m)
    return changed



def fill(tpl, **kw):
    """模板填充：先替换 {name} 占位符，再把 {{ }} 还原成 { } —— C 代码里花括号太多，
    用 str.format 会炸，这样最省心。"""
    for k, v in kw.items():
        tpl = tpl.replace("{" + k + "}", str(v))
    return tpl.replace("{{", "{").replace("}}", "}")


# ------------------------------------------------------------------ 代码模板
UI_C = """/******************************************************************************
 * 文件名   : {prefix}_ui.c
 * 说明     : {screen} 页面 —— 由 lvglab2task.py 生成，重新导出后请重跑脚本
 *
 * 生成来源 : {src_dir}
 * 本文件只做三件事：首次建页、load 到屏幕、关页时销毁。
 * 所有 lv_ 调用都在这里，而这里只会被任务线程以外的 kernel 线程调用。
 ******************************************************************************/

#include "lvgl/lvgl.h"
#include "{gen_include}"
#include "{prefix}_ui.h"

static lv_obj_t *s_page = NULL;

void {prefix}_ui_render(void)
{
    if(s_page == NULL)                      /* 第一次进来：把控件都建出来 */
    {{
        ui_{screen}_screen_init();
        s_page = ui_{screen};
    }}

    if(lv_scr_act() != s_page)              /* 切回本页：load 上去 */
    {{
        lv_scr_load(s_page);
    }}

    /* TODO: 数据更新写这里（例如 lv_label_set_text / lv_chart_set_next_value）
     *       控件句柄是 ui_{screen}.h 里的 ui_XXX 全局，本文件已 include */
}}

void {prefix}_ui_destroy(void)
{{
    if(s_page == NULL)
    {{
        return;
    }}

    ui_{screen}_screen_destroy();           /* 生成代码负责 lv_obj_del + 置 NULL */
    s_page = NULL;
}}
"""

UI_H = """/******************************************************************************
 * 文件名   : {prefix}_ui.h
 * 说明     : {screen} 页面接口（lvglab2task.py 生成）
 *
 * 顺带把生成代码的头文件包含进来，任务里要用 ui_XXX 句柄时不用再找路径。
 ******************************************************************************/

#ifndef __{PREFIX}_UI_H
#define __{PREFIX}_UI_H

#include "lvgl/lvgl.h"          /* 生成代码的头文件自己不带 include，必须先给 lv_obj_t */
#include "{gen_include}"

void {prefix}_ui_render(void);      /* kernel 线程：首次建页 + load */
void {prefix}_ui_destroy(void);     /* kernel 线程：销毁页面，归还 LVGL 堆 */

#endif
"""

TASK_C = """/******************************************************************************
 * 文件名   : task_{prefix}.c
 * 说明     : {NAME} —— 由 lvglab2task.py 生成的任务骨架
 *
 * 界面来自 LVGLab 导出的 {screen}，绘制交给 {prefix}_ui.c；
 * 本文件只管业务：数据怎么来、什么时候画、按键怎么响应。
 *
 * 结构（和 task_example.c 完全一致）：
 *     ① 数据 ② 快照 ③ 资源 ④ 逻辑 ⑤ 线程本体 ⑥ 生命周期 / 渲染
 ******************************************************************************/

#include "router.h"
#include "key.h"
#include "ui/{prefix}_ui.h"
#include "rtthread.h"
#include "rthw.h"

/**************************************************************************************************
 *                                        ① 数据（只有本线程写）
 **************************************************************************************************/
static int32_t s_value = 0;         /* TODO: 换成你的业务数据 */

/**************************************************************************************************
 *                                        ② 快照（本线程写 / render 读）
 **************************************************************************************************/
typedef struct
{{
    int32_t value;
}} {prefix}_view_t;

static {prefix}_view_t s_view;

static void view_push(void)
{{
    {prefix}_view_t v;
    rt_base_t level;

    v.value = s_value;

    level = rt_hw_interrupt_disable();
    s_view = v;
    rt_hw_interrupt_enable(level);
}}

/**************************************************************************************************
 *                                        ③ 资源（open 建 / close 删）
 **************************************************************************************************/
static rt_sem_t    s_exit = RT_NULL;
static rt_mq_t     s_q    = RT_NULL;
static rt_thread_t s_tid  = RT_NULL;

/**************************************************************************************************
 *                                        ④ 逻辑（跑在自己的线程里）
 **************************************************************************************************/
static void {prefix}_tick(void)
{{
    /* TODO: 周期活（采样、计算……），没有就留空 */
}}

static void {prefix}_key(key_id_t key)
{{
    switch(key)
    {{
        case KEY_ID_1:
            break;

        default:
            return;
    }}

    view_push();
}}

static void {prefix}_set(uint8_t id, int32_t value)
{{
    (void)value;

    switch(id)
    {{
        case 1:
            break;

        default:
            return;
    }}

    view_push();
}}

/**************************************************************************************************
 *                                        ⑤ 线程本体
 **************************************************************************************************/
static void {prefix}_thread(void *arg)
{{
    message msg;

    (void)arg;

    view_push();

    for(;;)
    {{
        if(rt_mq_recv(s_q, &msg, sizeof(msg), rt_tick_from_millisecond({tick_ms})) == RT_EOK)
        {{
            if(msg.type == 2)                   /* 2 = 管理端让退出 */
            {{
                break;
            }}

            if(msg.type == 0)                   /* 0 = 实体按键 */
            {{
                {prefix}_key((key_id_t)msg.code);
            }}
            else                                /* 1 = 触摸控件 */
            {{
                {prefix}_set(msg.code, msg.value);
            }}
        }}

        {prefix}_tick();
    }}

    rt_sem_release(s_exit);
}}

/**************************************************************************************************
 *                                        ⑥ 打开 / 关闭 / 投递 / 渲染
 **************************************************************************************************/
static void {prefix}_open(void)
{{
    if(s_tid != RT_NULL)
    {{
        return;
    }}

    s_value = 0;

    s_exit = rt_sem_create("{prefix}ex", 0, RT_IPC_FLAG_FIFO);
    s_q    = rt_mq_create("{NAME}", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    s_tid  = rt_thread_create("{NAME}", {prefix}_thread, RT_NULL, 1024, 12, 10);

    if((s_exit == RT_NULL) || (s_q == RT_NULL) || (s_tid == RT_NULL))
    {{
        rt_kprintf("{prefix}: open failed\\r\\n");

        if(s_tid  != RT_NULL) {{ rt_thread_delete(s_tid); s_tid  = RT_NULL; }}
        if(s_q    != RT_NULL) {{ rt_mq_delete(s_q);       s_q    = RT_NULL; }}
        if(s_exit != RT_NULL) {{ rt_sem_delete(s_exit);   s_exit = RT_NULL; }}
        return;
    }}

    rt_thread_startup(s_tid);
}}

static void {prefix}_close(void)
{{
    message bye = {{2, 0, 0}};
    message drop;
    uint8_t retry;

    if(s_tid == RT_NULL)
    {{
        return;
    }}

    for(retry = 0; retry < 20; retry++)     /* 队列满就腾位重投 */
    {{
        if(rt_mq_send(s_q, &bye, sizeof(bye)) == RT_EOK)
        {{
            break;
        }}
        (void)rt_mq_recv(s_q, &drop, sizeof(drop), 0);
    }}

    if(retry >= 20)
    {{
        rt_kprintf("{prefix}: quit msg failed\\r\\n");
        return;
    }}

    if(rt_sem_take(s_exit, rt_tick_from_millisecond(3000)) != RT_EOK)
    {{
        rt_kprintf("{prefix}: exit timeout\\r\\n");
        return;
    }}

    rt_mq_delete(s_q);      s_q    = RT_NULL;
    rt_sem_delete(s_exit);  s_exit = RT_NULL;
    s_tid = RT_NULL;
}}

static void {prefix}_post(const message *msg)
{{
    if(s_q != RT_NULL)
    {{
        (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));
    }}
}}

static void {prefix}_render(void)
{{
    {prefix}_view_t v;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    v = s_view;
    rt_hw_interrupt_enable(level);

    {prefix}_ui_render();                   /* 建页 / load */

    (void)v;                                /* TODO: 用 v 更新界面，例如
                                             * lv_label_set_text(ui_Label1, buf); */
}}

/**************************************************************************************************
 *                                        任务描述
 **************************************************************************************************/
const task_t task_{prefix} =
{{
    .name       = "{NAME}",          /* 调试打印用，也是线程名（<= 7 字符） */
    .keep       = 0,

    .open       = {prefix}_open,
    .close      = {prefix}_close,
    .post       = {prefix}_post,
    .render     = {prefix}_render,
    .ui_destroy = {prefix}_ui_destroy,
}};
"""


# ------------------------------------------------------------------ 转换主流程
def convert(src, screen, task, project, prefix=None, gen_include=None,
            tick_ms=100, copy_gen=True, fix_lvconf=True, make_task=True,
            register=False, force=True, log=print):
    src_dir = find_src_dir(src)
    if not os.path.isfile(os.path.join(src_dir, "ui_%s.c" % screen)):
        log("!! 找不到 %s/ui_%s.c，请确认导出目录和 Screens 选择" % (src_dir, screen))
        return False

    task = task.upper()
    prefix = (prefix or task).lower()
    gen_include = gen_include or ("gen/ui_%s.h" % screen)
    project = os.path.abspath(project)
    ui_dir = os.path.join(project, "User", "ui")
    task_dir = os.path.join(project, "User", "task")

    widgets = parse_widgets(src_dir, screen)
    log("源目录  : %s" % src_dir)
    log("Screen  : %s（%d 个控件：%s）" % (screen, len(widgets), ", ".join(widgets) or "无"))

    # 1) 检查 lv_conf
    need = scan_needed_macros(src_dir, screen)
    if need:
        conf = find_lv_conf(project)
        if conf is None:
            log("!! 工程里没找到 lv_conf.h，宏检查跳过（需要手动确认：%s）"
                % ", ".join(need.keys()))
        else:
            have = conf_values(conf)
            missing = [m for m in need if have.get(m, 0) == 0]
            log("lv_conf : %s" % conf)
            if missing:
                for m in missing:
                    log("  !! %s = 0，但生成代码用了 %s" % (m, need[m]))
                if fix_lvconf:
                    fix_conf(conf, missing, log)
                    log("  已改为 1 —— 记得 Clean + Rebuild")
            else:
                log("  需要的宏都开着，OK")

    # 2) 复制生成代码
    if copy_gen:
        gen_dir = os.path.join(ui_dir, "gen")
        os.makedirs(gen_dir, exist_ok=True)
        n = 0
        for f in glob.glob(os.path.join(src_dir, "*.c")) + glob.glob(os.path.join(src_dir, "*.h")):
            shutil.copy2(f, os.path.join(gen_dir, os.path.basename(f)))
            n += 1
        log("已复制  : %d 个文件 -> %s" % (n, gen_dir))

    # 3) 生成 ui 胶水
    out_c = os.path.join(ui_dir, "%s_ui.c" % prefix)
    out_h = os.path.join(ui_dir, "%s_ui.h" % prefix)
    if not force and os.path.isfile(out_c):
        log("!! %s 已存在，跳过（勾选覆盖后重试）" % out_c)
    else:
        fmt = dict(prefix=prefix, PREFIX=prefix.upper(), screen=screen,
                   src_dir=src_dir, gen_include=gen_include)
        log("生成    : %s (%d B)" % (out_c, write_gbk(out_c, fill(UI_C, **fmt))))
        log("生成    : %s (%d B)" % (out_h, write_gbk(out_h, fill(UI_H, **fmt))))

    # 4) 生成任务骨架
    if make_task:
        out_t = os.path.join(task_dir, "task_%s.c" % prefix)
        if not force and os.path.isfile(out_t):
            log("!! %s 已存在，跳过（勾选覆盖后重试）" % out_t)
        else:
            fmt = dict(prefix=prefix, NAME=task, screen=screen, tick_ms=tick_ms)
            log("生成    : %s (%d B)" % (out_t, write_gbk(out_t, fill(TASK_C, **fmt))))

    # 5) 注册到 menu.c
    if register:
        menu = os.path.join(task_dir, "menu.c")
        if os.path.isfile(menu):
            changed = register_menu(menu, prefix, log)
            if not changed:
                log("menu.c  : 已经有 task_%s，无需改动" % prefix)
        else:
            log("!! 没找到 %s，跳过注册" % menu)

    # 6) 提示
    log("")
    log("接下来：")
    log("  1. 在 menu.c 里确认有这两行（脚本已尝试自动加）：")
    log("        extern const task_t task_%s;" % prefix)
    log("        &task_%s," % prefix)
    log("  2. MRS 里 F5 刷新工程，Clean + Rebuild")
    log("  3. 任务里填 ④ 逻辑 和 %s_ui_render() 末尾的数据更新" % prefix)
    return True


def register_menu(menu_path, prefix, log):
    """往 menu.c 的 g_app_list[] 里加一行，并补 extern（GBK 安全）"""
    raw = open(menu_path, "rb").read()
    try:
        text = raw.decode("utf-8")
        enc = "utf-8"
    except UnicodeDecodeError:
        text = raw.decode("gbk")
        enc = "gbk"
    crlf = "\r\n" in text

    sym = "task_%s" % prefix
    if re.search(r"extern\s+const\s+task_t\s+%s\s*;" % sym, text):
        return False

    t = text.replace("\r\n", "\n")

    # 1) extern：插在最后一个 extern const task_t 后面
    m = list(re.finditer(r"[ \t]*extern\s+const\s+task_t\s+\w+\s*;[^\n]*\n", t))
    if m:
        pos = m[-1].end()
        t = t[:pos] + "extern const task_t %s;\n" % sym + t[pos:]
    else:
        marker = "static const task_t *const g_app_list[] ="
        pos = t.find(marker)
        if pos < 0:
            return False
        t = t[:pos] + "extern const task_t %s;\n\n" % sym + t[pos:]

    # 2) 表里加一行：插在 g_app_list 的 "};" 之前
    start = t.find("g_app_list[]")
    end = t.find("};", start)
    if start < 0 or end < 0:
        return False
    t = t[:end] + "    &%s,\n" % sym + t[end:]

    if crlf:
        t = t.replace("\n", "\r\n")
    open(menu_path + ".bak", "wb").write(raw)            # 备份
    open(menu_path, "wb").write(t.encode(enc))
    log("menu.c  : 已加入 &%s（备份 menu.c.bak）" % sym)
    return True


# ------------------------------------------------------------------ GUI
def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, scrolledtext

    root = tk.Tk()
    root.title("LVGLab → task_ui 转换器  v%s" % VERSION)
    root.geometry("760x560")
    root.minsize(680, 480)

    frm = ttk.Frame(root, padding=10)
    frm.pack(fill="both", expand=True)

    def row(label, r, browse=None, width=62):
        ttk.Label(frm, text=label).grid(row=r, column=0, sticky="w", pady=4)
        var = tk.StringVar()
        ent = ttk.Entry(frm, textvariable=var, width=width)
        ent.grid(row=r, column=1, sticky="we", padx=6)
        if browse:
            ttk.Button(frm, text="浏览…", width=8,
                       command=lambda: browse(var)).grid(row=r, column=2)
        return var

    def pick_dir(var):
        d = filedialog.askdirectory(title="选择目录")
        if d:
            var.set(os.path.normpath(d))

    src_var = row("LVGLab 导出目录", 0, pick_dir)
    proj_var = row("工程根目录", 1, pick_dir)

    ttk.Label(frm, text="Screen").grid(row=2, column=0, sticky="w", pady=4)
    scr_var = tk.StringVar()
    scr_box = ttk.Combobox(frm, textvariable=scr_var, values=[], width=24, state="readonly")
    scr_box.grid(row=2, column=1, sticky="w", padx=6)
    ttk.Button(frm, text="扫描", width=8, command=lambda: scan()).grid(row=2, column=2)

    ttk.Label(frm, text="任务名 / 前缀").grid(row=3, column=0, sticky="w", pady=4)
    sub = ttk.Frame(frm)
    sub.grid(row=3, column=1, columnspan=2, sticky="we", padx=6)
    task_var = tk.StringVar(value="CHART")
    pref_var = tk.StringVar(value="chart")
    ttk.Entry(sub, textvariable=task_var, width=14).pack(side="left")
    ttk.Label(sub, text="  前缀").pack(side="left")
    ttk.Entry(sub, textvariable=pref_var, width=14).pack(side="left", padx=6)
    ttk.Label(sub, text="（文件名/函数名用它，如 chart_ui.c）").pack(side="left")

    ttk.Label(frm, text="生成代码头文件").grid(row=4, column=0, sticky="w", pady=4)
    inc_var = tk.StringVar(value="gen/ui_Screen1.h")
    ttk.Entry(frm, textvariable=inc_var, width=40).grid(row=4, column=1, sticky="w", padx=6)

    ttk.Label(frm, text="任务节拍 (ms)").grid(row=5, column=0, sticky="w", pady=4)
    tick_var = tk.StringVar(value="100")
    ttk.Entry(frm, textvariable=tick_var, width=10).grid(row=5, column=1, sticky="w", padx=6)

    opt = ttk.Frame(frm)
    opt.grid(row=6, column=0, columnspan=3, sticky="w", pady=6)
    copy_var = tk.BooleanVar(value=True)
    task_var_on = tk.BooleanVar(value=True)
    conf_var = tk.BooleanVar(value=True)
    reg_var = tk.BooleanVar(value=True)
    force_var = tk.BooleanVar(value=True)
    for text, v in (("复制生成代码到 User/ui/gen", copy_var),
                    ("生成 task 骨架", task_var_on),
                    ("自动打开 lv_conf 缺的宏", conf_var),
                    ("自动注册到 menu.c", reg_var),
                    ("覆盖已存在的文件", force_var)):
        ttk.Checkbutton(opt, text=text, variable=v).pack(side="left", padx=(0, 10))

    log_box = scrolledtext.ScrolledText(frm, height=18, font=("Consolas", 9))
    log_box.grid(row=8, column=0, columnspan=3, sticky="nsew", pady=6)
    frm.rowconfigure(8, weight=1)
    frm.columnconfigure(1, weight=1)

    def log(msg):
        log_box.insert("end", str(msg) + "\n")
        log_box.see("end")
        root.update_idletasks()

    def scan():
        d = src_var.get().strip()
        if not d or not os.path.isdir(d):
            log("!! 先选导出目录")
            return
        try:
            items = list_screens(find_src_dir(d))
        except Exception as e:
            log("!! 扫描失败：%s" % e)
            return
        scr_box["values"] = [s for s, _ in items]
        if items:
            scr_var.set(items[0][0])
            inc_var.set("gen/ui_%s.h" % items[0][0])
        log("扫描到 %d 个 Screen：%s" % (len(items),
                                       ", ".join("%s(%d控件)" % it for it in items)))

    def do_convert():
        log("=" * 60)
        if not os.path.isdir(src_var.get().strip()):
            log("!! 导出目录无效")
            return
        if not os.path.isdir(proj_var.get().strip()):
            log("!! 工程根目录无效")
            return
        try:
            tick = int(tick_var.get())
        except ValueError:
            tick = 100
        ok = convert(src=src_var.get().strip(),
                     screen=scr_var.get().strip(),
                     task=task_var.get().strip() or "TASK",
                     project=proj_var.get().strip(),
                     prefix=pref_var.get().strip() or None,
                     gen_include=inc_var.get().strip() or None,
                     tick_ms=tick,
                     copy_gen=copy_var.get(),
                     fix_lvconf=conf_var.get(),
                     make_task=task_var_on.get(),
                     register=reg_var.get(),
                     force=force_var.get(),
                     log=log)
        log("完成" if ok else "失败")

    scr_box.bind("<<ComboboxSelected>>",
                 lambda e: inc_var.set("gen/ui_%s.h" % scr_var.get()))

    btns = ttk.Frame(frm)
    btns.grid(row=7, column=0, columnspan=3, sticky="we", pady=4)
    ttk.Button(btns, text="开始转换", width=14, command=do_convert).pack(side="left")
    ttk.Button(btns, text="清空日志", width=10,
               command=lambda: log_box.delete("1.0", "end")).pack(side="left", padx=8)

    # 默认值
    here = os.path.dirname(os.path.abspath(sys.argv[0]))
    proj_var.set(os.path.dirname(here) if os.path.basename(here).lower() == "tools" else here)
    if len(sys.argv) > 1 and os.path.isdir(sys.argv[1]):
        src_var.set(os.path.normpath(sys.argv[1]))

    log("选好导出目录后点『扫描』，再点『开始转换』。")
    log("生成物一律 GBK + CRLF，直接进 MRS 不用转码。")
    root.mainloop()


# ------------------------------------------------------------------ 入口
def main():
    # 打包成 --windowed 的 exe 后，sys.stdout/stderr 是 None，print() 会抛异常
    if sys.stdout is None:
        sys.stdout = open(os.devnull, "w", encoding="utf-8")
    if sys.stderr is None:
        sys.stderr = sys.stdout

    ap = argparse.ArgumentParser(description="LVGLab 导出 -> task_ui 结构")
    ap.add_argument("--src", help="导出目录（export/ui 或 export/ui/src）")
    ap.add_argument("--screen", help="Screen 名，如 Screen1")
    ap.add_argument("--task", help="任务名（大写，<=7 字符），如 CHART")
    ap.add_argument("--prefix", help="文件名/函数名前缀，默认任务名小写")
    ap.add_argument("--project", help="工程根目录")
    ap.add_argument("--gen-include", help="生成代码头文件相对路径，默认 gen/ui_<Screen>.h")
    ap.add_argument("--tick", type=int, default=100, help="任务节拍 ms，默认 100")
    ap.add_argument("--no-copy", action="store_true", help="不复制生成代码")
    ap.add_argument("--no-task", action="store_true", help="不生成 task 骨架")
    ap.add_argument("--no-conf", action="store_true", help="不改 lv_conf")
    ap.add_argument("--register", action="store_true", help="自动注册到 menu.c")
    ap.add_argument("--list", action="store_true", help="只列出 Screen")
    args = ap.parse_args()

    if args.src and args.list:
        for s, n in list_screens(find_src_dir(args.src)):
            print("%-16s %d 个控件" % (s, n))
        return 0

    if not (args.src and args.screen and args.task and args.project):
        run_gui()
        return 0

    ok = convert(args.src, args.screen, args.task, args.project,
                 prefix=args.prefix, gen_include=args.gen_include, tick_ms=args.tick,
                 copy_gen=not args.no_copy, fix_lvconf=not args.no_conf,
                 make_task=not args.no_task, register=args.register)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
