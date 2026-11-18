# -*- coding: utf-8 -*-
"""协议自测：把 PC 端编码器的输出，用板子端 C 代码的等价逻辑解回来，逐像素比对。
不需要硬件，也不需要 Pillow / numpy。

    python tools/selftest.py
"""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pc_streamer as P


# ---------- 板子端 frame_feed() 的 Python 等价实现 ----------
class BoardParser:
    def __init__(self):
        self.st = 0
        self.type = 0
        self.length = 0
        self.idx = 0
        self.hcrc = 0
        self.crc = 0
        self.crc_rx = 0
        self.payload = bytearray()
        self.frames = []
        self.bad = 0

    def feed(self, b):
        st = self.st
        if st == 0:
            if b == P.MAGIC0:
                self.st = 1
        elif st == 1:
            if b == P.MAGIC1:
                self.st = 2
            elif b != P.MAGIC0:
                self.st = 0
        elif st == 2:
            self.type = b
            self.hcrc = P.crc8(bytes([b]))
            self.st = 3
        elif st == 3:
            self.length = b
            self.hcrc = P.crc8(bytes([b]), self.hcrc)
            self.st = 4
        elif st == 4:
            self.length |= b << 8
            self.hcrc = P.crc8(bytes([b]), self.hcrc)
            self.st = 5
        elif st == 5:
            if b != self.hcrc or self.length == 0 or self.length > P.PAYLOAD_MAX:
                self.bad += 1
                self.st = 0
            else:
                self.idx = 0
                self.crc = 0xFFFF
                self.payload = bytearray()
                self.st = 6
        elif st == 6:
            self.payload.append(b)
            self.idx += 1
            self.crc = P.crc16(bytes([b]), self.crc)
            if self.idx >= self.length:
                self.st = 7
        elif st == 7:
            self.crc_rx = b
            self.st = 8
        elif st == 8:
            self.crc_rx |= b << 8
            self.st = 0
            if self.crc_rx != self.crc:
                self.bad += 1
            else:
                self.frames.append((self.type, bytes(self.payload)))

    def feed_bytes(self, data):
        for b in data:
            self.feed(b)


# ---------- 板子端 unpack_bits() 的 Python 等价实现 ----------
def unpack_bits(src, dst_len):
    out = bytearray()
    i = 0
    while i < len(src) and len(out) < dst_len:
        c = src[i]
        i += 1
        if c < 0x80:
            n = c + 1
            out += src[i:i + n]
            i += n
        else:
            n = (c & 0x7F) + 3
            out += bytes([src[i]]) * n
            i += 1
    return bytes(out[:dst_len])


def synth_desktop(shifting=True, t=0):
    """造一张"像桌面"的 16 色索引图：大片纯色 + 文字线 + 一个移动方块"""
    w, h = P.VIEW_W, P.VIEW_H
    img = bytearray([1]) * (w * h)                     # 背景

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1):
            s = y * w + x0
            img[s:s + (x1 - x0)] = bytes([c]) * (x1 - x0)

    rect(0, 0, w, 28, 3)                               # 顶栏
    rect(30, 60, 290, 384, 5)                          # 窗口
    rect(30, 60, 290, 84, 7)                           # 标题栏
    for i in range(12):                                # 文字行
        rect(48, 110 + i * 20, 272, 113 + i * 20, 2)
    for x in range(0, w, 40):                          # 竖线纹理
        rect(x, 400, x + 12, 440, 4)
    if shifting:
        x = 20 + (t * 8) % 260
        rect(x, 420, x + 48, 456, 9)                   # 移动的方块

    return bytes(img)


def main():
    random.seed(1)
    enc = P.Encoder()
    parser = BoardParser()

    # 1. 帧格式 + 压缩 + 解压 全链路
    frames_bytes = 0
    raw_bytes = 0
    for t in range(3):
        idx = synth_desktop(True, t)
        data = enc.encode_frame(idx)
        parser.feed_bytes(data)
        frames_bytes += len(data)
        raw_bytes += (P.VIEW_W * P.VIEW_H + 1) // 2

    assert parser.bad == 0, "有 %d 帧 CRC/长度错" % parser.bad
    n_tiles = (P.VIEW_W // P.TILE) * ((P.VIEW_H + P.TILE - 1) // P.TILE)
    assert len(parser.frames) == n_tiles * 3, \
        "帧数不对 %d != %d" % (len(parser.frames), n_tiles * 3)

    # 2. 逐块解回索引，拼成整帧，和原图比对
    for fi in range(3):
        canvas = bytearray(P.VIEW_W * P.VIEW_H)
        for k in range(n_tiles):
            ftype, pl = parser.frames[fi * n_tiles + k]
            assert ftype == P.T_VIDEO
            x = pl[0] | (pl[1] << 8)
            y = pl[2] | (pl[3] << 8)
            w, h, fmt = pl[4], pl[5], pl[6]
            assert fmt == P.FMT_IDX4
            idx = unpack_bits(pl[7:], (w * h + 1) // 2)
            # 4bit 半字节展开
            px = bytearray(w * h)
            for i in range(w * h):
                b = idx[i >> 1]
                px[i] = (b & 0x0F) if (i & 1) else (b >> 4)
            for r in range(h):
                s = (y + r) * P.VIEW_W + x
                canvas[s:s + w] = px[r * w:(r + 1) * w]
        assert bytes(canvas) == synth_desktop(True, fi), "第 %d 帧还原不一致" % fi

    per_frame = frames_bytes / 3
    print("3 帧 / %d 块：压缩后共 %d 字节，平均一帧 %.1f KB（4bit 原始 %.1f KB，压掉 %.0f%%）"
          % (n_tiles * 3, frames_bytes, per_frame / 1024.0, raw_bytes / 3 / 1024.0,
             100.0 * (1 - per_frame / (raw_bytes / 3))))
    print("  921600bps 下一帧约 %.2f 秒" % (per_frame * 10 / 921600))
    print("CRC 全对、像素逐点一致 —— 协议通过")

    # 3. PackBits 边界：随机数据（几乎压不动）和全 0（能压到极小）
    for name, data in (("随机", bytes(random.getrandbits(8) for _ in range(512))),
                       ("全 0", bytes(512)),
                       ("两字节游程", bytes([7, 7]) * 256)):
        comp = P.packbits(data)
        back = unpack_bits(comp, len(data))
        assert back == data, "%s 压缩/解压不一致" % name
        print("  %s 512 字节 -> %d 字节" % (name, len(comp)))

    # 4. 丢字节之后能不能重新同步
    parser2 = BoardParser()
    parser2.feed_bytes(b"\x00\x11\xA5\xA5\x5A\x03\x05\x00\xFF\x12\x34garbage")
    parser2.feed_bytes(enc.encode_frame(synth_desktop(True, 9)))
    assert len(parser2.frames) > 0, "丢字节后没能重新同步"
    print("  故意插垃圾字节后仍能同步，解出 %d 块" % len(parser2.frames))


if __name__ == "__main__":
    main()
