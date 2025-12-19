/**
 * 硬件和功能配置文件
 * 根据实际硬件修改以下配置
 */

#ifndef CONFIG_H
#define CONFIG_H

// ========== OLED显示屏配置 ==========
// 显示屏尺寸选择（取消注释其中一个）
#define OLED_128x64  // 128x64像素
// #define OLED_128x32  // 128x32像素

// I2C地址（通常为0x3C或0x3D）
#define OLED_I2C_ADDRESS 0x3C

// I2C引脚定义（ESP32 C3默认）
#define OLED_SDA_PIN 6   // 默认SDA
#define OLED_SCL_PIN 7   // 默认SCL

// ========== BLE配置 ==========
// CSC Service UUID
#define CSC_SERVICE_UUID "1816"

// CSC Measurement Characteristic UUID
#define CSC_MEASUREMENT_UUID "2A5B"

// CSC Control Point Characteristic UUID
#define CSC_CONTROL_POINT_UUID "2A55"

// BLE扫描超时时间（毫秒）
#define BLE_SCAN_TIMEOUT 10000

// BLE连接超时时间（毫秒）
#define BLE_CONNECT_TIMEOUT 5000

// ========== 功耗管理配置 ==========
// 深度睡眠唤醒时间（秒，0表示不自动唤醒）
#define DEEP_SLEEP_DURATION 0  // 0 = 通过外部唤醒（如运动检测）

// 运动检测阈值（速度变化，km/h）
#define MOTION_THRESHOLD 0.5

// 静止检测时间（秒，超过此时间无运动则进入睡眠）
#define STATIONARY_TIME 60

// CPU频率（MHz，降低可节省功耗）
// 可选值: 80, 160
#define CPU_FREQ_MHZ 80

// ========== 显示配置 ==========
// 显示刷新间隔（毫秒）
#define DISPLAY_REFRESH_INTERVAL 500

// 是否显示调试信息
#define DEBUG_MODE true

// 串口波特率
#define SERIAL_BAUD 115200

// ========== 传感器配置 ==========
// 轮子周长（毫米，用于计算速度）
// 常见值：
// 700x23C: 2096mm
// 700x25C: 2105mm
// 700x28C: 2114mm
// 26寸山地车: 2050mm
// 根据实际轮胎调整
#define WHEEL_CIRCUMFERENCE_MM 2100

// ========== 电池监控配置（可选） ==========
// 是否启用电池监控
#define ENABLE_BATTERY_MONITOR false

// 电池电压检测引脚（如果使用）
#define BATTERY_PIN A0

// 电池满电电压（V）
#define BATTERY_FULL_VOLTAGE 4.2

// 电池低电电压（V）
#define BATTERY_LOW_VOLTAGE 3.3

#endif // CONFIG_H

