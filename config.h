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
// CSC Service UUID（标准UUID为0x1816，但某些设备可能使用完整UUID或其他格式）
// 支持多种格式：
// - 16位短UUID: "1816"
// - 完整UUID: "00001816-0000-1000-8000-00805f9b34fb"
// - 自定义UUID（根据实际设备修改）
#define CSC_SERVICE_UUID "1816"
#define CSC_SERVICE_UUID_FULL "00001816-0000-1000-8000-00805f9b34fb"

// CSC Measurement Characteristic UUID
#define CSC_MEASUREMENT_UUID "2A5B"
#define CSC_MEASUREMENT_UUID_FULL "00002A5B-0000-1000-8000-00805f9b34fb"

// CSC Control Point Characteristic UUID
#define CSC_CONTROL_POINT_UUID "2A55"
#define CSC_CONTROL_POINT_UUID_FULL "00002A55-0000-1000-8000-00805f9b34fb"

// 是否自动检测CSC设备（通过特征值识别，即使UUID不匹配）
// 注意：当前仅通过Service UUID识别，不通过设备名称
#define AUTO_DETECT_CSC_DEVICE false

// BLE扫描超时时间（毫秒）
#define BLE_SCAN_TIMEOUT 10000

// BLE连接超时时间（毫秒）
#define BLE_CONNECT_TIMEOUT 5000

// 快速连接超时时间（毫秒，用于连接上次保存的设备）
#define BLE_QUICK_CONNECT_TIMEOUT 3000

// ========== 功耗管理配置 ==========
// 深度睡眠唤醒时间（秒，0表示不自动唤醒）
#define DEEP_SLEEP_DURATION 0  // 0 = 通过外部唤醒（如运动检测）

// 运动检测阈值（速度变化，km/h）
#define MOTION_THRESHOLD 0.5

// 速度过滤配置
#define MAX_REASONABLE_SPEED 100.0  // 最大合理速度 (km/h)
#define MIN_TIME_DIFF 1              // 最小时间差 (1/1024秒)
#define MAX_TIME_DIFF_SEC 10.0       // 最大时间差 (秒)
#define MAX_REV_DIFF 10              // 最大转数差（单次）

// 静止检测时间（秒，超过此时间无运动则进入睡眠）
#define STATIONARY_TIME 120

// 深度睡眠唤醒配置
// 注意：深度睡眠时BOOT按钮无法唤醒，请使用RST按钮（硬件复位）唤醒
// 或者使用定时唤醒（30秒后自动唤醒检查）
// 已禁用GPIO唤醒，BOOT按钮仅用于正常运行时进入匹配模式
#define WAKEUP_GPIO -1  // -1 = 禁用GPIO唤醒，使用定时唤醒或RST按钮

// 匹配/绑定按键GPIO（用于重新绑定新设备）
// 正常运行时，长按BOOT按钮进入匹配模式
// 可以使用BOOT按钮（GPIO9）或其他GPIO
// 设置为-1禁用匹配功能
#define PAIR_BUTTON_GPIO 9  // 9 = BOOT按钮，-1 = 禁用匹配功能

// 按键检测配置
#define BUTTON_DEBOUNCE_TIME 50  // 按键防抖时间（毫秒）
#define BUTTON_PRESS_TIME 2000   // 长按时间（毫秒，用于触发匹配模式，建议2秒以上避免误触发）

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


