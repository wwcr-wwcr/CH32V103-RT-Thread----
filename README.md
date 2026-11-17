# CH32V307VCT6 + RT-Thread + LVGL 基础工程

目录名沿用 CH32V103-RT-Thread，但实际芯片已经按用户确认改为 **CH32V307VCT6**。请勿将此固件烧录到 CH32V103。

## 本次移植范围

来源：桌面的 CH32V307-RT-Thread-1 工程。保留来源项目的 LVGL 8.3.11 源码、许可证、配置与原板显示/触摸驱动。基础层同步为 CH32V307 的 Core、Startup、Peripheral、Debug、RT-Thread、串口和 GPIO 驱动。

未移入 APP 外设应用、Runtime 任务管理、Tasks、菜单 UI。main 只打印启动信息后退出，LVGL 由 INIT_APP_EXPORT 自动启动，不需在 main 再调用初始化。

## 主要文件

- lvgl/lv_conf.h：颜色、控件、字体、LVGL 内存池配置。
- lvgl_port/lv_port_disp.c/.h：ST7796 FSMC 16 位并口，480×320 横屏，DMA2 通道2双缓冲，每块 480×10 像素。
- lvgl_port/lvgl_app.c：GUI 线程、GT911 软件 I2C、最小滑条演示、lcd_test/touch_test 串口命令。
- Ld/Link.ld：沿用原工程 256KB Flash、64KB RAM 分配；需与芯片实际存储分配保持一致。
- tools/build.py：不依赖 IDE 已生成 makefile 的全量编译脚本。

LVGL 池 22KB，显示双缓冲总计 19200 字节，RTT 堆 12KB；GUI 线程栈 3072 字节由 RTT 堆分配，不能重复计入静态 RAM。后续新增线程/控件须检查实际堆与栈余量。

## 打开与编译

工程内部名称已改为 CH32V307-RTT-LVGL。用 MounRiver 打开 CH32V307-RTT-LVGL.wvproj，或导入本目录 .project/.cproject，再执行 Build。旧 V103 工程已有独立备份，建议关闭旧工作区再打开新工程，避免 IDE 使用缓存配置。

命令行：在本目录执行 `python tools/build.py`。默认使用本机 D 盘 MounRiver 工具链；其他电脑可传入 `--toolchain 工具链bin目录`。

生成物：obj/CH32V307-RTT-LVGL.elf、.hex、.bin、.map。MRS 调试及下载路径也指向这些文件。没有自动烧录或执行擦除操作。

## 硬件验证

沿用原板接线，详细对照 lvgl_port/README.md。LCD 控制：PD11=CS、PD12=DC、PD5=WR、PD4=RD、PD13=RST、PC4=背光。触摸：PD2=INT、PD3=RST、PD7=SCL、PD6=SDA。

上电应看到版本文字和滑条。滑动验证触摸，串口查看 GT911 初始化日志。用 lcd_test 检查颜色和边界，用 touch_test 查看坐标。此次完成软件编译链接验证，显示、触摸和 DMA 运行需实板确认。

## 附带修复

修正继承的 RT-Thread list_device 类型名表缺少 Sensor 导致 Unknown 索引越界的问题。内核 object.c 在关闭 RT_DEBUG 时存在 obj 未使用警告，为原代码警告，未改变其行为。
