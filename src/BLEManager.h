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
#include "config.h"

class BLEManager {
private:
  BLEScan* pBLEScan;
  BLEClient* pClient;
  BLERemoteCharacteristic* pCSCMeasurement;
  BLERemoteCharacteristic* pCSCControlPoint;
  
  bool deviceFound;
  BLEAdvertisedDevice* foundDevice;
  
  // 静态成员变量（用于回调函数）
  static BLEManager* instance;
  static uint8_t* cscDataBuffer;
  static size_t cscDataLength;
  
  // 回调函数
  static void notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify
  );
  
  bool connectToServer();
  
public:
  BLEManager();
  ~BLEManager();
  
  bool begin();
  bool scanAndConnect();
  bool isConnected();
  uint8_t* readCSCData();
  void disconnect();
};

#endif // BLE_MANAGER_H

