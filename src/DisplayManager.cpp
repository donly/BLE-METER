/**
 * 显示管理类实现
 */

#include "DisplayManager.h"

// 前向声明结构体（在ble_meter.ino中定义）
struct SensorData {
  float speed;
  float cadence;
  bool connected;
};

DisplayManager::DisplayManager() {
  display = nullptr;
  initialized = false;
}

DisplayManager::~DisplayManager() {
  if (display) {
    delete display;
  }
}

bool DisplayManager::begin() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  
#ifdef OLED_128x64
  display = new Adafruit_SSD1306(128, 64, &Wire, -1);
#else
  display = new Adafruit_SSD1306(128, 32, &Wire, -1);
#endif

  if (!display->begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    Serial.println("SSD1306分配失败");
    return false;
  }
  
  display->clearDisplay();
  display->display();
  initialized = true;
  
  Serial.println("OLED显示初始化成功");
  return true;
}

void DisplayManager::clear() {
  if (display) {
    display->clearDisplay();
    display->display();
  }
}

void DisplayManager::showSplash(const char* text) {
  if (!display) return;
  
  display->clearDisplay();
  display->setTextSize(2);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(10, 20);
  display->println(text);
  display->display();
}

void DisplayManager::showStatus(const char* text) {
  if (!display) return;
  
  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0, 0);
  display->println("状态:");
  display->setTextSize(2);
  display->setCursor(0, 15);
  display->println(text);
  display->display();
}

void DisplayManager::showError(const char* text) {
  if (!display) return;
  
  display->clearDisplay();
  display->setTextSize(1);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0, 0);
  display->println("错误:");
  display->setTextSize(1);
  display->setCursor(0, 15);
  display->println(text);
  display->display();
}

void DisplayManager::updateDisplay(const SensorData& data) {
  if (!display) return;
  
  display->clearDisplay();
  
  // 显示速度
  display->setTextSize(3);
  display->setTextColor(SSD1306_WHITE);
  display->setCursor(0, 0);
  display->printf("%.1f", data.speed);
  
  // 显示单位
  display->setTextSize(1);
  display->setCursor(85, 5);
  display->println("km/h");
  
  // 显示踏频
  display->setTextSize(2);
  display->setCursor(0, 30);
  display->print("踏频: ");
  display->printf("%.0f", data.cadence);
  display->println(" rpm");
  
  // 显示连接状态
  display->setTextSize(1);
  display->setCursor(0, 50);
  if (data.connected) {
    display->print("已连接");
  } else {
    display->print("未连接");
  }
  
  display->display();
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

