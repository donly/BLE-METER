/**
 * 显示管理类
 * 负责OLED显示屏的初始化和数据显示
 * 使用 U8g2lib 库支持中文字体显示
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>
#include <U8g2lib.h>
#include "config.h"

// 前向声明
struct SensorData;

class DisplayManager {
private:
#ifdef OLED_128x64
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C* display;
#else
  U8G2_SSD1306_128X32_NONAME_F_HW_I2C* display;
#endif
  bool initialized;
  
  void drawSpeed(float speed);
  void drawCadence(float cadence);
  void drawConnectionStatus(bool connected);
  void drawCadenceWheel(float cadence, unsigned long currentTime);  // 绘制踏频轮子动画
  void drawAnalogSpeedometer(const SensorData& data);  // 绘制模拟仪表盘
  void drawStatisticsPanel(const SensorData& data);  // 绘制数据统计表盘
  
public:
  DisplayManager();
  ~DisplayManager();
  
  bool begin();
  void clear();
  void showSplash(const char* text);
  void showStatus(const char* text);
  void showError(const char* text);
  void updateDisplay(const SensorData& data, uint8_t theme = 0);  // 添加主题参数
  // 显示调试主界面（使用模拟数据，方便调试界面布局）
  // 默认使用主题0（数字表盘），可以通过参数指定其他主题
  void showDebugDisplay(uint8_t theme = 0);
  // 关闭显示（进入低功耗），清空并关闭面板
  void powerOff();
};

#endif // DISPLAY_MANAGER_H


