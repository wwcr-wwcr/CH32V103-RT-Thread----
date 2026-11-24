#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
字体生成器 —— 图形界面版 lv_font_conv（离线）

双击运行就是界面；带参数就是命令行：

    python tools/font_gui.py                      # 界面
    python tools/font_gui.py --selftest           # 无界面自检（打包后验证用）

界面里做的事：
    选字库文件 -> 填要的字（或点预设）-> 选字号/bpp -> 生成 .c

依赖同目录的 lv_font_conv.exe（43MB 单文件，v1.5.3，LVGL 8 格式）。
"""

import os
import re
import subprocess
import sys

APP_TITLE = "字体生成器 — lv_font_conv（LVGL 8）"
CONV_NAME = "lv_font_conv.exe"
MARKER_REL = os.path.join("User", "tasks.h")

# 常用字符预设
PRESET_ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
PRESET_NUM = "0123456789.-+%℃ :/"
PRESET_CN = ("设定当前温度光照度距离数据存储波形类型方正弦三角频率峰值平均"
             "电机转速车体角度加速度时间日期开关状态正常错误报警")
PRESET_UNIT = "℃%mVsHzlxrpm°"


# ------------------------------------------------------------------ 路径工具
def base_dir():
    """exe（或脚本）所在目录"""
    return os.path.dirname(os.path.abspath(sys.argv[0]))


def find_root(start=None):
    """往上找含 User/tasks.h 的目录"""
    cur = os.path.abspath(start or base_dir())
    for _ in range(8):
        if os.path.exists(os.path.join(cur, MARKER_REL)):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            break
        cur = parent
    return None


def find_converter():
    """找 lv_font_conv.exe：先看自己旁边，再看 tools/，最后看 PATH"""
    cands = [
        os.path.join(base_dir(), CONV_NAME),
        os.path.join(base_dir(), "tools", CONV_NAME),
    ]
    root = find_root()
    if root:
        cands.append(os.path.join(root, "tools", CONV_NAME))
        cands.append(os.path.join(root, CONV_NAME))
    for p in cands:
        if os.path.exists(p):
            return p
    return None


def run_conv(conv, font, symbols, size, bpp, out, name, cwd=None):
    """调用 lv_font_conv，返回 (成功?, 输出文本)"""
    if symbols:
        sel = ["--symbols", symbols]
    else:
        return False, "没有要用到的字符"

    cmd = [conv, "--font", font] + sel + [
        "--size", str(size), "--bpp", str(bpp),
        "--format", "lvgl", "--lv-font-name", name, "-o", out]

    flags = 0
    if os.name == "nt":
        flags = 0x08000000        # CREATE_NO_WINDOW，别弹黑框

    try:
        r = subprocess.run(cmd, capture_output=True, creationflags=flags, cwd=cwd)
    except OSError as e:
        return False, "调用失败：%s" % e

    msg = (r.stdout or b"").decode("utf-8", "replace").strip()
    err = (r.stderr or b"").decode("utf-8", "replace").strip()

    if r.returncode != 0 or not os.path.exists(out):
        return False, (err or msg or "未知错误（返回码 %d）" % r.returncode)

    return True, (msg or "ok")


def human(n):
    return "%.1f KB" % (n / 1024.0) if n >= 1024 else "%d B" % n


# ------------------------------------------------------------------ 图形界面
def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    conv = find_converter()
    root_dir = find_root()

    win = tk.Tk()
    win.title(APP_TITLE)
    win.geometry("680x640")
    win.minsize(620, 560)

    try:
        ttk.Style().theme_use("vista")
    except Exception:
        pass

    pad = {"padx": 10, "pady": 6}

    # ---------- 字库 ----------
    f1 = ttk.LabelFrame(win, text="字库文件（.ttf / .otf）")
    f1.pack(fill="x", **pad)

    font_var = tk.StringVar(value="C:\\Windows\\Fonts\\simhei.ttf")
    r = ttk.Frame(f1)
    r.pack(fill="x", padx=8, pady=8)
    ttk.Entry(r, textvariable=font_var).pack(side="left", fill="x", expand=True)

    def pick_font():
        p = filedialog.askopenfilename(
            title="选字体文件", filetypes=[("字体", "*.ttf *.otf"), ("所有文件", "*.*")])
        if p:
            font_var.set(os.path.normpath(p))

    ttk.Button(r, text="浏览…", command=pick_font).pack(side="left", padx=(6, 0))

    # ---------- 字符 ----------
    f2 = ttk.LabelFrame(win, text="要生成的字符")
    f2.pack(fill="both", expand=True, **pad)

    txt = tk.Text(f2, height=5, wrap="word", relief="solid", bd=1)
    txt.pack(fill="both", expand=True, padx=8, pady=(8, 4))
    txt.insert("1.0", "设定当前温度光照度距离波形频率")

    bar = ttk.Frame(f2)
    bar.pack(fill="x", padx=8, pady=(0, 8))

    cnt_lbl = ttk.Label(bar, text="", foreground="#2b7a4b")
    cnt_lbl.pack(side="right")

    def set_chars(s):
        txt.delete("1.0", "end")
        txt.insert("1.0", s)
        on_change()

    def add_chars(s):
        txt.insert("end", s)
        on_change()

    ttk.Button(bar, text="ASCII 可见字符", command=lambda: set_chars(PRESET_ASCII)).pack(side="left")
    ttk.Button(bar, text="数字符号", command=lambda: add_chars(PRESET_NUM)).pack(side="left", padx=4)
    ttk.Button(bar, text="单位", command=lambda: add_chars(PRESET_UNIT)).pack(side="left")
    ttk.Button(bar, text="常用汉字", command=lambda: add_chars(PRESET_CN)).pack(side="left", padx=4)
    ttk.Button(bar, text="清空", command=lambda: set_chars("")).pack(side="left")

    # ---------- 参数 ----------
    f3 = ttk.LabelFrame(win, text="参数")
    f3.pack(fill="x", **pad)

    p1 = ttk.Frame(f3)
    p1.pack(fill="x", padx=8, pady=8)

    ttk.Label(p1, text="字号").pack(side="left")
    size_var = tk.StringVar(value="20")
    ttk.Spinbox(p1, from_=8, to=48, width=5, textvariable=size_var).pack(side="left", padx=(4, 14))

    ttk.Label(p1, text="抗锯齿 bpp").pack(side="left")
    bpp_var = tk.StringVar(value="1")
    ttk.Combobox(p1, values=["1", "2", "4", "8"], width=4, state="readonly",
                 textvariable=bpp_var).pack(side="left", padx=(4, 14))

    ttk.Label(p1, text="变量名").pack(side="left")
    name_var = tk.StringVar(value="lv_font_my")
    ttk.Entry(p1, textvariable=name_var, width=22).pack(side="left", padx=4)

    p2 = ttk.Frame(f3)
    p2.pack(fill="x", padx=8, pady=(0, 8))

    ttk.Label(p2, text="输出").pack(side="left")
    out_var = tk.StringVar(
        value=os.path.join(root_dir or "", "User", "lv_font_my.c") if root_dir
        else os.path.join(base_dir(), "lv_font_my.c"))
    ttk.Entry(p2, textvariable=out_var).pack(side="left", fill="x", expand=True, padx=4)

    def pick_out():
        p = filedialog.asksaveasfilename(
            title="输出到", defaultextension=".c",
            initialdir=os.path.dirname(out_var.get()) or ".",
            initialfile=os.path.basename(out_var.get()),
            filetypes=[("C 文件", "*.c")])
        if p:
            out_var.set(os.path.normpath(p))
            sync_name()

    ttk.Button(p2, text="另存…", command=pick_out).pack(side="left")

    def on_change(*_):
        s = txt.get("1.0", "end").strip()
        n = len(dict.fromkeys(s.replace("\n", "").replace("\r", "")))
        cnt_lbl.config(text="%d 个唯一字符" % n if n else "（空）",
                       foreground="#2b7a4b" if n else "#b03")

    def sync_name():
        """变量名跟输出文件名联动：lv_font_xxx.c -> lv_font_xxx"""
        b = os.path.splitext(os.path.basename(out_var.get()))[0]
        if re.fullmatch(r"[A-Za-z_]\w*", b or ""):
            name_var.set(b)

    txt.bind("<KeyRelease>", on_change)
    out_var.trace_add("write", lambda *_: sync_name())

    # ---------- 日志 ----------
    log = tk.Text(win, height=8, wrap="word", state="disabled",
                  bg="#fbfbfd", relief="solid", bd=1)
    log.pack(fill="both", expand=True, padx=10, pady=(4, 10))

    def say(s):
        log.config(state="normal")
        log.insert("end", s + "\n")
        log.see("end")
        log.config(state="disabled")

    # ---------- 生成 ----------
    f4 = ttk.Frame(win)
    f4.pack(fill="x", padx=10, pady=(0, 6))

    def do_gen():
        c = find_converter()
        if not c:
            messagebox.showerror(APP_TITLE,
                                 "找不到 lv_font_conv.exe\n\n请把它和本程序放在同一个文件夹（tools/）里。")
            return
        if not os.path.exists(font_var.get().strip()):
            messagebox.showerror(APP_TITLE, "字库文件不存在：\n%s" % font_var.get())
            return

        chars = txt.get("1.0", "end")
        chars = "".join(dict.fromkeys(chars.replace("\r", "").replace("\n", ""))).strip()
        if not chars:
            messagebox.showerror(APP_TITLE, "还没填要生成的字符")
            return

        try:
            size = int(size_var.get())
        except ValueError:
            messagebox.showerror(APP_TITLE, "字号要是整数")
            return

        out = out_var.get().strip()
        if not out.lower().endswith(".c"):
            out += ".c"
            out_var.set(out)

        nm = name_var.get().strip() or "lv_font_my"
        d = os.path.dirname(out)
        if d and not os.path.isdir(d):
            try:
                os.makedirs(d, exist_ok=True)
            except OSError as e:
                messagebox.showerror(APP_TITLE, "输出目录建不了：%s" % e)
                return

        say("—" * 46)
        say("字库: %s" % os.path.basename(font_var.get()))
        say("字符: %d 个，字号 %d，bpp %s" % (len(chars), size, bpp_var.get()))
        win.config(cursor="watch")
        win.update_idletasks()

        ok, msg = run_conv(c, font_var.get().strip(), chars, size, int(bpp_var.get()), out, nm)
        win.config(cursor="")

        if ok:
            sz = os.path.getsize(out)
            say("生成成功: %s  (%s)" % (out, human(sz)))
            say("声明: LV_FONT_DECLARE(%s);   或写进 lv_conf.h 的 LV_FONT_CUSTOM_DECLARE" % nm)
            if sz > 20 * 1024:
                say("⚠ 超过 20 KB，注意 FLASH（你现在用了 77%）")
            messagebox.showinfo("完成", "已生成：\n%s\n\n大小 %s，%d 个字符" % (out, human(sz), len(chars)))
        else:
            say("失败: " + msg)
            messagebox.showerror("失败", msg[:800])

    def open_dir():
        d = os.path.dirname(out_var.get().strip())
        if d and os.path.isdir(d):
            os.startfile(d)
        else:
            messagebox.showinfo(APP_TITLE, "输出目录还不存在")

    ttk.Button(f4, text="生 成", command=do_gen).pack(side="left")
    ttk.Button(f4, text="打开输出目录", command=open_dir).pack(side="left", padx=8)
    ttk.Label(f4, text=("转换器: " + (conv or "未找到 lv_font_conv.exe")),
              foreground="#888" if conv else "#b03").pack(side="right")

    on_change()
    say("字库: %s" % font_var.get())
    say("转换器: %s" % (conv or "未找到（把它放 tools/ 下）"))
    say("填好字后点“生成”，输出是 UTF-8 的 .c，F5 加进 MRS 工程即可。")

    win.mainloop()


# ------------------------------------------------------------------ 自检
def run_selftest():
    base = base_dir()
    logp = os.path.join(base, "font_selftest.log")
    lines = []
    conv = find_converter()
    ok = False
    if conv:
        out = os.path.join(base, "_selftest_font.c")
        ok, msg = run_conv(conv, "C:\\Windows\\Fonts\\simhei.ttf",
                           "温度光照度", 20, 1, out, "lv_font_selftest")
        lines.append("conv=%s" % conv)
        lines.append("ok=%s" % ok)
        lines.append(msg)
        if os.path.exists(out):
            lines.append("size=%d" % os.path.getsize(out))
            head = open(out, "rb").read(200).decode("utf-8", "replace")
            lines.append("head=%s" % head.split("\n")[0])
            try:
                os.remove(out)
            except OSError:
                pass
    else:
        lines.append("converter not found")

    try:
        open(logp, "w", encoding="utf-8").write("\n".join(lines))
    except OSError:
        pass
    return 0 if ok else 1


def main():
    if "--selftest" in sys.argv[1:]:
        return run_selftest()
    run_gui()
    return 0


if __name__ == "__main__":
    sys.exit(main())
