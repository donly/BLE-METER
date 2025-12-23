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
#include <Preferences.h>

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
  int8_t batteryLevel = -1;  // 电池电量 (0-100, -1表示未获取)
  String deviceName = "";   // 设备名称
  int8_t rssi = 0;          // 信号强度 (dBm)
  float distance = 0.0;     // 此次连接以来的总路程 (km)
  float totalDistance = 0.0; // 总路程（累积所有连接的路程，km）
  float averageSpeed = 0.0; // 平均速度 (km/h)
  unsigned long rideDuration = 0;  // 本次骑行时长（秒）
  uint32_t initialWheelRevolutions = 0;  // 连接时的初始轮转数
  unsigned long connectionStartTime = 0;  // 连接开始时间（用于计算平均速度和骑行时长）
  unsigned long lastUpdateTime = 0;
} sensorData;

// 状态变量
unsigned long lastDisplayUpdate = 0;
unsigned long lastMotionTime = 0;

// 按键状态
bool pairingMode = false;
unsigned long buttonPressStartTime = 0;
bool buttonPressed = false;

// 显示主题（运行时变量，0=数字表盘，1=模拟表盘，2=统计表盘）
uint8_t currentDisplayTheme = DISPLAY_THEME;
Preferences themePreferences;  // 用于保存主题设置
Preferences distancePreferences;  // 用于保存总路程

// 函数声明
void checkPairButton();

void setup() {
  // 初始化串口
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  // 检查是否从深度睡眠唤醒
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    // 定时唤醒时不输出提示，避免干扰串口日志
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

  // ========== 显示调试模式 ==========
  // Splash 显示完后，如果启用调试模式，直接显示主界面并跳过所有后续代码
  #if defined(DISPLAY_DEBUG_MODE) && DISPLAY_DEBUG_MODE
    Serial.println("=== 显示调试模式已启用 ===");
    Serial.println("将直接显示主界面，使用模拟数据");
    Serial.println("速度: 25.5 km/h, 踏频: 85 rpm, 状态: 已连接");
    Serial.println("提示: 修改 config.h 中的 DISPLAY_DEBUG_MODE 为 false 可恢复正常运行");
    
    // 直接显示调试主界面
    displayManager.showDebugDisplay();
    Serial.println("调试主界面已显示，不再执行其他初始化代码");
    // 直接返回，跳过所有后续初始化（BLE、功耗管理、按键等）
    return;
  #endif
  // ========== 正常模式 ==========

  // 初始化BLE
  if (!bleManager.begin()) {
    Serial.println("BLE初始化失败！");
    displayManager.showError("BLE Init Failed");
    delay(3000);
  }

  // 初始化功耗管理
  powerManager.begin();

  // 加载保存的主题设置
  themePreferences.begin("display", false);
  uint8_t savedTheme = themePreferences.getUChar("theme", DISPLAY_THEME);
  Serial.printf("从Preferences读取的主题值: %d (默认值: %d)\n", savedTheme, DISPLAY_THEME);
  if (savedTheme <= 2) {  // 验证主题值有效（0=数字表盘，1=模拟表盘，2=统计表盘）
    currentDisplayTheme = savedTheme;
    Serial.printf("✓ 加载保存的主题: %d (0=数字表盘, 1=模拟表盘, 2=统计表盘)\n", currentDisplayTheme);
  } else {
    currentDisplayTheme = DISPLAY_THEME;
    Serial.printf("✗ 主题值无效，使用默认主题: %d\n", currentDisplayTheme);
  }
  
  // 加载保存的总路程
  distancePreferences.begin("distance", false);
  sensorData.totalDistance = distancePreferences.getFloat("total", 0.0);
  Serial.printf("加载总路程: %.3f km\n", sensorData.totalDistance);

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
  displayManager.showStatus("连接中...");
  
  // 尝试快速连接上次的设备
  if (bleManager.scanAndConnect()) {
    sensorData.connected = true;
    Serial.println("✓ 快速连接到上次的设备成功！");
    // 保存设备名称和信号强度
    sensorData.deviceName = bleManager.getDeviceName();
    sensorData.rssi = bleManager.getRSSI();
    // 立即读取一次电量
    sensorData.batteryLevel = bleManager.readBatteryLevel();
    // 重置路程统计、平均速度和骑行时长
    sensorData.distance = 0.0;
    sensorData.averageSpeed = 0.0;
    sensorData.rideDuration = 0;
    sensorData.initialWheelRevolutions = 0;
    sensorData.connectionStartTime = millis();
    displayManager.showStatus("已连接");
  } else {
    displayManager.showStatus("等待连接");
    Serial.println("提示: 长按BOOT按钮进入匹配模式");
  }
}

void loop() {
  // ========== 显示调试模式 ==========
  #if defined(DISPLAY_DEBUG_MODE) && DISPLAY_DEBUG_MODE
    // 调试模式下只更新显示，不执行其他逻辑
    static unsigned long lastDebugUpdate = 0;
    if (millis() - lastDebugUpdate >= DISPLAY_REFRESH_INTERVAL) {
      displayManager.showDebugDisplay();
      lastDebugUpdate = millis();
    }
    delay(10);
    return;  // 跳过后续所有逻辑
  #endif
  // ========== 正常模式 ==========

  // 检测匹配按键（如果配置了）
  // 注意：按键检测应该在循环中频繁调用，确保能及时响应
  #if PAIR_BUTTON_GPIO >= 0
  checkPairButton();
  #endif
  
  // 检查BLE连接状态
  if (!sensorData.connected) {
    if (pairingMode) {
      // 匹配模式：强制扫描并连接
      static unsigned long lastPairingUpdate = 0;
      if (millis() - lastPairingUpdate > 1000) {  // 每秒更新一次显示
        displayManager.showStatus("匹配中...");
        lastPairingUpdate = millis();
      }
      
      if (bleManager.scanAndConnectForced()) {
        sensorData.connected = true;
        pairingMode = false;
        Serial.println("✓ 匹配成功，传感器连接成功！");
        // 保存设备名称和信号强度
        sensorData.deviceName = bleManager.getDeviceName();
        sensorData.rssi = bleManager.getRSSI();
        // 立即读取一次电量
        sensorData.batteryLevel = bleManager.readBatteryLevel();
        // 重置路程统计和平均速度
        sensorData.distance = 0.0;
        sensorData.averageSpeed = 0.0;
        sensorData.initialWheelRevolutions = 0;
        sensorData.connectionStartTime = millis();
        displayManager.showStatus("已连接");
        delay(1000);
        lastMotionTime = millis();
      } else {
        // 匹配失败时显示状态，但不要立即退出匹配模式
        static unsigned long lastMatchTime = 0;
        if (millis() - lastMatchTime > 2000) {
          lastMatchTime = millis();
          // 继续显示“Pairing...”，不要显示“匹配失败”
        }
      }
    } else {
      // 正常模式：尝试快速连接上次的设备
      // 减少连接尝试频率，避免频繁调用
      static unsigned long lastConnectAttempt = 0;
      static bool connectionAttempted = false;
      static unsigned long lastStatusUpdate = 0;
      
      // 定期更新显示状态（即使不在连接尝试时）
      if (millis() - lastStatusUpdate > 1000) {  // 每秒更新一次显示
        if (connectionAttempted && (millis() - lastConnectAttempt < 5000)) {
          // 正在尝试连接
          displayManager.showStatus("连接中...");
        } else {
          // 等待连接
          displayManager.showStatus("等待连接");
        }
        lastStatusUpdate = millis();
      }
      
      // 每5秒尝试连接一次
      if (!connectionAttempted || (millis() - lastConnectAttempt > 5000)) {
        connectionAttempted = true;
        lastConnectAttempt = millis();
        
        if (bleManager.scanAndConnect()) {
          sensorData.connected = true;
          Serial.println("✓ 传感器连接成功，开始接收数据...");
          // 保存设备名称和信号强度
          sensorData.deviceName = bleManager.getDeviceName();
          sensorData.rssi = bleManager.getRSSI();
          // 立即读取一次电量
          sensorData.batteryLevel = bleManager.readBatteryLevel();
          // 重置路程统计和平均速度
          sensorData.distance = 0.0;
          sensorData.averageSpeed = 0.0;
          sensorData.initialWheelRevolutions = 0;
          sensorData.connectionStartTime = millis();
          displayManager.showStatus("已连接");
          delay(1000);
          lastMotionTime = millis();
          connectionAttempted = false;  // 连接成功后重置
        } else {
          // 连接失败，显示状态会在上面的定期更新中处理
          // 状态信息已在BLEManager中输出，这里不再重复输出
        }
      }
      
        // 如果长时间未连接，进入深度睡眠节省电量
        if (millis() > 30000) {  // 30秒后
          Serial.println("进入深度睡眠...");
          Serial.println("提示: 使用RST按钮唤醒，唤醒后长按BOOT按钮进入匹配模式");
          displayManager.powerOff();  // 直接关闭显示，避免睡眠字样缺字
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
        
        // 计算路程（此次连接以来的总路程）
        if (sensorData.initialWheelRevolutions == 0) {
          // 第一次收到数据，记录初始轮转数
          sensorData.initialWheelRevolutions = sensorData.wheelRevolutions;
        } else {
          // 计算轮转数差
          uint32_t revDiff = sensorData.wheelRevolutions - sensorData.initialWheelRevolutions;
          // 计算路程（km）= 轮转数差 × 轮周长(mm) / 1000000.0
          float distanceKm = (revDiff * WHEEL_CIRCUMFERENCE_MM) / 1000000.0;
          sensorData.distance = distanceKm;
          
          // 计算平均速度（路程 / 连接时长）
          unsigned long connectionDuration = millis() - sensorData.connectionStartTime;
          if (connectionDuration > 0) {
            float connectionHours = connectionDuration / 3600000.0;  // 转换为小时
            if (connectionHours > 0) {
              sensorData.averageSpeed = distanceKm / connectionHours;
            }
            // 计算本次骑行时长（秒）
            sensorData.rideDuration = connectionDuration / 1000;
          }
        }
        
        // 读取电池电量（定期读取，避免频繁调用）
        static unsigned long lastBatteryRead = 0;
        if (millis() - lastBatteryRead > 5000) {  // 每5秒读取一次电量
          sensorData.batteryLevel = bleManager.readBatteryLevel();
          lastBatteryRead = millis();
        }
        
        // 串口输出解析后的数据
        Serial.println("=== CSC数据 ===");
        if (sensorData.deviceName.length() > 0) {
          Serial.printf("设备名称: %s\n", sensorData.deviceName.c_str());
        }
        if (sensorData.rssi != 0) {
          Serial.printf("信号强度: %d dBm\n", sensorData.rssi);
        }
        Serial.printf("速度: %.2f km/h\n", sensorData.speed);
        Serial.printf("踏频: %.1f rpm\n", sensorData.cadence);
        Serial.printf("本次路程: %.3f km\n", sensorData.distance);
        Serial.printf("总路程: %.3f km\n", sensorData.totalDistance);
        Serial.printf("平均速度: %.2f km/h\n", sensorData.averageSpeed);
        unsigned long hours = sensorData.rideDuration / 3600;
        unsigned long minutes = (sensorData.rideDuration % 3600) / 60;
        unsigned long seconds = sensorData.rideDuration % 60;
        Serial.printf("骑行时长: %lu:%02lu:%02lu\n", hours, minutes, seconds);
        Serial.printf("轮转数: %lu\n", sensorData.wheelRevolutions);
        Serial.printf("曲柄转数: %u\n", sensorData.crankRevolutions);
        if (sensorData.batteryLevel >= 0) {
          Serial.printf("电池电量: %d%%\n", sensorData.batteryLevel);
        }
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
      // 计算最终骑行时长
      if (sensorData.connectionStartTime > 0) {
        unsigned long connectionDuration = millis() - sensorData.connectionStartTime;
        sensorData.rideDuration = connectionDuration / 1000;
      }
      
      // 累积此次连接的路程到总路程
      if (sensorData.distance > 0.0) {
        sensorData.totalDistance += sensorData.distance;
        distancePreferences.putFloat("total", sensorData.totalDistance);
        unsigned long hours = sensorData.rideDuration / 3600;
        unsigned long minutes = (sensorData.rideDuration % 3600) / 60;
        unsigned long seconds = sensorData.rideDuration % 60;
        Serial.printf("连接断开，累积路程: %.3f km，总路程: %.3f km，平均速度: %.2f km/h，骑行时长: %lu:%02lu:%02lu\n", 
                     sensorData.distance, sensorData.totalDistance, sensorData.averageSpeed, hours, minutes, seconds);
      }
      
      sensorData.connected = false;
      sensorData.deviceName = "";
      sensorData.rssi = 0;
      sensorData.batteryLevel = -1;
      sensorData.distance = 0.0;
      sensorData.averageSpeed = 0.0;
      sensorData.rideDuration = 0;
      sensorData.initialWheelRevolutions = 0;
      sensorData.connectionStartTime = 0;
      Serial.println("连接断开！");
      displayManager.showStatus("连接断开");
      delay(2000);
    }
  }

  // 更新显示
  if (millis() - lastDisplayUpdate >= DISPLAY_REFRESH_INTERVAL) {
    static uint8_t lastTheme = 255;
    if (currentDisplayTheme != lastTheme) {
      Serial.printf("切换显示主题: %d\n", currentDisplayTheme);
      lastTheme = currentDisplayTheme;
    }
    displayManager.updateDisplay(sensorData, currentDisplayTheme);
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
        } else if (pressDuration > BUTTON_DEBOUNCE_TIME && pressDuration < BUTTON_PRESS_TIME) {
          // 短按：切换显示主题（0->1->2->0循环）
          currentDisplayTheme = (currentDisplayTheme + 1) % 3;
          themePreferences.putUChar("theme", currentDisplayTheme);
          const char* themeNames[] = {"数字表盘", "模拟表盘", "统计表盘"};
          Serial.printf("切换显示主题: %d (%s)\n", currentDisplayTheme, themeNames[currentDisplayTheme]);
          displayManager.showStatus(themeNames[currentDisplayTheme]);
          delay(1000);
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

