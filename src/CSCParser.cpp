/**
 * CSC数据解析类实现
 * 
 * 设备数据格式（根据实际测试确认）:
 * 
 * 5字节数据包（仅踏频数据）:
 *   字节0: 标志位 (0x02 = 只有轮转时间)
 *   字节1-2: 轮转时间 (uint16_t, little-endian)
 *   字节3-4: 曲柄转数 (uint16_t, little-endian, 无曲柄时间)
 * 
 * 11字节数据包（完整数据）:
 *   字节0: 标志位 (0x03 = 轮转数+轮转时间)
 *   字节1-4: 轮转数 (uint32_t, little-endian)
 *   字节5-6: 轮转时间 (uint16_t, little-endian)
 *   字节7-8: 曲柄转数 (uint16_t, little-endian)
 *   字节9-10: 曲柄时间 (uint16_t, little-endian)
 * 
 * 注意: 设备不设置曲柄数据的标志位(bit 2,3)，需要根据数据包长度判断
 * 详细文档请参考: docs/ble_csc_protocol.md
 */

#include "CSCParser.h"
#include <Arduino.h>

// 前向声明结构体（在ble_meter.ino中定义）
struct SensorData {
  float speed;
  float cadence;
  uint32_t wheelRevolutions;
  uint16_t lastWheelEventTime;
  uint16_t crankRevolutions;
  uint16_t lastCrankEventTime;
};

CSCParser::CSCParser() {
  reset();
}

void CSCParser::reset() {
  lastWheelRevolutions = 0;
  lastWheelEventTime = 0;
  lastCrankRevolutions = 0;
  lastCrankEventTime = 0;
}

void CSCParser::parseData(uint8_t* data, size_t length, SensorData& sensorData) {
  if (data == nullptr || length < 1) {
    return;
  }
  
  if (DEBUG_MODE) {
    Serial.printf("[解析] 数据总长度: %d 字节\n", length);
  }
  
  // 首先需要知道数据长度，但CSC数据格式是变长的
  // 根据BLE规范，最大13字节，但实际长度取决于标志位
  // 先假设数据足够长，根据标志位解析
  
  uint8_t flags = data[0];
  Serial.printf("[解析] 标志位: 0x%02X", flags);
  if (DEBUG_MODE) {
    Serial.printf("\n  - 轮转数存在 (bit 0): %s", (flags & 0x01) ? "是" : "否");
    Serial.printf("\n  - 轮转时间存在 (bit 1): %s", (flags & 0x02) ? "是" : "否");
    Serial.printf("\n  - 曲柄转数存在 (bit 2): %s", (flags & 0x04) ? "是" : "否");
    Serial.printf("\n  - 曲柄时间存在 (bit 3): %s", (flags & 0x08) ? "是" : "否");
  }
  Serial.println();
  
  int offset = 1;
  
  // 注意：根据BLE CSC规范，某些设备可能只发送部分数据
  // 如果只有轮转时间而没有轮转数，无法计算速度
  // 如果只有曲柄转数或时间之一，无法计算踏频
  
  // 解析轮转数据
  // 根据BLE规范，轮转数（如果存在）是32位，轮转时间（如果存在）是16位
  if (flags & 0x01) {  // 轮转数存在
    if (offset + 4 > length) {
      Serial.println("[解析] 错误: 数据长度不足，无法读取轮转数");
      return;
    }
    // 轮转数是uint32_t (4字节)
    uint32_t wheelRevolutions = 
      (uint32_t)data[offset] |
      ((uint32_t)data[offset + 1] << 8) |
      ((uint32_t)data[offset + 2] << 16) |
      ((uint32_t)data[offset + 3] << 24);
    offset += 4;
    sensorData.wheelRevolutions = wheelRevolutions;
    Serial.printf("[解析] 轮转数: %lu\n", wheelRevolutions);
    
    if (flags & 0x02) {  // 轮转时间存在
      if (offset + 2 > length) {
        Serial.println("[解析] 错误: 数据长度不足，无法读取轮转时间");
        return;
      }
      // 轮转时间是uint16_t (2字节)
      uint16_t wheelEventTime = 
        (uint16_t)data[offset] |
        ((uint16_t)data[offset + 1] << 8);
      offset += 2;
      
      // 计算速度（需要轮转数和时间）
      sensorData.speed = calculateSpeed(wheelRevolutions, wheelEventTime);
      sensorData.lastWheelEventTime = wheelEventTime;
      Serial.printf("[解析] 轮转时间: %u (1/1024秒), 速度: %.2f km/h\n", wheelEventTime, sensorData.speed);
    } else {
      // 只有轮转数，没有时间，无法计算速度
      Serial.println("[解析] 警告: 有轮转数但无时间，无法计算速度");
    }
  } else if (flags & 0x02) {
    // 只有轮转时间，没有轮转数（非标准，但某些设备可能这样）
    if (offset + 2 > length) {
      Serial.println("[解析] 错误: 数据长度不足，无法读取轮转时间");
      return;
    }
    uint16_t wheelEventTime = 
      (uint16_t)data[offset] |
      ((uint16_t)data[offset + 1] << 8);
    offset += 2;
    sensorData.lastWheelEventTime = wheelEventTime;
    Serial.printf("[解析] 仅轮转时间: %u (1/1024秒)，无轮转数，无法计算速度\n", wheelEventTime);
  }
  
  // 解析曲柄数据
  // 根据BLE规范，曲柄转数是uint16_t (2字节)，曲柄时间也是uint16_t (2字节)
  // 参考: https://github.com/av1d/BLE-Cycling-Speed-and-Cadence-Service-examples-decode-data
  if (flags & 0x04) {  // 曲柄转数存在
    if (offset + 2 > length) {
      Serial.println("[解析] 错误: 数据长度不足，无法读取曲柄转数");
      return;
    }
    uint16_t crankRevolutions = 
      (uint16_t)data[offset] |
      ((uint16_t)data[offset + 1] << 8);
    offset += 2;
    sensorData.crankRevolutions = crankRevolutions;
    Serial.printf("[解析] 曲柄转数: %u\n", crankRevolutions);
    
    if (flags & 0x08) {  // 曲柄时间存在
      if (offset + 2 > length) {
        Serial.println("[解析] 错误: 数据长度不足，无法读取曲柄时间");
        return;
      }
      uint16_t crankEventTime = 
        (uint16_t)data[offset] |
        ((uint16_t)data[offset + 1] << 8);
      offset += 2;
      
      // 计算踏频（需要转数和时间）
      sensorData.cadence = calculateCadence(crankRevolutions, crankEventTime);
      sensorData.lastCrankEventTime = crankEventTime;
      Serial.printf("[解析] 曲柄时间: %u (1/1024秒), 踏频: %.1f rpm\n", crankEventTime, sensorData.cadence);
    } else {
      // 只有曲柄转数，没有时间，无法计算踏频
      Serial.println("[解析] 警告: 有曲柄转数但无时间，无法计算踏频");
    }
  } else if (flags & 0x08) {
    // 只有曲柄时间，没有转数（非标准）
    if (offset + 2 > length) {
      Serial.println("[解析] 错误: 数据长度不足，无法读取曲柄时间");
      return;
    }
    uint16_t crankEventTime = 
      (uint16_t)data[offset] |
      ((uint16_t)data[offset + 1] << 8);
    offset += 2;
    sensorData.lastCrankEventTime = crankEventTime;
    Serial.printf("[解析] 仅曲柄时间: %u (1/1024秒)，无转数，无法计算踏频\n", crankEventTime);
  } else {
    // 没有曲柄数据标志，但设备实际会在数据包中包含曲柄信息
    // 根据测试确认的数据格式：
    // - 5字节数据包: 标志位0x02，只有轮转时间 + 曲柄转数（无曲柄时间）
    // - 11字节数据包: 标志位0x03，轮转数+时间 + 曲柄转数+时间（完整数据）
    
    // 如果还有2字节数据，是曲柄转数
    if (offset + 2 <= length) {
      uint16_t crankRevolutions = 
        (uint16_t)data[offset] |
        ((uint16_t)data[offset + 1] << 8);
      
      // 如果还有额外2字节，是曲柄时间（11字节数据包格式）
      if (offset + 4 <= length) {
        uint16_t crankEventTime = 
          (uint16_t)data[offset + 2] |
          ((uint16_t)data[offset + 3] << 8);
        
        // 检查时间值是否合理（1/1024秒，范围0-65535）
        if (crankEventTime > 0 && crankEventTime <= 65535) {
          sensorData.cadence = calculateCadence(crankRevolutions, crankEventTime);
          sensorData.crankRevolutions = crankRevolutions;
          sensorData.lastCrankEventTime = crankEventTime;
          Serial.printf("[解析] 曲柄转数: %u, 时间: %u (1/1024秒), 踏频: %.1f rpm\n", 
                       crankRevolutions, crankEventTime, sensorData.cadence);
        } else {
          Serial.println("[解析] 曲柄时间值不合理，跳过");
        }
      } else {
        // 只有转数，没有时间（5字节数据包格式）
        // 保存转数，等待下次11字节数据包（包含曲柄时间）
        sensorData.crankRevolutions = crankRevolutions;
        Serial.printf("[解析] 曲柄转数: %u (无时间，等待11字节数据包)\n", crankRevolutions);
      }
    }
  }
  
  if (DEBUG_MODE) {
    Serial.printf("[解析] 解析完成，最终偏移: %d\n", offset);
  }
}

float CSCParser::calculateSpeed(uint32_t wheelRevolutions, uint16_t wheelEventTime) {
  if (lastWheelEventTime == 0) {
    lastWheelRevolutions = wheelRevolutions;
    lastWheelEventTime = wheelEventTime;
    Serial.printf("[速度计算] 第一次数据，保存初始值: 转数=%lu, 时间=%u\n", wheelRevolutions, wheelEventTime);
    return 0.0;
  }
  
  // 计算时间差（1/1024秒）
  uint16_t timeDiff;
  if (wheelEventTime >= lastWheelEventTime) {
    timeDiff = wheelEventTime - lastWheelEventTime;
  } else {
    // 处理溢出（65535 -> 0）
    timeDiff = (65535 - lastWheelEventTime) + wheelEventTime + 1;
    Serial.printf("[速度计算] 时间溢出检测: 上次=%u, 当前=%u, 差值=%u\n", 
                 lastWheelEventTime, wheelEventTime, timeDiff);
  }
  
  float timeSeconds = timeDiff / 1024.0;
  
  if (timeSeconds == 0) {
    Serial.println("[速度计算] 时间差为0，跳过");
    return 0.0;
  }
  
  // 计算转数差
  uint32_t revDiff = wheelRevolutions - lastWheelRevolutions;
  
  // 数据验证：检查是否合理
  // 1. 时间差不能太小（至少1/1024秒，约0.001秒）
  // 2. 时间差不能太大（超过10秒可能有问题，除非是静止后重新开始）
  // 3. 转数差应该合理（单次最多几转）
  
  if (timeDiff < MIN_TIME_DIFF) {
    Serial.printf("[速度计算] 时间差太小: %u (1/1024秒)，跳过\n", timeDiff);
    lastWheelRevolutions = wheelRevolutions;
    lastWheelEventTime = wheelEventTime;
    return 0.0;
  }
  
  // 如果时间差太大（超过10秒），可能是传感器重新启动，重置
  if (timeSeconds > MAX_TIME_DIFF_SEC) {
    Serial.printf("[速度计算] 时间差过大: %.2f秒，可能是传感器重启，重置\n", timeSeconds);
    lastWheelRevolutions = wheelRevolutions;
    lastWheelEventTime = wheelEventTime;
    return 0.0;
  }
  
  // 如果转数差异常大（超过10转），可能是数据错误
  if (revDiff > MAX_REV_DIFF) {
    Serial.printf("[速度计算] 转数差异常: %lu转，时间差: %.3f秒，可能数据错误\n", revDiff, timeSeconds);
    // 仍然更新，但可能需要过滤
  }
  
  // 计算速度 (km/h)
  // 速度 = (转数 * 轮周长) / 时间 * 3.6
  float distance = (revDiff * WHEEL_CIRCUMFERENCE_MM) / 1000000.0;  // 转换为km
  float speed = (distance / timeSeconds) * 3600.0;  // 转换为km/h
  
  // 速度合理性检查（自行车速度通常在0-100 km/h）
  if (speed > MAX_REASONABLE_SPEED) {
    Serial.printf("[速度计算] 警告: 速度异常高 %.2f km/h (转数差=%lu, 时间=%.3f秒)\n", 
                 speed, revDiff, timeSeconds);
    Serial.printf("[速度计算] 可能原因: 传感器触发不稳定或时间戳异常\n");
    // 如果速度异常高且时间差很小，可能是传感器抖动，返回0
    if (timeSeconds < 0.1) {
      Serial.println("[速度计算] 时间差过小，可能是传感器抖动，返回0");
      lastWheelRevolutions = wheelRevolutions;
      lastWheelEventTime = wheelEventTime;
      return 0.0;
    }
  }
  
  Serial.printf("[速度计算] 转数差=%lu, 时间差=%.3f秒, 速度=%.2f km/h\n", revDiff, timeSeconds, speed);
  
  lastWheelRevolutions = wheelRevolutions;
  lastWheelEventTime = wheelEventTime;
  
  return speed;
}

float CSCParser::calculateCadence(uint16_t crankRevolutions, uint16_t crankEventTime) {
  if (lastCrankEventTime == 0) {
    lastCrankRevolutions = crankRevolutions;
    lastCrankEventTime = crankEventTime;
    return 0.0;
  }
  
  // 计算时间差（秒）
  uint16_t timeDiff;
  if (crankEventTime >= lastCrankEventTime) {
    timeDiff = crankEventTime - lastCrankEventTime;
  } else {
    // 处理溢出
    timeDiff = (65535 - lastCrankEventTime) + crankEventTime + 1;
  }
  
  float timeSeconds = timeDiff / 1024.0;
  
  if (timeSeconds == 0) {
    return 0.0;
  }
  
  // 计算转数差
  uint16_t revDiff;
  if (crankRevolutions >= lastCrankRevolutions) {
    revDiff = crankRevolutions - lastCrankRevolutions;
  } else {
    // 处理溢出（uint16_t最大值65535）
    revDiff = (65535 - lastCrankRevolutions) + crankRevolutions + 1;
  }
  
  // 计算踏频 (rpm)
  float cadence = (revDiff / timeSeconds) * 60.0;
  
  lastCrankRevolutions = crankRevolutions;
  lastCrankEventTime = crankEventTime;
  
  return cadence;
}

