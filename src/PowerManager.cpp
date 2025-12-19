/**
 * 功耗管理类实现
 */

#include "PowerManager.h"
#include <esp_pm.h>
#include <esp_wifi.h>

PowerManager::PowerManager() {
  lastActivityTime = 0;
}

void PowerManager::begin() {
  lastActivityTime = millis();
  
  // 配置功耗管理
  esp_pm_config_esp32c3_t pm_config = {
    .max_freq_mhz = CPU_FREQ_MHZ,
    .min_freq_mhz = 10,
    .light_sleep_enable = true
  };
  
  // 注意：ESP32 C3的功耗管理API可能不同，需要根据实际SDK版本调整
  // esp_pm_configure(&pm_config);
  
  Serial.println("功耗管理初始化完成");
}

void PowerManager::enterDeepSleep(unsigned long seconds) {
  Serial.println("进入深度睡眠模式...");
  
  // 配置唤醒源（如果需要）
  if (seconds > 0) {
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  } else {
    // 使用外部唤醒（如GPIO）
    // esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  }
  
  // 进入深度睡眠
  esp_deep_sleep_start();
}

void PowerManager::setCpuFrequency(uint32_t freq) {
  setCpuFrequencyMhz(freq);
  Serial.printf("CPU频率设置为: %d MHz\n", getCpuFrequencyMhz());
}

void PowerManager::updateActivity() {
  lastActivityTime = millis();
}

unsigned long PowerManager::getInactiveTime() {
  return millis() - lastActivityTime;
}

