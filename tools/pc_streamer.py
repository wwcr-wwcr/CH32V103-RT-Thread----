#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
串口投屏 —— 电脑端（16 色 · 目标 1 秒 1 帧 · 不带声音）

把当前屏幕抓下来 → 缩到 320x460 → 量化成 16 色 → 每块 4bit 索引 + PackBits 压缩
→ 串口 → 板子 LCD 显示。

配套板子端：User/task/task_6.c（菜单里的 STREAM）+ drivers/uart_stream.c

为什么这么干：
    320x460 全屏 RGB565 = 300KB，921600bps 要 3.3 秒。
    4bit 索引直接砍到 73.6KB，PackBits 再压掉一半，整屏 0.3~0.5 秒。

依赖：
    pip install pyserial mss pillow

用法：
    # 不连串口、不抓屏，只跑编码器看压缩效果（推荐第一次先跑这个）
    python pc_streamer.py --dry-run

    # 正式投屏
    python pc_streamer.py --port COM5 --baud 921600

    # 板子是竖屏，想让电脑画面横着看就转 90 度
    python pc_streamer.py --port COM5 --rotate 90

注意：
    1. 先让板子进 STREAM 页面，再运行本脚本；
    2. 退出 STREAM 页面（K4）之前先关掉脚本，否则板子的 msh 会收到二进制乱码；
    3. 波特率必须和 task_6.c 里的 T6_BAUD 一致。
"""

import argparse
import struct
import sys
import time

# ----------------------------------------------------------------------------
# 协议（必须和 drivers/uart_stream.h 一致）
# ----------------------------------------------------------------------------
MAGIC0, MAGIC1 = 0xA5, 0x5A
T_VIDEO, T_CTRL = 0x01, 0x03

FMT_IDX4 = 0

CMD_CLEAR = 0
CMD_PALETTE = 1

TILE = 32
VIEW_W = 320
VIEW_H = 460          # 底部 20px 是板子的状态栏，不要覆盖
PAL_NUM = 16
PAYLOAD_MAX = 1024    # 板子端 USTREAM_PAYLOAD_MAX


# ----------------------------------------------------------------------------
# CRC
# ----------------------------------------------------------------------------
def _crc8_table():
    tbl = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
        tbl.append(c)
    return tbl


def _crc16_table():
    tbl = []
    for i in range(256):
        c = i << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
        tbl.append(c)
    return tbl


CRC8_TBL = _crc8_table()
CRC16_TBL = _crc16_table()


def crc8(data, crc=0):
    for b in data:
        crc = CRC8_TBL[(crc ^ b) & 0xFF]
    return crc


def crc16(data, crc=0xFFFF):
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ CRC16_TBL[((crc >> 8) ^ b) & 0xFF]
    return crc


def build_frame(ftype, payload):
    """帧头 6 字节 + 负载 + CRC16(2)，小端"""
    n = len(payload)
    head = bytes([ftype, n & 0xFF, (n >> 8) & 0xFF])
    return bytes([MAGIC0, MAGIC1]) + head + bytes([crc8(head)]) + payload + \
        struct.pack("<H", crc16(payload))


# ----------------------------------------------------------------------------
# 压缩
# ----------------------------------------------------------------------------
def pack_nibbles(idx):
    """每个像素 0~15，两个塞一个字节（高半字节在前）"""
    n = len(idx)
    out = bytearray((n + 1) // 2)
    for i in range(0, n - 1, 2):
        out[i >> 1] = (idx[i] << 4) | idx[i + 1]
    if n & 1:
        out[n >> 1] = idx[n - 1] << 4
    return bytes(out)


def packbits(data):
    """
    PackBits 压缩（板子端 unpack_bits() 的逆运算）
        控制字节 c:
            c < 0x80   后面跟 c+1 个原样字节（1~128）
            c >= 0x80  后面 1 个字节重复 (c&0x7F)+3 次（3~130）
    """
    out = bytearray()
    n = len(data)
    i = 0

    while i < n:
        # 先看这里有多少个连续相同字节
        j = i + 1
        while j < n and data[j] == data[i] and j - i < 130:
            j += 1

        if j - i >= 3:
            out.append(0x80 | (j - i - 3))
            out.append(data[i])
            i = j
            continue

        # 凑一段"原样"数据，直到遇到下一个 >=3 的重复
        lit = bytearray()
        while i < n and len(lit) < 128:
            if i + 2 < n and data[i] == data[i + 1] == data[i + 2]:
                break
            lit.append(data[i])
            i += 1
        out.append(len(lit) - 1)
        out += lit

    return bytes(out)


def pack_palette(img):
    """PIL 量化 -> (32 字节调色板负载, 每像素索引 bytes)"""
    from PIL import Image

    # 关掉抖动：抖动出来的噪点会把 PackBits 的压缩率毁掉，宁可要色块
    try:
        q = img.quantize(colors=PAL_NUM, method=Image.MEDIANCUT,
                         dither=Image.Dither.NONE)
    except AttributeError:
        q = img.quantize(colors=PAL_NUM, method=Image.MEDIANCUT,
                         dither=getattr(Image, "NONE", 0))
    pal = q.getpalette() or []
    pal = pal[:PAL_NUM * 3]

    body = bytearray()
    for i in range(PAL_NUM):
        r = pal[3 * i] if 3 * i < len(pal) else 0
        g = pal[3 * i + 1] if 3 * i + 1 < len(pal) else 0
        b = pal[3 * i + 2] if 3 * i + 2 < len(pal) else 0
        body += struct.pack("<H", ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

    return bytes(body), q.tobytes()


class Encoder:
    """整屏切成 32x32 块，逐块压"""

    def __init__(self):
        self.raw = 0
        self.packed = 0
        self.tiles = 0

    def encode_frame(self, idx):
        """返回 (整帧字节, 压缩率统计)"""
        out = bytearray()

        # 一帧 = 帧头 + 150 个块。板子端每块自己写自己的窗口，顺序任意。
        for ty in range(0, VIEW_H, TILE):
            for tx in range(0, VIEW_W, TILE):
                w = min(TILE, VIEW_W - tx)
                h = min(TILE, VIEW_H - ty)

                # 逐行切片：块在内存里不是连续的，一行一行抠
                block = b"".join(idx[(ty + r) * VIEW_W + tx:
                                     (ty + r) * VIEW_W + tx + w]
                                 for r in range(h))

                packed = pack_nibbles(block)
                comp = packbits(packed)

                self.raw += len(packed)
                self.packed += len(comp)
                self.tiles += 1

                out += build_frame(T_VIDEO,
                                   struct.pack("<HHBB", tx, ty, w, h) +
                                   bytes([FMT_IDX4]) + comp)

        return bytes(out)


# ----------------------------------------------------------------------------
# 串口
# ----------------------------------------------------------------------------
class Link:
    def __init__(self, port, baud, dry=False):
        self.dry = dry
        self.sent = 0
        self.ser = None
        if dry:
            return
        import serial
        self.ser = serial.Serial(port, baud, timeout=0, write_timeout=2.0)
        self.ser.reset_output_buffer()

    def write(self, data):
        """阻塞写：写不进去说明线路满了，等它自然消化，不在 Python 里堆"""
        self.sent += len(data)
        if self.dry:
            return
        try:
            self.ser.write(data)
        except Exception:
            try:
                self.ser.reset_output_buffer()
            except Exception:
                pass

    def close(self):
        if self.ser is not None:
            try:
                self.ser.flush()
                self.ser.close()
            except Exception:
                pass


# ----------------------------------------------------------------------------
# 抓屏
# ----------------------------------------------------------------------------
def fit_image(img, rotate, mode):
    from PIL import Image

    if rotate:
        img = img.rotate(rotate, expand=True)

    if mode == "stretch":
        return img.resize((VIEW_W, VIEW_H), Image.BILINEAR)

    sw, sh = img.size
    scale = min(VIEW_W / sw, VIEW_H / sh)
    nw, nh = max(1, int(sw * scale)), max(1, int(sh * scale))
    canvas = Image.new("RGB", (VIEW_W, VIEW_H), (0, 0, 0))
    canvas.paste(img.resize((nw, nh), Image.BILINEAR),
                 ((VIEW_W - nw) // 2, (VIEW_H - nh) // 2))
    return canvas


class ScreenGrabber:
    def __init__(self, monitor=1, rotate=0, mode="contain"):
        import mss
        self.rotate = rotate
        self.mode = mode
        self.sct = mss.mss()
        self.mon = self.sct.monitors[monitor]

    def grab(self):
        from PIL import Image
        shot = self.sct.grab(self.mon)
        img = Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")
        return fit_image(img, self.rotate, self.mode)

    def close(self):
        try:
            self.sct.close()
        except Exception:
            pass


class FakeGrabber:
    """--dry-run 用：一张会动的假桌面"""

    def __init__(self):
        from PIL import Image, ImageDraw
        self.Image = Image
        self.draw = ImageDraw
        self.t = 0

    def grab(self):
        img = self.Image.new("RGB", (VIEW_W, VIEW_H), (32, 34, 40))
        d = self.draw.Draw(img)
        # 顶栏
        d.rectangle([0, 0, VIEW_W, 28], fill=(60, 64, 74))
        # 一个窗口
        d.rectangle([30, 60, 290, 380], fill=(230, 230, 235), outline=(120, 120, 130))
        d.rectangle([30, 60, 290, 84], fill=(70, 110, 200))
        for i in range(12):
            d.line([48, 110 + i * 20, 272, 110 + i * 20], fill=(90, 90, 100), width=3)
        # 一个跑动的方块
        x = 20 + (self.t * 8) % 260
        d.rectangle([x, 400, x + 48, 448], fill=(220, 60, 60))
        self.t += 1

        img = fit_image(img, 0, "contain")
        return img

    def close(self):
        pass


# ----------------------------------------------------------------------------
# 主循环
# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="CH32V307 串口投屏（16 色，电脑端）")
    ap.add_argument("--port", default="COM5", help="串口名，如 COM5 或 /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=921600,
                    help="必须和 task_6.c 里的 T6_BAUD 一致")
    ap.add_argument("--fps", type=float, default=2.0,
                    help="每秒最多抓几次屏幕（线路能承受时才会真的到这个数）")
    ap.add_argument("--monitor", type=int, default=1, help="第几个显示器，1 = 主屏")
    ap.add_argument("--rotate", type=int, default=0, choices=[0, 90, 180, 270],
                    help="画面旋转角度，竖屏想横着看就填 90")
    ap.add_argument("--fit", default="contain", choices=["contain", "stretch"],
                    help="contain 保比例补黑边 / stretch 拉伸铺满")
    ap.add_argument("--dry-run", action="store_true",
                    help="不连串口、不抓屏，只跑编码器看压缩效果")
    args = ap.parse_args()

    # 1. 串口
    try:
        link = Link(args.port, args.baud, dry=args.dry_run)
    except Exception as e:
        sys.exit("打开串口失败：%s\n（串口名写错了？板子没插？）" % e)

    # 2. 画面
    if args.dry_run:
        grabber = FakeGrabber()
    else:
        try:
            grabber = ScreenGrabber(args.monitor, args.rotate, args.fit)
        except ImportError:
            sys.exit("缺依赖：pip install mss pillow")
        except Exception as e:
            sys.exit("抓屏失败：%s" % e)

    enc = Encoder()
    link.write(build_frame(T_CTRL, bytes([CMD_CLEAR])))

    period = 1.0 / max(0.2, args.fps)
    line_bps = args.baud / 10.0

    print("开跑：%d bps（约 %.0f KB/s），%dx%d，16 色 + PackBits，Ctrl+C 退出" %
          (args.baud, line_bps / 1024.0, VIEW_W, VIEW_H))

    stat_t = time.time()
    stat_frames = 0
    stat_bytes = 0
    last_pal = None

    try:
        while True:
            t0 = time.time()

            img = grabber.grab()
            pal, idx = pack_palette(img)

            head = bytearray()
            if pal != last_pal:
                head += build_frame(T_CTRL, bytes([CMD_PALETTE]) + pal)
                last_pal = pal

            frame = enc.encode_frame(idx)

            # 一帧发完再抓下一帧：线路自己会给节奏，不会在 PC 侧堆数据
            link.write(bytes(head) + frame)

            stat_frames += 1
            stat_bytes += len(head) + len(frame)

            now = time.time()
            if now - stat_t >= 1.0:
                el = now - stat_t
                ratio = (100.0 * enc.packed / enc.raw) if enc.raw else 0.0
                fps = stat_frames / el
                kbs = stat_bytes / el / 1024.0
                print("\r%4.1f fps  %5.1f KB/s / 线路 %.0f KB/s（占 %3.0f%%）"
                      "  压缩后占原始 %4.1f%%  一帧 %5.1f KB   " %
                      (fps, kbs, line_bps / 1024.0, 100.0 * kbs / (line_bps / 1024.0),
                       ratio, (stat_bytes / el / 1024.0) / max(fps, 1e-6)),
                      end="", flush=True)
                stat_t = now
                stat_frames = 0
                stat_bytes = 0
                enc.raw = enc.packed = enc.tiles = 0

            # 抓得太快就歇一下（真正限制速度的是串口）
            slack = period - (time.time() - t0)
            if slack > 0:
                time.sleep(slack)

    except KeyboardInterrupt:
        print("\n退出")
    finally:
        grabber.close()
        link.close()


if __name__ == "__main__":
    main()
