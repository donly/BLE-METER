/**
 * 功耗管理类实现
 */

#include "PowerManager.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

PowerManager::PowerManager() {
  lastActivityTime = 0;
}

void PowerManager::begin() {
  lastActivityTime = millis();
  
  // 设置CPU频率
  setCpuFrequencyMhz(CPU_FREQ_MHZ);
  
  Serial.println("功耗管理初始化完成");
}

void PowerManager::enterDeepSleep(unsigned long seconds) {
  Serial.println("进入深度睡眠模式...");
  Serial.flush();  // 确保串口数据发送完成
  
  // 配置唤醒源
  if (seconds > 0) {
    // 仅在明确指定时才配置定时唤醒
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    Serial.printf("配置定时唤醒: %lu 秒后\n", seconds);
  } else {
    // 不配置默认定时唤醒，需通过 RST/外部唤醒
    Serial.println("未配置定时唤醒，需按 RST 或外部唤醒");
  }
  
  Serial.println("进入深度睡眠...");
  Serial.flush();
  delay(100);  // 短暂延迟确保串口数据发送
  
  // 进入深度睡眠
  esp_deep_sleep_start();
  // 注意：进入深度睡眠后，程序会重启，不会执行后面的代码
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


