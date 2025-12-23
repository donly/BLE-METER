# 自行车BLE传感器显示屏 BLE METER

A bicycle BLE sensor display project based on ESP32 C3 for displaying Cycling Speed and Cadence (CSC) sensor data.

## 项目简介 / Project Description

本项目使用ESP32 C3开发板，通过BLE协议连接自行车的CSC传感器，实时显示速度和踏频数据。针对电池供电场景进行了低功耗优化。

This project uses ESP32 C3 development board to connect to bicycle CSC sensors via BLE protocol, displaying real-time speed and cadence data. It is optimized for low power consumption in battery-powered scenarios.

## 硬件要求

- **主控板**: ESP32 C3开发板（推荐ESP32-C3-SuperMini，体积小、功耗低，非常适合电池供电应用）
- **显示屏**: OLED显示屏（SSD1306，128x64或128x32，I2C接口）
- **电源**: 18650电池或聚合物锂电池（2000mAh+）
- **传感器**: 支持BLE CSC协议的自行车传感器

## 功能特性

- ✅ BLE CSC协议支持（Service UUID: 0x1816）
- ✅ 实时速度和踏频显示
- ✅ 低功耗管理（深度睡眠模式）
- ✅ 自动运动检测和唤醒
- ✅ OLED显示屏驱动
- ✅ 电池电量监控（可选）

## 项目结构

```
ble_meter/
├── README.md                 # 项目说明文档
├── ble_meter.ino            # 主程序入口
├── platformio.ini           # PlatformIO配置文件（可选）
├── libraries.txt            # 所需Arduino库列表
├── config.h                 # 硬件和功能配置
├── src/                     # 源代码目录
│   ├── BLEManager.h         # BLE连接管理
│   ├── BLEManager.cpp
│   ├── DisplayManager.h     # 显示管理
│   ├── DisplayManager.cpp
│   ├── PowerManager.h       # 功耗管理
│   ├── PowerManager.cpp
│   ├── CSCParser.h          # CSC数据解析
│   └── CSCParser.cpp
└── docs/                    # 文档目录
    ├── hardware_setup.md    # 硬件连接说明
    └── ble_csc_protocol.md  # BLE CSC协议格式文档
```

## 快速开始

### 1. 安装Arduino IDE

下载并安装 [Arduino IDE](https://www.arduino.cc/en/software)

### 2. 安装ESP32开发板支持

在Arduino IDE中：
1. 文件 → 首选项 → 附加开发板管理器网址
2. 添加：`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. 工具 → 开发板 → 开发板管理器 → 搜索"ESP32" → 安装

### 3. 安装所需库

根据 `libraries.txt` 中的列表安装所需库：
- ArduinoBLE（ESP32 BLE库）
- Adafruit SSD1306（OLED显示）
- Adafruit GFX（图形库）

### 4. 硬件连接

参考 `docs/hardware_setup.md` 进行硬件连接

### 5. 配置和上传

1. 打开 `ble_meter.ino`
2. 根据硬件配置修改 `config.h`
3. 选择开发板：工具 → 开发板 → ESP32 Arduino → ESP32C3 Dev Module（ESP32-C3-SuperMini使用相同配置）
4. 选择端口
5. 上传代码

## 配置说明

在 `config.h` 中可以配置：
- OLED显示屏尺寸和I2C地址
- BLE扫描间隔
- 深度睡眠唤醒条件
- 显示刷新频率

## 功耗优化

- 静止时自动进入深度睡眠（~5μA）
- 静止判断条件：
  - 速度 < 0.5 km/h
  - 静止时间 > 60秒（可在config.h中配置）
- 唤醒方式：
  - 定时唤醒（默认30秒后自动唤醒检查）
  - GPIO唤醒（可配置，如BOOT按钮）
- CPU频率动态调整
- BLE连接优化

## 开发计划

- [x] 项目结构搭建
- [ ] BLE CSC连接实现
- [ ] 数据解析和显示
- [ ] 低功耗管理
- [ ] 运动检测
- [ ] 测试和优化

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！

