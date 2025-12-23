/**
 * 显示管理类实现
 * 使用 U8g2lib 库支持中文字体显示
 */

#include "DisplayManager.h"
#include <Arduino.h>
#include <math.h>

// 前向声明结构体（在ble_meter.ino中定义）
struct SensorData {
  float speed;
  float cadence;
  bool connected;
  int8_t batteryLevel;
  String deviceName;
  int8_t rssi;
  float distance;
  float totalDistance;
  float averageSpeed;
  unsigned long rideDuration;
  uint32_t wheelRevolutions;
  uint32_t initialWheelRevolutions;
  unsigned long connectionStartTime;
};

DisplayManager::DisplayManager() {
#ifdef OLED_128x64
  display = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#else
  display = new U8G2_SSD1306_128X32_NONAME_F_HW_I2C(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#endif
  initialized = false;
}

DisplayManager::~DisplayManager() {
  if (display) {
    delete display;
  }
}

bool DisplayManager::begin() {
  // 初始化 I2C 总线（使用自定义引脚）
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  
  // 设置 I2C 地址（U8g2 需要 8 位地址，所以需要乘以 2）
  display->setI2CAddress(OLED_I2C_ADDRESS * 2);
  
  // 初始化显示（HW_I2C 版本使用硬件 I2C）
  display->begin();
  display->enableUTF8Print();
  display->clearBuffer();
  display->sendBuffer();
  
  // 默认中文字体：unifont Chinese3（覆盖更多汉字）
  display->setFont(u8g2_font_unifont_t_chinese3);
  
  initialized = true;
  
  Serial.println("OLED显示初始化成功");
  Serial.println("使用HW_I2C，I2C地址: 0x");
  Serial.print(OLED_I2C_ADDRESS, HEX);
  Serial.print(" (设置值: 0x");
  Serial.print(OLED_I2C_ADDRESS * 2, HEX);
  Serial.println(")");
  return true;
}

void DisplayManager::clear() {
  if (display) {
    display->clearBuffer();
    display->sendBuffer();
  }
}

void DisplayManager::showSplash(const char* text) {
  if (!display) return;
  
  display->clearBuffer();
  // 使用较大英文字体并居中显示
  display->setFont(u8g2_font_logisoso24_tr);
  int16_t x = (display->getDisplayWidth() - display->getUTF8Width(text)) / 2;
  int16_t y = 44;  // 适配128x64，垂直居中略下
  if (x < 0) x = 0;
  display->drawUTF8(x, y, text);
  display->sendBuffer();
}

void DisplayManager::showStatus(const char* text) {
  if (!display) return;
  
  display->clearBuffer();
  // 使用 unifont Chinese3，覆盖更多汉字，略微右移防止裁剪
  display->setFont(u8g2_font_unifont_t_chinese3);
  display->drawUTF8(2, 32, text);
  display->sendBuffer();
}

void DisplayManager::showError(const char* text) {
  if (!display) return;
  
  display->clearBuffer();
  display->setFont(u8g2_font_unifont_t_chinese3);
  display->drawUTF8(0, 12, "错误:");
  display->drawUTF8(0, 28, text);
  display->sendBuffer();
}

void DisplayManager::updateDisplay(const SensorData& data, uint8_t theme) {
  if (!display) return;
  
  display->clearBuffer();
  
  // 根据主题选择显示方式
  if (theme == 1) {
    // 模拟仪表盘主题
    drawAnalogSpeedometer(data);
    display->sendBuffer();
    return;
  } else if (theme == 2) {
    // 数据统计表盘主题
    drawStatisticsPanel(data);
    display->sendBuffer();
    return;
  }
  
  // 主题0：数字显示仪表盘主题（默认）
  
  // 显示速度（大字体）
  display->setFont(u8g2_font_logisoso32_tn);  // 使用数字字体显示速度
  char speedStr[16];
  snprintf(speedStr, sizeof(speedStr), "%.1f", data.speed);
  display->setCursor(0, 32);
  display->print(speedStr);
  
  // 显示单位
  display->setFont(u8g2_font_unifont_t_chinese3);
  display->drawUTF8(85, 12, "km/h");
  
  // 显示踏频（仅128x64屏幕）
#ifdef OLED_128x64
  // 踏频数字（中等字体，比速度小）
  display->setFont(u8g2_font_logisoso24_tn);  // 使用中等字体显示踏频（比速度的32小）
  char cadenceStr[16];
  snprintf(cadenceStr, sizeof(cadenceStr), "%.0f", data.cadence);
  display->setCursor(0, 64);  // 底部对齐
  display->print(cadenceStr);
  
  // 显示"rpm"单位（在踏频数字后面，紧跟着）
  display->setFont(u8g2_font_unifont_t_chinese3);
  display->drawUTF8(display->getCursorX() + 2, 64, "rpm");
  
  // 绘制踏频轮子动画（在右侧）
  drawCadenceWheel(data.cadence, millis());
#else
  // 128x32 屏幕空间较小，只显示关键信息
  display->setFont(u8g2_font_unifont_t_chinese3);
  if (data.connected) {
    int16_t x = 0;
    // 显示设备名称（如果有，最多7个字符）
    if (data.deviceName.length() > 0) {
      String name = data.deviceName;
      if (name.length() > 7) {
        name = name.substring(0, 7);
      }
      display->drawUTF8(x, 20, name.c_str());
      x += display->getUTF8Width(name.c_str()) + 1;
    }
    
    // 显示信号强度（去掉单位）
    if (data.rssi != 0) {
      char rssiStr[16];
      snprintf(rssiStr, sizeof(rssiStr), "%d", data.rssi);
      display->drawUTF8(x, 20, rssiStr);
      x += display->getUTF8Width(rssiStr) + 1;
    }
    
    // 显示电量（如果有，显示在右侧）
    if (data.batteryLevel >= 0) {
      char batteryStr[16];
      snprintf(batteryStr, sizeof(batteryStr), "%d%%", data.batteryLevel);
      int16_t batteryX = display->getDisplayWidth() - display->getUTF8Width(batteryStr);
      display->drawUTF8(batteryX, 20, batteryStr);
    }
  } else {
    display->drawUTF8(0, 20, "Disconnected");
  }
#endif
  
  display->sendBuffer();
}

// 关闭显示（清空并进入省电）
void DisplayManager::powerOff() {
  if (!display) return;
  display->clearBuffer();
  display->sendBuffer();
  display->setPowerSave(1);
}

void DisplayManager::drawSpeed(float speed) {
  // 可以在这里实现更复杂的速度显示
}

void DisplayManager::drawCadence(float cadence) {
  // 可以在这里实现更复杂的踏频显示
}

void DisplayManager::drawConnectionStatus(bool connected) {
  // 可以在这里实现连接状态指示
}

// 绘制模拟仪表盘
void DisplayManager::drawAnalogSpeedometer(const SensorData& data) {
  if (!display) return;
  
#ifdef OLED_128x64
  const int16_t centerX = 64;   // 表盘中心X坐标（屏幕中心）
  const int16_t centerY = 55;   // 表盘中心Y坐标（减少与顶部距离）
  const int16_t radiusX = 62;   // 表盘X方向半径（水平方向，增大）
  const int16_t radiusY = 52;   // 表盘Y方向半径（垂直方向，压扁，增大）
  const float maxSpeed = 60.0;  // 最大速度 60 km/h
  const float startAngle = 180.0;  // 起始角度（左侧，0 km/h）
  const float endAngle = 0.0;      // 结束角度（右侧，60 km/h）
  const float angleRange = 180.0;  // 角度范围（180度，半圆）
  
  // 绘制表盘外圆（半椭圆，压扁适配）
  for (float angle = startAngle; angle >= endAngle; angle -= 2.0) {
    float rad = angle * M_PI / 180.0;
    int16_t x = centerX + (int16_t)(cos(rad) * radiusX);
    int16_t y = centerY - (int16_t)(sin(rad) * radiusY);  // 负号修正上下颠倒
    display->drawPixel(x, y);
  }
  
  // 绘制刻度线
  for (int i = 0; i <= 6; i++) {
    float speed = i * 10.0;  // 0, 10, 20, 30, 40, 50, 60
    float angle = startAngle - (speed / maxSpeed) * angleRange;  // 左边0，右边60
    float rad = angle * M_PI / 180.0;
    
    // 长刻度线
    int16_t x1 = centerX + (int16_t)(cos(rad) * (radiusX - 6));
    int16_t y1 = centerY - (int16_t)(sin(rad) * (radiusY - 6));  // 负号修正
    int16_t x2 = centerX + (int16_t)(cos(rad) * radiusX);
    int16_t y2 = centerY - (int16_t)(sin(rad) * radiusY);  // 负号修正
    display->drawLine(x1, y1, x2, y2);
    
    // 显示刻度值（0, 20, 40, 60）
    if (i % 2 == 0) {
      display->setFont(u8g2_font_6x10_tf);
      char speedLabel[4];
      snprintf(speedLabel, sizeof(speedLabel), "%.0f", speed);
      int16_t labelX = centerX + (int16_t)(cos(rad) * (radiusX - 15)) - 6;
      int16_t labelY = centerY - (int16_t)(sin(rad) * (radiusY - 15)) + 3;  // 负号修正
      display->drawStr(labelX, labelY, speedLabel);
    }
  }
  
  // 绘制指针
  float currentSpeed = data.speed;
  if (currentSpeed > maxSpeed) currentSpeed = maxSpeed;
  if (currentSpeed < 0) currentSpeed = 0;
  
  float pointerAngle = startAngle - (currentSpeed / maxSpeed) * angleRange;  // 左边0，右边60
  float pointerRad = pointerAngle * M_PI / 180.0;
  
  // 指针长度（从中心到表盘边缘）
  int16_t pointerLengthX = radiusX - 10;
  int16_t pointerLengthY = radiusY - 10;
  int16_t pointerX = centerX + (int16_t)(cos(pointerRad) * pointerLengthX);
  int16_t pointerY = centerY - (int16_t)(sin(pointerRad) * pointerLengthY);  // 负号修正
  
  // 绘制指针线
  display->drawLine(centerX, centerY, pointerX, pointerY);
  
  // 绘制指针中心点
  display->drawDisc(centerX, centerY, 3, U8G2_DRAW_ALL);
  
  // 左上角显示信号强度
  if (data.rssi != 0) {
    display->setFont(u8g2_font_6x10_tf);
    char rssiStr[8];
    snprintf(rssiStr, sizeof(rssiStr), "%d", data.rssi);
    display->drawStr(2, 10, rssiStr);
  }
  
  // 右上角显示电量
  if (data.batteryLevel >= 0) {
    display->setFont(u8g2_font_6x10_tf);
    char batteryStr[8];
    snprintf(batteryStr, sizeof(batteryStr), "%d%%", data.batteryLevel);
    int16_t batteryX = display->getDisplayWidth() - display->getStrWidth(batteryStr) - 2;
    display->drawStr(batteryX, 10, batteryStr);
  }
  
  // 表中间显示踏频
  display->setFont(u8g2_font_logisoso16_tn);
  char cadenceStr[8];
  snprintf(cadenceStr, sizeof(cadenceStr), "%.0f", data.cadence);
  int16_t cadenceX = centerX - display->getStrWidth(cadenceStr) / 2;
  int16_t cadenceY = centerY + 2;  // 上移，从+8改为+2
  display->drawStr(cadenceX, cadenceY, cadenceStr);
  
  // 显示"rpm"单位
  display->setFont(u8g2_font_6x10_tf);
  int16_t rpmX = centerX - 12;
  int16_t rpmY = cadenceY + 12;
  display->drawStr(rpmX, rpmY, "rpm");
#endif
}

// 绘制数据统计表盘
void DisplayManager::drawStatisticsPanel(const SensorData& data) {
  if (!display) return;
  
#ifdef OLED_128x64
  // 128x64屏幕：显示完整统计信息
  display->setFont(u8g2_font_6x10_tf);
  int16_t y = 10;
  int16_t lineHeight = 9;
  
  // 第1行：速度
  char speedStr[32];
  snprintf(speedStr, sizeof(speedStr), "Speed: %.1f km/h", data.speed);
  display->drawStr(2, y, speedStr);
  y += lineHeight;
  
  // 第2行：踏频
  char cadenceStr[32];
  snprintf(cadenceStr, sizeof(cadenceStr), "Cadence: %.0f rpm", data.cadence);
  display->drawStr(2, y, cadenceStr);
  y += lineHeight;
  
  // 第3行：本次路程
  char distanceStr[32];
  if (data.distance < 1.0) {
    snprintf(distanceStr, sizeof(distanceStr), "Distance: %.0f m", data.distance * 1000.0);
  } else {
    snprintf(distanceStr, sizeof(distanceStr), "Distance: %.2f km", data.distance);
  }
  display->drawStr(2, y, distanceStr);
  y += lineHeight;
  
  // 第4行：总路程
  char totalDistStr[32];
  if (data.totalDistance < 1.0) {
    snprintf(totalDistStr, sizeof(totalDistStr), "Total: %.0f m", data.totalDistance * 1000.0);
  } else {
    snprintf(totalDistStr, sizeof(totalDistStr), "Total: %.2f km", data.totalDistance);
  }
  display->drawStr(2, y, totalDistStr);
  y += lineHeight;
  
  // 第5行：平均速度
  char avgSpeedStr[32];
  snprintf(avgSpeedStr, sizeof(avgSpeedStr), "Avg Speed: %.1f km/h", data.averageSpeed);
  display->drawStr(2, y, avgSpeedStr);
  y += lineHeight;
  
  // 第6行：本次骑行时长
  char durationStr[32];
  unsigned long hours = data.rideDuration / 3600;
  unsigned long minutes = (data.rideDuration % 3600) / 60;
  unsigned long seconds = data.rideDuration % 60;
  if (hours > 0) {
    snprintf(durationStr, sizeof(durationStr), "Duration: %lu:%02lu:%02lu", hours, minutes, seconds);
  } else {
    snprintf(durationStr, sizeof(durationStr), "Duration: %lu:%02lu", minutes, seconds);
  }
  display->drawStr(2, y, durationStr);
  y += lineHeight;
  
  // 第7行：设备名称 + 信号/电量（合并为一行，保持总行数≤7）
  char infoStr[48];
  if (data.connected) {
    String deviceName = "";
    if (data.deviceName.length() > 0) {
      deviceName = data.deviceName;
      if (deviceName.length() > 7) {
        deviceName = deviceName.substring(0, 7);
      }
    }
    
    int len = 0;
    if (deviceName.length() > 0) {
      len = snprintf(infoStr, sizeof(infoStr), "%s", deviceName.c_str());
    } else {
      len = snprintf(infoStr, sizeof(infoStr), "Connected");
    }
    
    if (data.rssi != 0 && len < (int)sizeof(infoStr)) {
      len += snprintf(infoStr + len, sizeof(infoStr) - len, " R:%d", data.rssi);
    }
    if (data.batteryLevel >= 0 && len < (int)sizeof(infoStr)) {
      len += snprintf(infoStr + len, sizeof(infoStr) - len, " B:%d%%", data.batteryLevel);
    }
    display->drawStr(2, y, infoStr);
  } else {
    snprintf(infoStr, sizeof(infoStr), "Disconnected");
    display->drawStr(2, y, infoStr);
  }
#else
  // 128x32屏幕：显示简化统计信息
  display->setFont(u8g2_font_6x10_tf);
  int16_t y = 8;
  int16_t lineHeight = 8;
  
  // 第1行：速度和踏频
  char line1[32];
  snprintf(line1, sizeof(line1), "S:%.1f C:%.0f", data.speed, data.cadence);
  display->drawStr(2, y, line1);
  y += lineHeight;
  
  // 第2行：路程
  char line2[32];
  if (data.distance < 1.0) {
    snprintf(line2, sizeof(line2), "D:%.0fm T:%.0fm", data.distance * 1000.0, data.totalDistance * 1000.0);
  } else {
    snprintf(line2, sizeof(line2), "D:%.2fkm T:%.1fkm", data.distance, data.totalDistance);
  }
  display->drawStr(2, y, line2);
  y += lineHeight;
  
  // 第3行：平均速度和骑行时长
  char line3[32];
  unsigned long hours = data.rideDuration / 3600;
  unsigned long minutes = (data.rideDuration % 3600) / 60;
  if (hours > 0) {
    snprintf(line3, sizeof(line3), "Avg:%.1f T:%lu:%02lu", data.averageSpeed, hours, minutes);
  } else {
    snprintf(line3, sizeof(line3), "Avg:%.1f T:%lum", data.averageSpeed, minutes);
  }
  display->drawStr(2, y, line3);
  y += lineHeight;
  
  // 第4行：信号和电量
  char line4[32];
  if (data.rssi != 0 && data.batteryLevel >= 0) {
    snprintf(line4, sizeof(line4), "R:%d B:%d%%", data.rssi, data.batteryLevel);
  } else if (data.rssi != 0) {
    snprintf(line4, sizeof(line4), "RSSI:%d", data.rssi);
  } else if (data.batteryLevel >= 0) {
    snprintf(line4, sizeof(line4), "Battery:%d%%", data.batteryLevel);
  } else {
    snprintf(line4, sizeof(line4), "");
  }
  if (strlen(line4) > 0) {
    display->drawStr(2, y, line4);
  }
#endif
}

// 绘制踏频轮子动画
void DisplayManager::drawCadenceWheel(float cadence, unsigned long currentTime) {
  if (!display) return;
  
#ifdef OLED_128x64
  // 轮子位置：右下角
  const int16_t radius = 18;    // 轮子半径（放大，从14增加到18）
  const int16_t centerX = 128 - radius - 2;  // 轮子中心X坐标（右下角，留2像素边距）
  const int16_t centerY = 64 - radius - 2;   // 轮子中心Y坐标（右下角，留2像素边距）
  
  // 根据踏频计算旋转角度
  // 踏频单位是rpm（每分钟转数），转换为每秒转数，再计算角度
  // 增加速度倍数，让轮子转得更快更明显
  const float speedMultiplier = 3.0;  // 速度倍数，让视觉上更明显
  float rotationAngle = 0.0;
  if (cadence > 0) {
    // 使用时间计算旋转角度，使轮子持续旋转
    // 角度 = (当前时间毫秒 / 1000.0) * (踏频 / 60.0) * 360.0 * 速度倍数
    float seconds = currentTime / 1000.0;
    float rotations = seconds * (cadence / 60.0) * speedMultiplier;
    rotationAngle = fmod(rotations * 360.0, 360.0);
  }
  
  // 绘制轮子外圆
  display->drawCircle(centerX, centerY, radius, U8G2_DRAW_ALL);
  
  // 绘制辐条（4条，表示旋转）
  const int numSpokes = 4;
  for (int i = 0; i < numSpokes; i++) {
    float angle = (rotationAngle + i * 90.0) * M_PI / 180.0;
    // 辐条从中心到边缘
    int16_t x1 = centerX;
    int16_t y1 = centerY;
    int16_t x2 = centerX + (int16_t)(cos(angle) * (radius - 1));
    int16_t y2 = centerY + (int16_t)(sin(angle) * (radius - 1));
    display->drawLine(x1, y1, x2, y2);
  }
  
  // 绘制中心点
  display->drawDisc(centerX, centerY, 2, U8G2_DRAW_ALL);
#endif
}

// 显示调试主界面（使用模拟数据，方便调试界面布局）
void DisplayManager::showDebugDisplay(uint8_t theme) {
  if (!display) return;
  
  // 创建模拟数据用于调试
  SensorData debugData;
  debugData.speed = 25.5;      // 模拟速度 25.5 km/h
  debugData.cadence = 85;      // 模拟踏频 85 rpm
  debugData.connected = true;  // 模拟已连接状态
  debugData.batteryLevel = 75; // 模拟电量 75%
  debugData.deviceName = "CSC-Sensor"; // 模拟设备名称
  debugData.rssi = -65;        // 模拟信号强度 -65 dBm
  debugData.distance = 1.5;    // 模拟本次路程 1.5 km
  debugData.totalDistance = 150.3; // 模拟总路程 150.3 km
  debugData.averageSpeed = 22.8;   // 模拟平均速度 22.8 km/h
  debugData.rideDuration = 240;    // 模拟骑行时长 240秒（4分钟）
  
  // 使用与正常显示相同的方法显示（默认主题0，数字表盘）
  // 可以通过参数指定其他主题（0=数字表盘，1=模拟表盘，2=统计表盘）
  updateDisplay(debugData, theme);
}


