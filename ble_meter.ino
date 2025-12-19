/**
 * 自行车BLE传感器显示屏主程序
 * 基于ESP32 C3开发板
 * 
 * 功能：
 * - 通过BLE连接CSC传感器
 * - 实时显示速度和踏频
 * - 低功耗管理
 */

#include "config.h"
#include "src/BLEManager.h"
#include "src/DisplayManager.h"
#include "src/PowerManager.h"
#include "src/CSCParser.h"

// 全局对象
BLEManager bleManager;
DisplayManager displayManager;
PowerManager powerManager;
CSCParser cscParser;

// 传感器数据
struct SensorData {
  float speed = 0.0;        // 速度 (km/h)
  float cadence = 0.0;      // 踏频 (rpm)
  uint32_t wheelRevolutions = 0;
  uint16_t lastWheelEventTime = 0;
  uint16_t crankRevolutions = 0;
  uint16_t lastCrankEventTime = 0;
  bool connected = false;
  unsigned long lastUpdateTime = 0;
} sensorData;

// 状态变量
unsigned long lastDisplayUpdate = 0;
unsigned long lastMotionTime = 0;

void setup() {
  // 初始化串口
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  Serial.println("\n\n=== 自行车BLE传感器显示屏 ===");
  Serial.println("初始化中...");

  // 配置CPU频率
  setCpuFrequencyMhz(CPU_FREQ_MHZ);
  Serial.printf("CPU频率: %d MHz\n", getCpuFrequencyMhz());

  // 初始化显示
  if (!displayManager.begin()) {
    Serial.println("显示初始化失败！");
    while(1) delay(1000);
  }
  displayManager.showSplash("BLE Meter");
  delay(2000);

  // 初始化BLE
  if (!bleManager.begin()) {
    Serial.println("BLE初始化失败！");
    displayManager.showError("BLE Init Failed");
    delay(3000);
  }

  // 初始化功耗管理
  powerManager.begin();

  Serial.println("初始化完成");
  displayManager.clear();
  displayManager.showStatus("扫描传感器...");
}

void loop() {
  // 检查BLE连接状态
  if (!sensorData.connected) {
    // 尝试连接传感器
    if (bleManager.scanAndConnect()) {
      sensorData.connected = true;
      displayManager.showStatus("已连接");
      delay(1000);
      lastMotionTime = millis();
    } else {
      displayManager.showStatus("未找到传感器");
      delay(1000);
      
      // 如果长时间未连接，进入深度睡眠节省电量
      if (millis() > 30000) {  // 30秒后
        Serial.println("进入深度睡眠...");
        displayManager.showStatus("睡眠中...");
        delay(1000);
        powerManager.enterDeepSleep();
      }
    }
  } else {
    // 已连接，读取数据
    if (bleManager.isConnected()) {
      // 读取CSC数据
      uint8_t* data = bleManager.readCSCData();
      if (data != nullptr) {
        // 解析数据
        cscParser.parseData(data, sensorData);
        free(data);  // 释放内存
        sensorData.lastUpdateTime = millis();
        
        // 更新运动时间
        if (sensorData.speed > MOTION_THRESHOLD) {
          lastMotionTime = millis();
        }
      }
    } else {
      // 连接断开
      sensorData.connected = false;
      displayManager.showStatus("连接断开");
      delay(2000);
    }
  }

  // 更新显示
  if (millis() - lastDisplayUpdate >= DISPLAY_REFRESH_INTERVAL) {
    displayManager.updateDisplay(sensorData);
    lastDisplayUpdate = millis();
  }

  // 检查是否需要进入睡眠
  if (STATIONARY_TIME > 0 && 
      (millis() - lastMotionTime) > (STATIONARY_TIME * 1000) &&
      sensorData.speed < MOTION_THRESHOLD) {
    Serial.println("检测到静止，进入深度睡眠...");
    displayManager.showStatus("睡眠中...");
    delay(1000);
    powerManager.enterDeepSleep();
  }

  // 短暂延迟，避免CPU占用过高
  delay(10);
}

