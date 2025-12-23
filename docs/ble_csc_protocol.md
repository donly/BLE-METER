# BLE CSC (Cycling Speed and Cadence) 协议格式文档

## 概述

本文档描述了自行车BLE传感器（BT003-2）的CSC数据协议格式。该设备使用标准的BLE CSC Service (UUID: 0x1816)，但数据格式与标准规范略有不同。

## Service 和 Characteristic

- **Service UUID**: `0x1816` (Cycling Speed and Cadence)
- **Measurement Characteristic UUID**: `0x2A5B` (CSC Measurement)
- **Control Point Characteristic UUID**: `0x2A55` (CSC Control Point)

## 数据包格式

设备发送两种长度的数据包：

### 1. 5字节数据包（仅踏频数据）

**格式**：
```
字节0: 标志位 (Flags)
字节1-2: 轮转时间 (Wheel Event Time, uint16_t, little-endian)
字节3-4: 曲柄转数 (Crank Revolutions, uint16_t, little-endian)
```

**示例**：
```
02 9C 01 4E D2
```

**解析**：
- 标志位: `0x02` = `00000010` (bit 1 = 轮转时间存在)
- 轮转时间: `0x9C 0x01` = `0x019C` = 412 (1/1024秒)
- 曲柄转数: `0x4E 0xD2` = `0xD24E` = 53838

**说明**：
- 此数据包只包含曲柄转数，不包含曲柄时间
- 无法计算踏频（需要转数+时间）
- 无法计算速度（没有轮转数）

### 2. 11字节数据包（完整数据）

**格式**：
```
字节0: 标志位 (Flags)
字节1-4: 轮转数 (Wheel Revolutions, uint32_t, little-endian)
字节5-6: 轮转时间 (Wheel Event Time, uint16_t, little-endian)
字节7-8: 曲柄转数 (Crank Revolutions, uint16_t, little-endian)
字节9-10: 曲柄时间 (Crank Event Time, uint16_t, little-endian)
```

**示例**：
```
03 73 02 00 00 4C D1 BE 01 90 E4
```

**解析**：
- 标志位: `0x03` = `00000011` (bit 0 = 轮转数存在, bit 1 = 轮转时间存在)
- 轮转数: `0x73 0x02 0x00 0x00` = `0x00000273` = 627
- 轮转时间: `0x4C 0xD1` = `0xD14C` = 53580 (1/1024秒)
- 曲柄转数: `0xBE 0x01` = `0x01BE` = 446
- 曲柄时间: `0x90 0xE4` = `0xE490` = 58512 (1/1024秒)

**说明**：
- 此数据包包含完整的速度和踏频数据
- 可以计算速度（轮转数 + 轮转时间）
- 可以计算踏频（曲柄转数 + 曲柄时间）

## 标志位 (Flags) 定义

| Bit | 名称 | 说明 |
|-----|------|------|
| 0 | Wheel Revolution Data Present | 轮转数存在 |
| 1 | Crank Revolution Data Present | 轮转时间存在 |
| 2 | Crank Revolutions Present | 曲柄转数存在（但设备不设置此标志） |
| 3 | Crank Event Time Present | 曲柄时间存在（但设备不设置此标志） |

**注意**：虽然设备在11字节数据包中包含曲柄数据，但标志位的bit 2和bit 3不会被设置。需要在解析时根据数据包长度和剩余字节来判断。

## 数据字段说明

### 轮转数 (Wheel Revolutions)
- **类型**: `uint32_t` (4字节)
- **字节序**: Little-endian
- **说明**: 累积轮转数，从0开始递增
- **用途**: 与轮转时间一起计算速度

### 轮转时间 (Wheel Event Time)
- **类型**: `uint16_t` (2字节)
- **字节序**: Little-endian
- **单位**: 1/1024秒
- **范围**: 0-65535 (会溢出回0)
- **说明**: 上次轮转事件的时间戳
- **用途**: 与轮转数一起计算速度

### 曲柄转数 (Crank Revolutions)
- **类型**: `uint16_t` (2字节)
- **字节序**: Little-endian
- **说明**: 累积曲柄转数，从0开始递增（会溢出）
- **用途**: 与曲柄时间一起计算踏频

### 曲柄时间 (Crank Event Time)
- **类型**: `uint16_t` (2字节)
- **字节序**: Little-endian
- **单位**: 1/1024秒
- **范围**: 0-65535 (会溢出回0)
- **说明**: 上次曲柄事件的时间戳
- **用途**: 与曲柄转数一起计算踏频

## 计算方法

### 速度计算

```
时间差 = (当前轮转时间 - 上次轮转时间) / 1024.0 秒
转数差 = 当前轮转数 - 上次轮转数
距离 = 转数差 × 轮周长 (mm) / 1000000.0  km
速度 = (距离 / 时间差) × 3600.0  km/h
```

**注意**：需要处理时间溢出（65535 → 0）

### 踏频计算

```
时间差 = (当前曲柄时间 - 上次曲柄时间) / 1024.0 秒
转数差 = 当前曲柄转数 - 上次曲柄转数
踏频 = (转数差 / 时间差) × 60.0  rpm
```

**注意**：
- 需要处理时间溢出（65535 → 0）
- 需要处理转数溢出（65535 → 0）
- 第一次数据无法计算（需要两次数据的时间差）

## 数据包发送规律

根据测试观察：

1. **5字节数据包**：
   - 当只有踏频传感器触发时发送
   - 包含轮转时间和曲柄转数
   - 无法计算速度和踏频

2. **11字节数据包**：
   - 当速度传感器触发时发送
   - 包含完整的速度和踏频数据
   - 可以同时计算速度和踏频

3. **数据更新频率**：
   - 速度传感器：每次轮转触发
   - 踏频传感器：每次曲柄转触发
   - 数据包发送频率取决于传感器触发频率

## 解析建议

1. **检查数据包长度**：
   - 5字节：只有部分数据
   - 11字节：完整数据

2. **解析顺序**：
   - 先解析标志位
   - 根据标志位解析轮转数据
   - 检查剩余字节，解析曲柄数据（即使标志位未设置）

3. **数据验证**：
   - 检查时间值是否在合理范围内（0-65535）
   - 检查转数是否递增（考虑溢出）
   - 第一次数据无法计算速度/踏频，需要保存等待下次数据

## 参考

- [Bluetooth SIG CSC Service Specification](https://www.bluetooth.com/specifications/specs/cycling-speed-and-cadence-service-1-0/)
- [GitHub: BLE CSC Decode Examples](https://github.com/av1d/BLE-Cycling-Speed-and-Cadence-Service-examples-decode-data)

