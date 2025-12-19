/**
 * CSC数据解析类
 * 解析BLE CSC传感器数据，计算速度和踏频
 */

#ifndef CSC_PARSER_H
#define CSC_PARSER_H

#include <stdint.h>
#include "config.h"

// 前向声明
struct SensorData;

class CSCParser {
private:
  uint32_t lastWheelRevolutions;
  uint16_t lastWheelEventTime;
  uint32_t lastCrankRevolutions;
  uint16_t lastCrankEventTime;
  
  float calculateSpeed(uint32_t wheelRevolutions, uint16_t wheelEventTime);
  float calculateCadence(uint16_t crankRevolutions, uint16_t crankEventTime);
  
public:
  CSCParser();
  
  void parseData(uint8_t* data, SensorData& sensorData);
  void reset();
};

#endif // CSC_PARSER_H

