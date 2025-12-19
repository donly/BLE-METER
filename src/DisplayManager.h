/**
 * 显示管理类
 * 负责OLED显示屏的初始化和数据显示
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "config.h"

// 前向声明
struct SensorData;

class DisplayManager {
private:
  Adafruit_SSD1306* display;
  bool initialized;
  
  void drawSpeed(float speed);
  void drawCadence(float cadence);
  void drawConnectionStatus(bool connected);
  
public:
  DisplayManager();
  ~DisplayManager();
  
  bool begin();
  void clear();
  void showSplash(const char* text);
  void showStatus(const char* text);
  void showError(const char* text);
  void updateDisplay(const SensorData& data);
};

#endif // DISPLAY_MANAGER_H

