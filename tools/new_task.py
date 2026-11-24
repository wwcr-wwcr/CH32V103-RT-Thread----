#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
任务生成器 —— 一键生成 task + ui 三件套（带图形界面）

双击运行就是界面；带参数运行就是命令行：

    python tools/new_task.py                       # 图形界面
    python tools/new_task.py task_veml7700         # 命令行生成
    python tools/new_task.py veml7700 --register   # 生成并注册进菜单
    python tools/new_task.py xxx --force           # 覆盖已存在的

生成（与 task 同目录，方便一起管理）：
    User/<名字>/<名字>.c          从 tools/templates/task_template.c 生成
    User/<名字>/<名字>_ui.c       空壳，等 LVGL 设计器粘贴
    User/<名字>/<名字>_ui.h       空壳，等 LVGL 设计器粘贴

--register 会改 User/task_menu/menu.c：加 extern 和 &task_xxx,
"""

import os
import re
import sys

APP_TITLE = "任务生成器 — task + ui 三件套"
TEMPLATE_REL = os.path.join("tools", "templates", "task_template.c")
MENU_REL = os.path.join("User", "task_menu", "menu.c")
MARKER_REL = os.path.join("User", "tasks.h")          # 用来认工程根目录

UI_C = '''/******************************************************************************
 * 文件名   : {name}_ui.c
 * 说明     : 界面实现 —— 空壳，等 LVGL 设计器生成后整段粘贴进来
 *
 * 设计器里"文件名前缀"填 {name}（会自动补 _ui），生成的就是这一份。
 ******************************************************************************/

/* 还没有界面 */
'''

UI_H = '''/******************************************************************************
 * 文件名   : {name}_ui.h
 * 说明     : 界面头文件 —— 空壳，等 LVGL 设计器生成后把它的 .h 内容粘贴进来
 *
 * 生成后会是这样两行（现在先注释着，task 里也是注释调用，不影响编译）：
 *     void {name}_ui_render(...);
 *     void {name}_ui_destroy(void);
 ******************************************************************************/

#ifndef {guard}
#define {guard}

#include <stdint.h>

/* 等设计器生成后粘贴到这里 */

#endif
'''


# ------------------------------------------------------------------ 基础工具
def read_text(path):
    """工程里源码是 GBK，也可能有 UTF-8 的，自动认"""
    raw = open(path, "rb").read()
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("gbk")


def save_gbk(path, text):
    """统一写 GBK + CRLF，和工程保持一致"""
    open(path, "wb").write(text.replace("\r\n", "\n").replace("\n", "\r\n")
                           .encode("gbk", "replace"))


def find_root(start):
    """从 start 往上找，直到看见 User/tasks.h"""
    cur = os.path.abspath(start)
    for _ in range(8):
        if os.path.exists(os.path.join(cur, MARKER_REL)):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            break
        cur = parent
    return None


def normalize(name):
    """task_veml7700 / veml7700 -> task_veml7700；非法返回 None"""
    name = (name or "").strip().lower().replace("-", "_").replace(" ", "_")
    if not name:
        return None
    if not name.startswith("task_"):
        name = "task_" + name
    if not re.fullmatch(r"task_[a-z0-9_]+", name):
        return None
    return name


def short_name(name):
    """线程名 <= 7 字符"""
    return name[5:].upper()[:6] if name.startswith("task_") else name.upper()[:6]


# ------------------------------------------------------------------ 生成逻辑
def generate(root, name, force=False, register=False, template=None, log=None):
    """返回 (是否成功, 提示信息, 生成的文件列表)"""
    name = normalize(name)
    if not name:
        return False, "名字不合法：只能用 a-z 0-9 _，例如 task_veml7700", []

    if not root or not os.path.isdir(root):
        return False, "工程根目录不对：%s" % root, []

    tpl_path = template or os.path.join(root, TEMPLATE_REL)
    if not os.path.exists(tpl_path):
        return False, "找不到模板：%s" % tpl_path, []

    tpl = read_text(tpl_path).replace("\r\n", "\n")
    task_c = tpl.replace("task_name", name).replace('"NAME"', '"%s"' % short_name(name))

    folder = os.path.join(root, "User", name)
    plan = [
        (os.path.join(folder, "%s.c" % name), task_c),
        (os.path.join(folder, "%s_ui.c" % name), UI_C.format(name=name)),
        (os.path.join(folder, "%s_ui.h" % name),
         UI_H.format(name=name, guard="__%s_UI_H" % name.upper())),
    ]

    exists = [p for p, _ in plan if os.path.exists(p)]
    if exists and not force:
        names = "\n  ".join(os.path.relpath(p, root) for p in exists)
        return False, "这些文件已存在（勾选“覆盖已有文件”才会替换）：\n  " + names, []

    os.makedirs(folder, exist_ok=True)
    made = []
    for path, text in plan:
        save_gbk(path, text)
        made.append(os.path.relpath(path, root))
        if log:
            log("生成: " + os.path.relpath(path, root))

    msg = "生成完成，共 %d 个文件。" % len(made)

    if register:
        menu = os.path.join(root, MENU_REL)
        if not os.path.exists(menu):
            msg += "\n!! 找不到 menu.c，没注册"
        else:
            t = read_text(menu).replace("\r\n", "\n")
            t2, how = _register_in_menu(name, t)
            if t2 == t:
                msg += "\n菜单里已经有 %s，跳过注册" % name
                if log:
                    log("注册: 已存在，跳过")
            else:
                save_gbk(menu, t2)
                msg += "\n已注册到菜单（%s）" % how
                if log:
                    log("注册: menu.c " + how)

    msg += "\n\n下一步：\n  1) MRS 里 F5，把 %s 加进工程\n" % name
    msg += "  2) 设计器前缀填 %s_ui，生成后粘到 %s_ui.c / .h\n" % (name, name)
    msg += "  3) 把 %s.c 里 task_render / task_ui_destroy 的注释打开" % name
    return True, msg, made


def _register_in_menu(name, t):
    """往 menu.c 加 extern 和 &task_xxx, —— 返回 (新文本, 说明)"""
    how = []

    if ("extern const task_t %s;" % name) not in t:
        ms = list(re.finditer(r"^extern const task_t \w+;\n", t, flags=re.M))
        if ms:
            pos = ms[-1].end()
            t = t[:pos] + "extern const task_t %s;\n" % name + t[pos:]
        else:
            anchor = "//********************在这里写任务表********************//\n"
            if anchor in t:
                t = t.replace(anchor, anchor + "extern const task_t %s;\n" % name)
        how.append("extern")

    if ("&%s," % name) not in t:
        i = t.index("static const task_t *const g_app_list[] =")
        j = t.index("};", i)
        ls = t.rindex("\n", 0, j) + 1
        t = t[:ls] + "    &%s,\n" % name + t[ls:]
        how.append("g_app_list")

    return t, " + ".join(how) if how else "无变化"


# ------------------------------------------------------------------ 图形界面
def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    root = tk.Tk()
    root.title(APP_TITLE)
    root.geometry("620x520")
    root.minsize(560, 460)

    try:
        ttk.Style().theme_use("vista")
    except Exception:
        pass

    pad = {"padx": 10, "pady": 6}

    # ---- 工程根目录 ----
    frm = ttk.LabelFrame(root, text="工程根目录")
    frm.pack(fill="x", **pad)

    root_var = tk.StringVar()
    detected = find_root(os.path.dirname(os.path.abspath(sys.argv[0]))) or find_root(os.getcwd())
    root_var.set(detected or "")

    row = ttk.Frame(frm)
    row.pack(fill="x", padx=8, pady=8)
    ttk.Entry(row, textvariable=root_var).pack(side="left", fill="x", expand=True)

    def pick_root():
        d = filedialog.askdirectory(title="选工程根目录（含 User/tasks.h 的那层）")
        if d:
            root_var.set(os.path.normpath(d))

    ttk.Button(row, text="浏览…", command=pick_root).pack(side="left", padx=(6, 0))

    # ---- 任务名 ----
    frm2 = ttk.LabelFrame(root, text="任务名")
    frm2.pack(fill="x", **pad)

    name_var = tk.StringVar(value="task_")
    row2 = ttk.Frame(frm2)
    row2.pack(fill="x", padx=8, pady=8)
    ttk.Label(row2, text="名字").pack(side="left")
    ent = ttk.Entry(row2, textvariable=name_var, width=28)
    ent.pack(side="left", padx=8)
    hint = ttk.Label(row2, text="", foreground="#666")
    hint.pack(side="left")

    def on_name(*_):
        n = normalize(name_var.get())
        if not n:
            hint.config(text="（auto：会补 task_ 前缀）", foreground="#999")
        else:
            hint.config(text="→ %s  线程名 %s" % (n, short_name(n)), foreground="#2b7a4b")

    name_var.trace_add("write", on_name)
    on_name()

    # ---- 选项 ----
    frm3 = ttk.Frame(root)
    frm3.pack(fill="x", padx=10, pady=(0, 4))
    reg_var = tk.BooleanVar(value=True)
    force_var = tk.BooleanVar(value=False)
    ttk.Checkbutton(frm3, text="注册到菜单（改 menu.c：extern + g_app_list）",
                    variable=reg_var).pack(anchor="w")
    ttk.Checkbutton(frm3, text="覆盖已存在的文件", variable=force_var).pack(anchor="w")

    # ---- 按钮 ----
    frm4 = ttk.Frame(root)
    frm4.pack(fill="x", padx=10, pady=6)

    log_box = tk.Text(root, height=12, wrap="word", state="disabled",
                      bg="#fbfbfd", relief="solid", bd=1)
    log_box.pack(fill="both", expand=True, padx=10, pady=(0, 10))

    def log(s):
        log_box.config(state="normal")
        log_box.insert("end", s + "\n")
        log_box.see("end")
        log_box.config(state="disabled")

    last_folder = {"path": None}

    def do_generate():
        r = root_var.get().strip()
        if not r or not os.path.isdir(r):
            messagebox.showerror(APP_TITLE, "工程根目录不对，请先选对")
            return
        ok, msg, made = generate(r, name_var.get(), force=force_var.get(),
                                 register=reg_var.get(), log=log)
        if ok:
            n = normalize(name_var.get())
            last_folder["path"] = os.path.join(r, "User", n)
            messagebox.showinfo("完成", msg)
        else:
            messagebox.showerror("失败", msg)

    def open_folder():
        p = last_folder["path"]
        if p and os.path.isdir(p):
            os.startfile(p)
        else:
            messagebox.showinfo(APP_TITLE, "先生成一个任务，或者手动去 User/ 下找")

    ttk.Button(frm4, text="生 成", command=do_generate).pack(side="left")
    ttk.Button(frm4, text="打开任务文件夹", command=open_folder).pack(side="left", padx=8)
    ttk.Label(frm4, text="模板：" + TEMPLATE_REL, foreground="#888").pack(side="right")

    log("模板: %s" % os.path.join(root_var.get() or "<未选>", TEMPLATE_REL))
    log("填个名字（例如 veml7700），点“生成”即可。")

    ent.focus_set()
    ent.icursor("end")
    root.mainloop()


# ------------------------------------------------------------------ 命令行
def run_cli(argv):
    args = [a for a in argv if not a.startswith("--")]
    flags = [a for a in argv if a.startswith("--")]

    if not args:
        print(__doc__)
        return 2

    root = find_root(os.path.dirname(os.path.abspath(sys.argv[0]))) or find_root(os.getcwd())
    if not root:
        print("找不到工程根目录（应从工程内运行）")
        return 2

    ok, msg, _ = generate(root, args[0],
                          force="--force" in flags,
                          register="--register" in flags,
                          log=lambda s: print(s))
    print(msg)
    return 0 if ok else 1


def main():
    argv = sys.argv[1:]

    if "--selftest" in argv:
        # 给打包后的 exe 做无界面自检：往 exe 旁边写 selftest.log
        base = os.path.dirname(os.path.abspath(sys.argv[0]))
        root = find_root(base) or find_root(os.getcwd())
        lines = ["root=%s" % root]
        ok = False
        if root:
            ok, msg, made = generate(root, "task_selftest", force=True, register=False)
            lines.append("ok=%s" % ok)
            lines.append(msg)
            lines += made
            for p in made:
                try:
                    os.remove(os.path.join(root, p))
                except OSError:
                    pass
            d = os.path.join(root, "User", "task_selftest")
            try:
                os.rmdir(d)
            except OSError:
                pass
        try:
            open(os.path.join(base, "selftest.log"), "w", encoding="utf-8").write("\n".join(lines))
        except OSError:
            pass
        return 0 if ok else 1

    if argv:
        return run_cli(argv)

    run_gui()
    return 0


if __name__ == "__main__":
    sys.exit(main())
