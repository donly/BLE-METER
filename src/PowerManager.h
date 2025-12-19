/**
 * 功耗管理类
 * 负责低功耗模式管理和CPU频率控制
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <esp_sleep.h>
#include "config.h"

class PowerManager {
private:
  unsigned long lastActivityTime;
  
public:
  PowerManager();
  
  void begin();
  void enterDeepSleep(unsigned long seconds = 0);
  void setCpuFrequency(uint32_t freq);
  void updateActivity();
  unsigned long getInactiveTime();
};

#endif // POWER_MANAGER_H

