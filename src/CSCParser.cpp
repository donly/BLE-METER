/**
 * CSC数据解析类实现
 * 
 * CSC Measurement数据格式（根据BLE规范）:
 * 字节0: 标志位
 *   bit 0: 轮转数存在
 *   bit 1: 轮转时间存在
 *   bit 2: 曲柄转数存在
 *   bit 3: 曲柄时间存在
 * 字节1-4: 累积轮转数 (uint32_t, little-endian)
 * 字节5-6: 最后轮转时间 (uint16_t, 1/1024秒, little-endian)
 * 字节7-10: 累积曲柄转数 (uint32_t, little-endian)
 * 字节11-12: 最后曲柄时间 (uint16_t, 1/1024秒, little-endian)
 */

#include "CSCParser.h"

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

void CSCParser::parseData(uint8_t* data, SensorData& sensorData) {
  if (data == nullptr) {
    return;
  }
  
  // CSC数据至少需要1字节（标志位）
  // 根据标志位判断数据长度，最大约13字节
  uint8_t flags = data[0];
  int offset = 1;
  
  // 解析轮转数据
  if (flags & 0x01) {  // 轮转数存在
    // 需要至少5字节（1标志 + 4轮转数）
    if (offset + 4 <= 13) {  // 假设最大长度为13字节
      uint32_t wheelRevolutions = 
        (uint32_t)data[offset] |
        ((uint32_t)data[offset + 1] << 8) |
        ((uint32_t)data[offset + 2] << 16) |
        ((uint32_t)data[offset + 3] << 24);
      offset += 4;
      
      if (flags & 0x02) {  // 轮转时间存在
        // 需要额外2字节
        if (offset + 2 <= 13) {
          uint16_t wheelEventTime = 
            (uint16_t)data[offset] |
            ((uint16_t)data[offset + 1] << 8);
          offset += 2;
          
          // 计算速度
          sensorData.speed = calculateSpeed(wheelRevolutions, wheelEventTime);
          sensorData.wheelRevolutions = wheelRevolutions;
          sensorData.lastWheelEventTime = wheelEventTime;
        }
      }
    }
  }
  
  // 解析曲柄数据
  if (flags & 0x04) {  // 曲柄转数存在
    // 需要至少2字节（曲柄转数是uint16_t）
    if (offset + 2 <= 13) {
      uint16_t crankRevolutions = 
        (uint16_t)data[offset] |
        ((uint16_t)data[offset + 1] << 8);
      offset += 2;
      
      if (flags & 0x08) {  // 曲柄时间存在
        // 需要额外2字节
        if (offset + 2 <= 13) {
          uint16_t crankEventTime = 
            (uint16_t)data[offset] |
            ((uint16_t)data[offset + 1] << 8);
          
          // 计算踏频
          sensorData.cadence = calculateCadence(crankRevolutions, crankEventTime);
          sensorData.crankRevolutions = crankRevolutions;
          sensorData.lastCrankEventTime = crankEventTime;
        }
      }
    }
  }
}

float CSCParser::calculateSpeed(uint32_t wheelRevolutions, uint16_t wheelEventTime) {
  if (lastWheelEventTime == 0) {
    lastWheelRevolutions = wheelRevolutions;
    lastWheelEventTime = wheelEventTime;
    return 0.0;
  }
  
  // 计算时间差（秒）
  uint16_t timeDiff;
  if (wheelEventTime >= lastWheelEventTime) {
    timeDiff = wheelEventTime - lastWheelEventTime;
  } else {
    // 处理溢出（65535 -> 0）
    timeDiff = (65535 - lastWheelEventTime) + wheelEventTime + 1;
  }
  
  float timeSeconds = timeDiff / 1024.0;
  
  if (timeSeconds == 0) {
    return 0.0;
  }
  
  // 计算转数差
  uint32_t revDiff = wheelRevolutions - lastWheelRevolutions;
  
  // 计算速度 (km/h)
  // 速度 = (转数 * 轮周长) / 时间 * 3.6
  float distance = (revDiff * WHEEL_CIRCUMFERENCE_MM) / 1000000.0;  // 转换为km
  float speed = (distance / timeSeconds) * 3600.0;  // 转换为km/h
  
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

