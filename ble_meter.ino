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

// 按键状态
bool pairingMode = false;
unsigned long buttonPressStartTime = 0;
bool buttonPressed = false;

// 函数声明
void checkPairButton();

void setup() {
  // 初始化串口
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  // 检查是否从深度睡眠唤醒
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("\n\n=== 从定时唤醒 ===");
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("\n\n=== 从RST按钮唤醒（硬件复位）===");
  } else {
    Serial.println("\n\n=== 自行车BLE传感器显示屏 ===");
  }
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

  // 初始化匹配按键（如果配置了）
  #if PAIR_BUTTON_GPIO >= 0
  // ESP32 C3 Super Mini的BOOT按钮（GPIO9）需要配置为输入模式，启用内部上拉
  pinMode(PAIR_BUTTON_GPIO, INPUT_PULLUP);
  // 等待GPIO稳定
  delay(10);
  Serial.printf("匹配按键已初始化: GPIO%d\n", PAIR_BUTTON_GPIO);
  Serial.printf("长按时间: %d ms\n", BUTTON_PRESS_TIME);
  Serial.println("提示: 长按BOOT按钮进入匹配模式");
  #endif

  Serial.println("初始化完成");
  displayManager.clear();
  
  // 尝试快速连接上次的设备
  if (bleManager.scanAndConnect()) {
    sensorData.connected = true;
    Serial.println("✓ 快速连接到上次的设备成功！");
    displayManager.showStatus("已连接");
  } else {
    displayManager.showStatus("等待连接");
    Serial.println("提示: 长按BOOT按钮进入匹配模式");
  }
}

void loop() {
  // 检测匹配按键（如果配置了）
  // 注意：按键检测应该在循环中频繁调用，确保能及时响应
  #if PAIR_BUTTON_GPIO >= 0
  checkPairButton();
  #endif
  
  // 检查BLE连接状态
  if (!sensorData.connected) {
    if (pairingMode) {
      // 匹配模式：强制扫描并连接
      displayManager.showStatus("匹配中...");
      if (bleManager.scanAndConnectForced()) {
        sensorData.connected = true;
        pairingMode = false;
        Serial.println("✓ 匹配成功，传感器连接成功！");
        displayManager.showStatus("已连接");
        delay(1000);
        lastMotionTime = millis();
      } else {
        displayManager.showStatus("匹配失败");
        // 不要使用delay，改用非阻塞方式
        static unsigned long lastMatchTime = 0;
        if (millis() - lastMatchTime > 2000) {
          lastMatchTime = millis();
          pairingMode = false;
        }
      }
    } else {
      // 正常模式：尝试快速连接上次的设备
      // 减少连接尝试频率，避免频繁调用
      static unsigned long lastConnectAttempt = 0;
      static bool connectionAttempted = false;
      
      // 每5秒尝试连接一次
      if (!connectionAttempted || (millis() - lastConnectAttempt > 5000)) {
        connectionAttempted = true;
        lastConnectAttempt = millis();
        
        if (bleManager.scanAndConnect()) {
          sensorData.connected = true;
          Serial.println("✓ 传感器连接成功，开始接收数据...");
          displayManager.showStatus("已连接");
          delay(1000);
          lastMotionTime = millis();
          connectionAttempted = false;  // 连接成功后重置
        } else {
          displayManager.showStatus("设备未找到");
          // 状态信息已在BLEManager中输出，这里不再重复输出
        }
      }
      
        // 如果长时间未连接，进入深度睡眠节省电量
        if (millis() > 30000) {  // 30秒后
          Serial.println("进入深度睡眠...");
          Serial.println("提示: 使用RST按钮唤醒，唤醒后长按BOOT按钮进入匹配模式");
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
      size_t dataLength = bleManager.getLastDataLength();
      if (data != nullptr) {
        // 解析数据
        cscParser.parseData(data, dataLength, sensorData);
        
        // 串口输出解析后的数据
        Serial.println("=== CSC数据 ===");
        Serial.printf("速度: %.2f km/h\n", sensorData.speed);
        Serial.printf("踏频: %.1f rpm\n", sensorData.cadence);
        Serial.printf("轮转数: %lu\n", sensorData.wheelRevolutions);
        Serial.printf("曲柄转数: %u\n", sensorData.crankRevolutions);
        Serial.println("===============");
        
        free(data);  // 释放内存
        sensorData.lastUpdateTime = millis();
        
        // 更新运动时间
        if (sensorData.speed > MOTION_THRESHOLD) {
          lastMotionTime = millis();
        }
      } else {
        // 没有新数据，减少状态输出
        static unsigned long lastStatusTime = 0;
        if (millis() - lastStatusTime > 10000) {  // 每10秒输出一次状态
          Serial.println("等待CSC数据... (连接正常)");
          lastStatusTime = millis();
        }
      }
    } else {
      // 连接断开
      sensorData.connected = false;
      Serial.println("连接断开！");
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

// 检测匹配按键
// 正常运行时，长按BOOT按钮进入匹配模式
// ESP32 C3 Super Mini的BOOT按钮连接到GPIO9
// 注意：深度睡眠时使用RST按钮唤醒（硬件复位）
void checkPairButton() {
  static bool lastRawState = HIGH;
  static bool lastStableState = HIGH;
  static unsigned long lastDebounceTime = 0;
  static bool longPressNotified = false;
  
  // 读取按键状态（BOOT按钮按下时为LOW，释放时为HIGH，使用内部上拉）
  bool currentButtonState = digitalRead(PAIR_BUTTON_GPIO);
  
  // 检测原始状态变化（用于防抖）
  if (currentButtonState != lastRawState) {
    lastDebounceTime = millis();
    lastRawState = currentButtonState;
  }
  
  // 状态稳定后处理（防抖时间后）
  if ((millis() - lastDebounceTime) > BUTTON_DEBOUNCE_TIME) {
    // 按键按下（从HIGH变为LOW）
    if (currentButtonState == LOW && lastStableState == HIGH) {
      buttonPressStartTime = millis();
      buttonPressed = true;
      longPressNotified = false;
    } 
    // 按键释放（从LOW变为HIGH）
    else if (currentButtonState == HIGH && lastStableState == LOW) {
      if (buttonPressed) {
        unsigned long pressDuration = millis() - buttonPressStartTime;
        buttonPressed = false;
        
        if (pressDuration >= BUTTON_PRESS_TIME) {
          // 长按：进入匹配模式
          Serial.println("=== 进入匹配模式 ===");
          if (!pairingMode) {
            pairingMode = true;
            Serial.println("开始扫描并连接新的CSC传感器...");
            displayManager.showStatus("匹配模式");
            // 清除上次保存的设备地址
            bleManager.clearLastDevice();
            // 如果已连接，先断开
            if (sensorData.connected) {
              bleManager.disconnect();
              sensorData.connected = false;
            }
          } else {
            // 匹配模式下再次长按：取消匹配
            pairingMode = false;
            Serial.println("取消匹配模式");
            displayManager.showStatus("已取消");
          }
        }
      }
    }
    // 按键持续按下时的反馈（仅在长按时间达到时输出一次）
    else if (buttonPressed && currentButtonState == LOW) {
      unsigned long pressDuration = millis() - buttonPressStartTime;
      if (pressDuration >= BUTTON_PRESS_TIME && !longPressNotified) {
        longPressNotified = true;
      }
    }
    
    // 更新稳定状态
    lastStableState = currentButtonState;
  }
  
  lastRawState = currentButtonState;
}

