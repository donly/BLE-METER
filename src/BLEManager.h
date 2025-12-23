/**
 * BLE连接管理类
 * 负责BLE扫描、连接CSC传感器和数据读取
 */

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include "config.h"

class BLEManager {
private:
  BLEScan* pBLEScan;
  BLEClient* pClient;
  BLERemoteCharacteristic* pCSCMeasurement;
  BLERemoteCharacteristic* pCSCControlPoint;
  BLERemoteCharacteristic* pBatteryLevel;  // 电池电量特征值
  
  bool deviceFound;
  BLEAdvertisedDevice* foundDevice;
  
  // 静态成员变量（用于回调函数）
  static BLEManager* instance;
  static uint8_t* cscDataBuffer;
  static size_t cscDataLength;
  static size_t lastReadDataLength;
  
  // 回调函数
  static void notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify
  );
  
  bool connectToServer();
  bool isCSCDevice(BLEAdvertisedDevice device);
  bool checkCSCService(BLERemoteService* service);
  
  // 设备记忆功能
  void saveLastDeviceAddress(BLEAddress address);
  String loadLastDeviceAddress();
  bool connectToLastDevice();
  
  Preferences preferences;
  
public:
  BLEManager();
  ~BLEManager();
  
  bool begin();
  bool scanAndConnect();
  bool scanAndConnectForced();  // 强制扫描（用于匹配模式）
  bool isConnected();
  uint8_t* readCSCData();
  size_t getLastDataLength();
  int8_t readBatteryLevel();  // 读取电池电量 (0-100, -1表示未获取)
  String getDeviceName();     // 获取设备名称
  int8_t getRSSI();           // 获取信号强度 (dBm)
  void disconnect();
  void clearLastDevice();  // 清除保存的设备地址
};

#endif // BLE_MANAGER_H

