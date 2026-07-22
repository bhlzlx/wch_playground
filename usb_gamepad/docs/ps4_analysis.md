# GP2040-CE PS4手柄驱动分析文档

## 概述

GP2040-CE 是一个基于 RP2040 (双核 Cortex-M0+) 的开源游戏手柄固件，支持多种主机平台（PS3/PS4/PS5/Switch/Xbox等）。本文档分析其 PS4 驱动实现，为在 CH582M 上复刻提供参考。

## 一、整体架构

### 1.1 文件结构

```
src/drivers/ps4/
├── PS4Driver.cpp           # PS4 主驱动 - HID报告处理、描述符管理
├── PS4Auth.cpp             # PS4 认证 - RSA签名、密钥管理
└── PS4AuthUSBListener.cpp  # PS4 USB Dongle认证监听器

headers/drivers/ps4/
├── PS4Driver.h             # PS4Driver 类声明
├── PS4Descriptors.h        # USB描述符、数据结构定义
├── PS4Auth.h               # PS4Auth 类声明
└── PS4AuthUSBListener.h    # PS4AuthUSBListener 类声明
```

### 1.2 核心类层次

```
GPDriver (基类 - gpdriver.h)
  └── PS4Driver            # PS4主驱动

GPAuthDriver (基类 - gpauthdriver.h)
  └── PS4Auth              # PS4认证驱动

USBListener (基类 - usblistener.h)
  └── PS4AuthUSBListener   # USB Dongle认证监听
```

## 二、USB描述符体系

### 2.1 设备描述符 (Device Descriptor)

```c
// 使用 Razer Panthera 或 DS4 的 VID/PID
#define PS4_VENDOR_ID   0x1532  // Razer Panthera
#define PS4_PRODUCT_ID  0x0401
#define DS4_VENDOR_ID   0x054C  // 原装DS4
#define DS4_PRODUCT_ID  0x09CC
```

设备描述符是标准USB 2.0格式，`bMaxPacketSize0=64`, `bNumConfigurations=1`。

### 2.2 配置描述符 (Configuration Descriptor)

```
配置描述符 (9B)
  └── 接口描述符 (9B)              # bInterfaceClass=0x03 (HID)
      ├── HID描述符 (9B)            # bcdHID=1.11
      ├── IN端点描述符 (7B)          # EP1 IN, 中断传输, 64B, 1ms轮询
      └── OUT端点描述符 (7B)        # EP2 OUT, 中断传输, 64B, 1ms轮询
总大小: 41 字节 (PS4_CONFIG1_DESC_SIZE)
```

### 2.3 HID报告描述符 (关键)

PS4的HID报告描述符结构：

```
Usage Page (Generic Desktop)
Usage (Game Pad)
Collection (Application)
  Report ID 1 (输入报告 - 手柄状态)
    ├── X, Y, Z, Rz       (4×8bit 摇杆)
    ├── Hat switch         (4bit 方向键, 0-7, 15=中位)
    ├── Buttons 1-14       (14bit 按钮)
    ├── Report Counter     (6bit 计数器, Usage 0xFF00:0x20)
    ├── Rx, Ry             (2×8bit 扳机)
    └── Vendor Data        (54B 扩展数据, Usage 0xFF00:0x21)
  
  Report ID 5 (输出报告 - 手柄功能控制, 31B)
    Usage (0xFF00:0x22)
  
  Report ID 3 (Feature报告 - 控制器定义, 47B)
    Usage (0x2721)
  
  Report ID 2 (Feature报告 - 校准数据, 36B)
  
  // ... 多个额外 Feature 报告 (0x08-0x15, 0x80-0x90)
  
  // 认证相关 Feature 报告 (Usage Page 0xFFF0)
  Report ID 0xF0  (认证载荷设置, 63B)
  Report ID 0xF1  (签名Nonce获取, 63B)
  Report ID 0xF2  (签名状态查询, 15B)
  Report ID 0xF3  (认证重置, 7B)
End Collection
```

## 三、输入报告结构 (PS4Report)

总大小: 64字节 (1个USB HID包)

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | reportID | 固定为 0x01 |
| 1 | 1 | leftStickX | 左摇杆X (0x00-0xFF, 中位0x80) |
| 2 | 1 | leftStickY | 左摇杆Y |
| 3 | 1 | rightStickX | 右摇杆X |
| 4 | 1 | rightStickY | 右摇杆Y |
| 5 | 4bit | dpad | 方向键 (0=N,1=NE,2=E,3=SE,4=S,5=SW,6=W,7=NW,0xF=中位) |
| 5-6 | 14bit | buttons | 14个按钮位 |
| 6 | 6bit | reportCounter | 报告计数器 (0-63, 每次报告递增) |
| 7 | 1 | leftTrigger | 左扳机 (0-0xFF) |
| 8 | 1 | rightTrigger | 右扳机 (0-0xFF) |
| 9-62 | 54 | miscData | 扩展数据 (根据设备类型不同) |

### 按钮位映射 (Little-Endian 位序)

| 位 | 按钮 | PS4对应 |
|----|------|---------|
| 0 | buttonWest | □ Square |
| 1 | buttonSouth | × Cross |
| 2 | buttonEast | ○ Circle |
| 3 | buttonNorth | △ Triangle |
| 4 | buttonL1 | L1 |
| 5 | buttonR1 | R1 |
| 6 | buttonL2 | L2 |
| 7 | buttonR2 | R2 |
| 8 | buttonSelect | Share |
| 9 | buttonStart | Options |
| 10 | buttonL3 | L3 |
| 11 | buttonR3 | R3 |
| 12 | buttonHome | PS按钮 |
| 13 | buttonTouchpad | 触摸板按下 |

### 扩展数据 (Gamepad模式, 54字节)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0-1 | 2 | axisTiming (16bit时间戳) |
| 2-3 | 2 | battery (电池) |
| 4-5 | 2 | gyroscope.x |
| 6-7 | 2 | gyroscope.y |
| 8-9 | 2 | gyroscope.z |
| 10-11 | 2 | accelerometer.x |
| 12-13 | 2 | accelerometer.y |
| 14-15 | 2 | accelerometer.z |
| 16-19 | 4 | misc (传感器杂项) |
| 20 | 1 | 电池/充电状态标志 |
| 21 | 1 | misc2 |
| 22 | 1 | touchpadActive + padding (6bit) |
| 23 | 1 | tpadIncrement |
| 24-30 | 7 | touchpadData (2个触摸点) |
| 31-51 | 21 | mystery2 (填充) |

### 触摸点数据 (TouchpadXY, 4字节)

| 偏移 | 位 | 字段 |
|------|-----|------|
| 0 | 0-6 | counter (触摸计数器 0-127) |
| 0 | 7 | unpressed (0=按下, 1=未按下) |
| 1-3 | - | X(12bit) + Y(12bit) 交织存储 |

触摸板分辨率: 1920×943

## 四、输出报告 (PS4FeatureOutputReport, 32字节)

PS4主机通过 OUT 端点 (Report ID 0x05) 发送给手柄：

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 1 | reportID (0x05) |
| 1 | 1 | 功能更新标志 (震动/LED/音量等) |
| 2 | 1 | 保留 |
| 3 | 1 | unknown0 |
| 4 | 1 | rumbleRight (右马达 0-0xFF) |
| 5 | 1 | rumbleLeft (左马达 0-0xFF) |
| 6 | 1 | ledRed |
| 7 | 1 | ledGreen |
| 8 | 1 | ledBlue |
| 9 | 1 | ledBlinkOn (闪烁时间, 10ms单位) |
| 10 | 1 | ledBlinkOff |
| 11-18 | 8 | extData (扩展数据) |
| 19 | 1 | volumeLeft |
| 20 | 1 | volumeRight |
| 21 | 1 | volumeMic |
| 22 | 1 | volumeSpeaker |
| 23 | 1 | unknownAudio |
| 24-31 | 8 | padding |

## 五、控制器定义 (PS4ControllerConfig, 48字节)

通过 Feature Report 0x03 读写：

| 偏移 | 大小 | 字段 | 默认值 |
|------|------|------|--------|
| 0-1 | 2 | hidUsage | 0x2721 |
| 2 | 1 | mystery0 | 0x04 |
| 3 | 1 | features (位域) | 0xEF (全部启用) |
| 4 | 1 | controllerType | 见 PS4ControllerType |
| 5-6 | 2 | touchpadParam | {0x2C, 0x56} |
| 7-16 | 10 | imuConfig (陀螺仪/加速度计参数) | |
| 17-18 | 2 | magicID | 0x0D0D |
| 19-22 | 4 | mystery1 | {0} |
| 23-25 | 3 | wheelParam | {0x0D, 0x84, 0x03} |
| 26-47 | 21 | mystery2 | {0} |

### controllerType 值

| 值 | 类型 |
|----|------|
| 0 | 无 |
| 1 | PS4_CONTROLLER (DS4标准手柄) |
| 2 | PS4_HOTAS (飞行摇杆) |
| 3 | PS4_WHEEL (方向盘) |
| 4 | PS4_GAMEPAD (普通手柄) |
| 5 | PS4_DRUMS (鼓) |
| 6 | PS4_GUITAR (吉他) |
| 7 | PS4_ARCADESTICK (街机摇杆/PS5兼容模式) |

### features 位域

| 位 | 名称 | 描述 |
|----|------|------|
| 0 | enableController | 启用手柄功能 |
| 1 | enableMotion | 启用运动传感器 |
| 2 | enableLED | 启用LED控制 |
| 3 | enableRumble | 启用震动 |
| 4 | enableAnalog | 启用模拟输入 |
| 5 | enableUnknown0 | 未知功能0 |
| 6 | enableTouchpad | 启用触摸板 |
| 7 | enableUnknown1 | 未知功能1 |

## 六、PS4认证协议

### 6.1 认证流程概览

PS4/PS5 主机要求手柄通过RSA-2048签名认证。GP2040-CE 支持两种认证方式：

1. **密钥模式 (INPUT_MODE_AUTH_TYPE_KEYS)**: 使用提取的PS4密钥进行本地RSA签名
2. **USB透传模式 (INPUT_MODE_AUTH_TYPE_USB)**: 通过USB Host连接正版PS4手柄进行认证透传

### 6.2 密钥模式流程

```
┌──────────┐                    ┌──────────┐
│ PS4 主机  │                    │  手柄     │
└────┬─────┘                    └────┬─────┘
     │                               │
     │  SET_REPORT 0xF0 (nonce数据)   │
     │  ──────────────────────────▶  │
     │  256字节nonce, 分5页发送       │
     │  (4×56B + 1×32B)             │
     │                               │
     │  处理nonce:                   │
     │  ① SHA256(nonce) → hash      │
     │  ② RSA_PSS_Sign(hash) → sig  │
     │  ③ 组装响应:                  │
     │     [签名256B][序列号16B]      │
     │     [RSA_N 256B][RSA_E 256B] │
     │     [PS4签名256B][填充24B]    │
     │  共1064字节                   │
     │                               │
     │  GET_REPORT 0xF2 (查询状态)    │
     │  ◀────────────────────────── │
     │  回复: 0=就绪, 16=处理中      │
     │                               │
     │  GET_REPORT 0xF1 (获取签名)    │
     │  ◀────────────────────────── │
     │  分19块返回, 每块56字节       │
     │  (19×56 = 1064字节)          │
     │                               │
     │  GET_REPORT 0xF3 (认证重置)    │
     │  ◀────────────────────────── │
     │                               │
```

### 6.3 认证数据结构 (1064字节)

| 偏移 | 大小 | 内容 |
|------|------|------|
| 0 | 256 | RSA签名 (对nonce的PSS签名) |
| 256 | 16 | 手柄序列号 |
| 272 | 256 | RSA公钥 N (模数) |
| 528 | 256 | RSA公钥 E (指数) |
| 784 | 256 | PS4原始签名 (固件签名) |
| 1040 | 24 | 零填充 |

### 6.4 USB透传模式

当使用USB Dongle认证时：
- 设备作为USB Host连接正版PS4手柄
- 使用TinyUSB Host栈 (tuh_hid_get_report/set_report)
- 将PS4主机发来的nonce转发给正版手柄
- 将正版手柄的签名转发回PS4主机

## 七、驱动工作流程

### 7.1 初始化 (initialize)

1. 根据配置选择 VID/PID (Razer Panthera / DS4 / Wheel)
2. 根据 deviceType 设置 controllerType 和功能标志位
3. 初始化各设备类型的按钮映射
4. 初始化 HID 报告 (摇杆中位, 方向键中位)
5. 初始化 controllerConfig (功能描述符)
6. 设置 TinyUSB class_driver 回调

### 7.2 认证初始化 (initializeAux)

1. 根据 controllerType 选择 PS4 或 PS5 认证类型
2. 创建 PS4Auth 驱动实例
3. 如果认证可用，初始化并获取认证数据缓冲区

### 7.3 主循环处理 (process)

每帧调用一次：
1. 读取 GPIO 按钮状态
2. 根据 deviceType 填充不同的报告结构
3. 映射 D-Pad
4. 映射摇杆和扳机
5. 处理触摸板数据
6. 处理传感器数据
7. 如果报告有变化，通过 tud_hid_report() 发送
8. 如果无变化但超过保活时间(5ms)，更新计数器触发下次发送
9. 处理输出报告 (震动/LED)

### 7.4 Feature Report 处理

- `get_report()`: 处理主机对 Feature Report 的读取请求
  - 0x02: 返回校准数据
  - 0x03: 返回控制器定义
  - 0x12: 返回MAC地址
  - 0xA3: 返回固件版本
  - 0xF1: 返回签名数据块 (19块×56B)
  - 0xF2: 返回签名状态
  - 0xF3: 重置认证

- `set_report()`: 处理主机对 Feature/Output Report 的写入
  - 0x05 (Output): 接收震动/LED控制数据
  - 0xF0 (Feature): 接收认证nonce数据

### 7.5 保活机制

- 仅在报告内容变化时发送 (减少USB带宽)
- 5ms无变化时递增 reportCounter 强制触发下次发送
- reportCounter 6位宽 (0-63循环)

## 八、CH582M 移植要点

### 8.1 平台差异

| 特性 | RP2040 | CH582M |
|------|--------|--------|
| CPU | 双核 Cortex-M0+ @133MHz | 单核 RISC-V (WCH RISC-V3A) @60MHz |
| RAM | 264KB | 32KB |
| Flash | 2MB (外部) | 256KB (内置) |
| USB | TinyUSB (开源) | WCH 专有 USB 设备库 |
| USB端点 | 灵活的EP配置 | 固定8通道 (4端点双向) |
| BLE | 无 | 内置 BLE 5.0 |
| 开发环境 | Pico SDK + GCC | WCH MounRiver Studio + GCC |

### 8.2 简化策略

考虑到 CH582M 的资源限制 (32KB RAM) 和单核架构：

1. **简化设备类型**: 先只实现 Gamepad 模式 (最常用)
2. **简化认证**: 
   - 可选: 移植 mbedtls 进行本地RSA签名 (但mbedtls RSA需要较多RAM)
   - 推荐: 实现USB透传认证 (利用CH582M的USB Host功能)
   - 最低: 使用街机摇杆模式 (PS4_ARCADESTICK)，某些场景可能免认证
3. **移除触摸板**: 街机摇杆用不到触摸板
4. **简化传感器**: 固定返回零值
5. **简化按钮映射**: 固定GPIO到PS4按钮的映射

### 8.3 USB端点映射

CH582M 的 USB 设备控制器：
- EP0: 控制传输 (默认)
- EP1: IN + OUT (双向) → 用于HID输入报告
- EP2: IN + OUT (双向) → 用于HID输出/Feature报告
- EP3-4: 可选的额外端点

建议映射:
- EP1 IN: HID Input Report (手柄状态, 64B)
- EP2 OUT: HID Output Report (震动/LED, 32B)
- EP0: 控制传输 (Feature Reports, 描述符请求)

### 8.4 内存预算 (CH582M)

- PS4Report: 64B
- PS4FeatureOutputReport: 32B  
- PS4ControllerConfig: 48B
- 认证缓冲区 (可选): 1064B (如果不做认证可省略)
- USB端点缓冲区: 4×64B = 256B
- 其他: ~8KB

总计: 约1.5KB (不含认证) 或 2.5KB (含认证)
RAM充足，CH582M的32KB完全够用。

### 8.5 关键常量汇总

```
PS4_HAT_NOTHING     = 0x0F
PS4_HAT_UP          = 0x00
PS4_HAT_RIGHT       = 0x02
PS4_HAT_DOWN        = 0x04
PS4_HAT_LEFT        = 0x06
PS4_JOYSTICK_MIN    = 0x00
PS4_JOYSTICK_MID    = 0x80
PS4_JOYSTICK_MAX    = 0xFF
PS4_TP_X_MAX        = 1920
PS4_TP_Y_MAX        = 943
ENDPOINT0_SIZE      = 64
PS4_ENDPOINT_SIZE   = 64
PS4_FEATURES_SIZE   = 32
PS4_KEEPALIVE_TIMER = 5 (ms)
PS4_MAX_GEARS       = 8
```
