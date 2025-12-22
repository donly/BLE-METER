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
    // 定时唤醒（如果明确指定了时间）
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    Serial.printf("配置定时唤醒: %lu 秒后\n", seconds);
  } else {
    // 使用定时唤醒（默认30秒后自动唤醒检查）
    // 注意：深度睡眠时BOOT按钮无法唤醒，请使用RST按钮（硬件复位）唤醒
    esp_sleep_enable_timer_wakeup(30 * 1000000ULL);
    Serial.println("配置定时唤醒: 30秒后（自动检查）");
    Serial.println("提示: 深度睡眠时请使用RST按钮唤醒设备");
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


