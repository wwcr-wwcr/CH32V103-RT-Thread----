#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
字体生成器 —— 图形界面包装 lv_font_conv（离线，单文件 exe）

就干一件事：把「字符表」变成 lv_font_cn.c。

字符只从 LVGL/GUI_APP/Font 来（或你直接改界面里的输入框）。
不加扫描、不加预设 —— 你要多少字就写多少字。

    python font_gui.py               # 图形界面
    python font_gui.py --selftest    # 无界面自检（打包验证用）
"""

import os
import os.path as osp
import subprocess
import sys

APP_TITLE = "字体生成器"
CONV_NAME = "lv_font_conv.exe"
MARKER_REL = osp.join("User", "tasks.h")
FONT_TTF_REL = osp.join("tools", "fusion-pixel-12px-monospaced-zh_hans.ttf")
FONT_LIST_REL = osp.join("LVGL", "GUI_APP", "Font")
FONT_OUT_REL = osp.join("LVGL", "GUI_APP", "lv_font_cn12.c")


# ------------------------------------------------------------------ 路径
def base_dir():
    return osp.dirname(osp.abspath(sys.argv[0]))


def find_root(start=None):
    """往上找含 User/tasks.h 的目录 = 工程根"""
    cur = osp.abspath(start or base_dir())
    for _ in range(8):
        if osp.exists(osp.join(cur, MARKER_REL)):
            return cur
        parent = osp.dirname(cur)
        if parent == cur:
            break
        cur = parent
    return None


def find_converter(root):
    cands = [osp.join(base_dir(), CONV_NAME)]
    if root:
        cands += [osp.join(root, "tools", CONV_NAME), osp.join(root, CONV_NAME)]
    for p in cands:
        if osp.exists(p):
            return p
    return None


def read_font_list(root):
    """读 Font：跳过空行和 # 行，收所有非 ASCII 字符（去重保序）"""
    p = osp.join(root, FONT_LIST_REL)
    if not osp.exists(p):
        return ""
    t = None
    for enc in ("utf-8", "gbk"):
        try:
            t = open(p, encoding=enc).read()
            break
        except UnicodeDecodeError:
            continue
    if t is None:
        return ""
    out = []
    for line in t.replace("\r\n", "\n").split("\n"):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        for c in s:
            if ord(c) > 0x7F and c not in out:
                out.append(c)
    return "".join(out)


def write_font_list(root, chars):
    """回写 Font：纯字符，12 字一行，不带任何注释"""
    p = osp.join(root, FONT_LIST_REL)
    d = osp.dirname(p)
    if d and not osp.isdir(d):
        os.makedirs(d, exist_ok=True)
    lines = [chars[i:i + 12] for i in range(0, len(chars), 12)]
    open(p, "w", encoding="utf-8", newline="\r\n").write("\n".join(lines) + "\n")


def clean_chars(text):
    """输入框内容 -> 去重保序的字符集（只留非 ASCII）"""
    out = []
    for c in text.replace("\r", "").replace("\n", ""):
        if ord(c) > 0x7F and c not in out:
            out.append(c)
    return "".join(out)


def run_conv(conv, ttf, chars, size, bpp, out, name, fallback):
    cmd = [conv, "--font", ttf, "--symbols", chars,
           "--size", str(size), "--bpp", str(bpp),
           "--format", "lvgl", "--lv-font-name", name, "-o", out]
    if fallback:
        cmd += ["--lv-fallback", fallback]

    flags = 0x08000000 if os.name == "nt" else 0      # CREATE_NO_WINDOW
    try:
        r = subprocess.run(cmd, capture_output=True, creationflags=flags)
    except OSError as e:
        return False, "调用失败：%s" % e

    msg = (r.stdout or b"").decode("utf-8", "replace").strip()
    err = (r.stderr or b"").decode("utf-8", "replace").strip()
    if r.returncode != 0 or not osp.exists(out):
        return False, (err or msg or "转换器返回 %d" % r.returncode)
    return True, (msg or "ok")


def human(n):
    return "%.1f KB" % (n / 1024.0) if n >= 1024 else "%d B" % n


# ------------------------------------------------------------------ 界面
def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    root_dir = find_root()
    conv = find_converter(root_dir)

    win = tk.Tk()
    win.title(APP_TITLE)
    win.geometry("620x520")
    win.minsize(560, 460)
    try:
        ttk.Style().theme_use("vista")
    except Exception:
        pass

    pad = {"padx": 10, "pady": 6}

    # ---------- 要生成的字符 ----------
    f2 = ttk.LabelFrame(win, text="要生成的字符（只收中文，英文数字由回退字体提供）")
    f2.pack(fill="both", expand=True, **pad)

    txt = tk.Text(f2, height=8, wrap="word", relief="solid", bd=1,
                  font=("Microsoft YaHei", 12))
    txt.pack(fill="both", expand=True, padx=8, pady=(8, 4))

    if root_dir:
        txt.insert("1.0", read_font_list(root_dir))

    bar = ttk.Frame(f2)
    bar.pack(fill="x", padx=8, pady=(0, 8))
    cnt = ttk.Label(bar, text="", foreground="#2b7a4b")
    cnt.pack(side="right")

    def status(*_):
        s = clean_chars(txt.get("1.0", "end"))
        cnt.config(text="%d 字，约 %d 字节" % (len(s), len(s) * 26),
                   foreground="#2b7a4b" if s else "#b03")

    txt.bind("<KeyRelease>", status)

    def do_load():
        if not root_dir:
            messagebox.showerror(APP_TITLE, "找不到工程根目录（含 User/tasks.h）")
            return
        s = read_font_list(root_dir)
        txt.delete("1.0", "end")
        txt.insert("1.0", s)
        status()
        say("已读取 %s（%d 字）" % (FONT_LIST_REL, len(s)))

    def do_save():
        if not root_dir:
            messagebox.showerror(APP_TITLE, "找不到工程根目录")
            return
        s = clean_chars(txt.get("1.0", "end"))
        if not s:
            messagebox.showinfo(APP_TITLE, "字符框是空的")
            return
        write_font_list(root_dir, s)
        say("已写回 %s（%d 字）" % (FONT_LIST_REL, len(s)))

    ttk.Button(bar, text="读取 Font", command=do_load).pack(side="left")
    ttk.Button(bar, text="写回 Font", command=do_save).pack(side="left", padx=6)

    # ---------- 参数 ----------
    f3 = ttk.LabelFrame(win, text="参数")
    f3.pack(fill="x", **pad)

    p1 = ttk.Frame(f3)
    p1.pack(fill="x", padx=8, pady=8)

    ttk.Label(p1, text="字号").pack(side="left")
    size_var = tk.StringVar(value="12")
    ttk.Spinbox(p1, from_=8, to=48, width=5, textvariable=size_var).pack(side="left", padx=(4, 12))

    ttk.Label(p1, text="bpp").pack(side="left")
    bpp_var = tk.StringVar(value="1")
    ttk.Combobox(p1, values=["1", "2", "4", "8"], width=3, state="readonly",
                 textvariable=bpp_var).pack(side="left", padx=(4, 12))

    ttk.Label(p1, text="变量名").pack(side="left")
    name_var = tk.StringVar(value="lv_font_cn12")
    ttk.Entry(p1, textvariable=name_var, width=14).pack(side="left", padx=4)

    ttk.Label(p1, text="回退").pack(side="left", padx=(10, 0))
    fb_var = tk.StringVar(value="lv_font_montserrat_14")
    ttk.Entry(p1, textvariable=fb_var, width=20).pack(side="left", padx=4)

    p2 = ttk.Frame(f3)
    p2.pack(fill="x", padx=8, pady=(0, 8))

    ttk.Label(p2, text="输出").pack(side="left")
    out_var = tk.StringVar(
        value=osp.join(root_dir, FONT_OUT_REL) if root_dir else osp.join(base_dir(), "lv_font_cn12.c"))
    ttk.Entry(p2, textvariable=out_var).pack(side="left", fill="x", expand=True, padx=4)

    def pick_out():
        p = filedialog.asksaveasfilename(
            title="输出到", defaultextension=".c",
            initialdir=osp.dirname(out_var.get()) or ".",
            initialfile=osp.basename(out_var.get()),
            filetypes=[("C 文件", "*.c")])
        if p:
            out_var.set(osp.normpath(p))

    ttk.Button(p2, text="另存…", command=pick_out).pack(side="left")

    # ---------- 日志 ----------
    log = tk.Text(win, height=7, wrap="word", state="disabled",
                  bg="#fbfbfd", relief="solid", bd=1)
    log.pack(fill="both", expand=True, padx=10, pady=(4, 8))

    def say(s):
        log.config(state="normal")
        log.insert("end", s + "\n")
        log.see("end")
        log.config(state="disabled")

    # ---------- 生成 ----------
    f4 = ttk.Frame(win)
    f4.pack(fill="x", padx=10, pady=(0, 8))

    def do_gen():
        c = find_converter(root_dir)
        if not c:
            messagebox.showerror(APP_TITLE, "找不到 lv_font_conv.exe（应与本程序同在 tools/）")
            return

        chars = clean_chars(txt.get("1.0", "end"))
        if not chars:
            messagebox.showerror(APP_TITLE, "还没有字符")
            return

        try:
            size = int(size_var.get())
        except ValueError:
            messagebox.showerror(APP_TITLE, "字号要是整数")
            return

        ttf = osp.join(root_dir, FONT_TTF_REL) if root_dir else ""
        if not osp.exists(ttf):
            ttf = filedialog.askopenfilename(title="选字体文件",
                                             filetypes=[("字体", "*.ttf *.otf")])
            if not ttf:
                return

        out = out_var.get().strip()
        if not out.lower().endswith(".c"):
            out += ".c"
            out_var.set(out)
        d = osp.dirname(out)
        if d and not osp.isdir(d):
            os.makedirs(d, exist_ok=True)

        say("—" * 44)
        say("字符 %d 个，%dpx / %sbpp，回退 %s"
            % (len(chars), size, bpp_var.get(), fb_var.get().strip() or "无"))
        win.config(cursor="watch")
        win.update_idletasks()

        ok, msg = run_conv(c, ttf, chars, size, int(bpp_var.get()), out,
                           name_var.get().strip() or "lv_font_cn12", fb_var.get().strip())

        win.config(cursor="")
        if ok:
            sz = osp.getsize(out)
            say("成功: %s  (%s)" % (out, human(sz)))
            say("用法: LV_FONT_DECLARE(%s);" % (name_var.get().strip() or "lv_font_cn12"))
            messagebox.showinfo("完成", "已生成：\n%s\n\n%s，%d 个字符" % (out, human(sz), len(chars)))
        else:
            say("失败: " + msg)
            messagebox.showerror("失败", msg[:600])

    def open_dir():
        d = osp.dirname(out_var.get().strip())
        if d and osp.isdir(d):
            os.startfile(d)

    ttk.Button(f4, text="生 成", command=do_gen).pack(side="left")
    ttk.Button(f4, text="打开输出目录", command=open_dir).pack(side="left", padx=8)
    ttk.Label(f4, text=("转换器: " + (conv or "未找到")),
              foreground="#888" if conv else "#b03").pack(side="right")

    status()
    say("字符来源：%s" % FONT_LIST_REL)
    say("只收中文，英文数字由回退字体提供。改完点“生成”。")

    win.mainloop()


# ------------------------------------------------------------------ 自检
def run_selftest():
    base = base_dir()
    logp = osp.join(base, "font_selftest.log")
    root = find_root()
    conv = find_converter(root)
    ok = False
    lines = []
    if conv:
        out = osp.join(base, "_selftest_font.c")
        ttf = osp.join(root or "", FONT_TTF_REL)
        ok, msg = run_conv(conv, ttf, "南京信息职业技术学院", 12, 1, out,
                           "lv_font_selftest", "lv_font_montserrat_14")
        lines += ["conv=%s" % conv, "ok=%s" % ok, msg,
                  "size=%d" % (osp.getsize(out) if osp.exists(out) else 0)]
        if osp.exists(out):
            head = open(out, "rb").read(160).decode("utf-8", "replace").split("\n")[0]
            lines.append("head=%s" % head)
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
