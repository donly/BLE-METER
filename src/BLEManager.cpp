/**
 * BLE连接管理类实现
 */

#include "BLEManager.h"

// 静态成员变量定义
BLEManager* BLEManager::instance = nullptr;
uint8_t* BLEManager::cscDataBuffer = nullptr;
size_t BLEManager::cscDataLength = 0;

BLEManager::BLEManager() {
  pBLEScan = nullptr;
  pClient = nullptr;
  pCSCMeasurement = nullptr;
  pCSCControlPoint = nullptr;
  deviceFound = false;
  foundDevice = nullptr;
  instance = this;
  cscDataBuffer = nullptr;
  cscDataLength = 0;
}

BLEManager::~BLEManager() {
  disconnect();
  if (pBLEScan) {
    pBLEScan->stop();
    delete pBLEScan;
  }
  if (pClient) {
    delete pClient;
  }
}

bool BLEManager::begin() {
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  pClient = BLEDevice::createClient();
  
  return true;
}

bool BLEManager::scanAndConnect() {
  deviceFound = false;
  if (foundDevice) {
    delete foundDevice;
    foundDevice = nullptr;
  }
  
  Serial.println("开始扫描CSC传感器...");
  
  // 开始扫描
  BLEScanResults foundDevices = pBLEScan->start(BLE_SCAN_TIMEOUT / 1000, false);
  
  Serial.printf("扫描到 %d 个设备\n", foundDevices.getCount());
  
  // 查找CSC设备
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    if (device.haveServiceUUID()) {
      BLEUUID serviceUUID = device.getServiceUUID();
      String serviceUUIDStr = serviceUUID.toString();
      Serial.printf("设备 %d: Service UUID = %s\n", i, serviceUUIDStr.c_str());
      
      if (serviceUUIDStr == CSC_SERVICE_UUID) {
        Serial.println("找到CSC传感器！");
        foundDevice = new BLEAdvertisedDevice(device);
        deviceFound = true;
        break;
      }
    }
  }
  
  pBLEScan->clearResults();
  
  if (deviceFound && foundDevice) {
    return connectToServer();
  }
  
  Serial.println("未找到CSC传感器");
  return false;
}

bool BLEManager::connectToServer() {
  if (!foundDevice) {
    return false;
  }
  
  Serial.print("连接到设备: ");
  Serial.println(foundDevice->getAddress().toString().c_str());
  
  if (pClient->connect(foundDevice)) {
    Serial.println("已连接到服务器");
    
    // 获取CSC服务
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(CSC_SERVICE_UUID));
    if (pRemoteService == nullptr) {
      Serial.println("未找到CSC服务");
      pClient->disconnect();
      return false;
    }
    
    // 获取Measurement特征值
    pCSCMeasurement = pRemoteService->getCharacteristic(BLEUUID(CSC_MEASUREMENT_UUID));
    if (pCSCMeasurement == nullptr) {
      Serial.println("未找到Measurement特征值");
      pClient->disconnect();
      return false;
    }
    
    // 订阅通知
    if (pCSCMeasurement->canNotify()) {
      pCSCMeasurement->registerForNotify(notifyCallback);
    }
    
    // 获取Control Point特征值（可选）
    pCSCControlPoint = pRemoteService->getCharacteristic(BLEUUID(CSC_CONTROL_POINT_UUID));
    
    Serial.println("CSC服务连接成功");
    return true;
  }
  
  Serial.println("连接失败");
  return false;
}

void BLEManager::notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  if (instance && length > 0) {
    // 分配缓冲区并复制数据
    if (instance->cscDataBuffer) {
      free(instance->cscDataBuffer);
    }
    instance->cscDataBuffer = (uint8_t*)malloc(length);
    if (instance->cscDataBuffer) {
      memcpy(instance->cscDataBuffer, pData, length);
      instance->cscDataLength = length;
    }
  }
}

bool BLEManager::isConnected() {
  return pClient && pClient->isConnected();
}

uint8_t* BLEManager::readCSCData() {
  if (!isConnected() || !pCSCMeasurement) {
    return nullptr;
  }
  
  // 如果有通知数据，返回它
  if (cscDataBuffer && cscDataLength > 0) {
    uint8_t* data = (uint8_t*)malloc(cscDataLength);
    if (data) {
      memcpy(data, cscDataBuffer, cscDataLength);
      free(cscDataBuffer);
      cscDataBuffer = nullptr;
      cscDataLength = 0;
      return data;
    }
  }
  
  // 否则尝试读取
  std::string value = pCSCMeasurement->readValue();
  if (value.length() > 0) {
    uint8_t* data = (uint8_t*)malloc(value.length());
    if (data) {
      memcpy(data, value.data(), value.length());
      return data;
    }
  }
  
  return nullptr;
}

void BLEManager::disconnect() {
  if (pClient && pClient->isConnected()) {
    pClient->disconnect();
  }
}

