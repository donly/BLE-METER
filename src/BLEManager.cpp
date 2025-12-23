/**
 * BLE连接管理类实现
 */

#include "BLEManager.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <WString.h>

// 静态成员变量定义
BLEManager* BLEManager::instance = nullptr;
uint8_t* BLEManager::cscDataBuffer = nullptr;
size_t BLEManager::cscDataLength = 0;
size_t BLEManager::lastReadDataLength = 0;

BLEManager::BLEManager() {
  pBLEScan = nullptr;
  pClient = nullptr;
  pCSCMeasurement = nullptr;
  pCSCControlPoint = nullptr;
  pBatteryLevel = nullptr;
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
  
  // 初始化Preferences用于保存设备地址
  preferences.begin("ble_meter", false);
  
  return true;
}

bool BLEManager::scanAndConnect() {
  deviceFound = false;
  if (foundDevice) {
    delete foundDevice;
    foundDevice = nullptr;
  }
  
  // 首先尝试连接上次保存的设备（快速连接）
  // 减少输出频率，避免刷屏
  static unsigned long lastConnectAttempt = 0;
  static unsigned long lastFailMessage = 0;
  bool shouldOutput = (millis() - lastConnectAttempt > 5000);  // 每5秒输出一次
  
  if (shouldOutput) {
    Serial.println("尝试连接上次的设备...");
    lastConnectAttempt = millis();
  }
  
  if (connectToLastDevice()) {
    Serial.println("✓ 快速连接到上次的设备成功！");
    lastFailMessage = 0;  // 重置失败消息计时
    return true;
  }
  
  // 快速连接失败，不自动扫描
  // 减少失败消息的输出频率
  if (shouldOutput && (millis() - lastFailMessage > 10000)) {  // 每10秒输出一次失败消息
    Serial.println("上次的设备不可用");
    Serial.println("提示: 长按匹配按键（BOOT按钮）进入匹配模式");
    lastFailMessage = millis();
  }
  return false;
}

bool BLEManager::scanAndConnectForced() {
  deviceFound = false;
  if (foundDevice) {
    delete foundDevice;
    foundDevice = nullptr;
  }
  
  Serial.println("=== 进入匹配模式 ===");
  Serial.println("开始扫描CSC传感器...");
  
  // 开始扫描（返回指针）
  BLEScanResults* foundDevices = pBLEScan->start(BLE_SCAN_TIMEOUT / 1000, false);
  
  if (foundDevices == nullptr) {
    Serial.println("扫描失败");
    return false;
  }
  
  Serial.printf("扫描到 %d 个设备\n", foundDevices->getCount());
  
  // 获取上次保存的设备地址（优先连接）
  String lastAddress = loadLastDeviceAddress();
  bool foundLastDevice = false;
  
  // 查找CSC设备
  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    
    // 打印设备信息用于调试
    Serial.printf("设备 %d: 地址=%s", i, device.getAddress().toString().c_str());
    if (device.haveName()) {
      Serial.printf(", 名称=%s", device.getName().c_str());
    }
    Serial.println();
    
    // 检查是否为CSC设备
    if (isCSCDevice(device)) {
      // 如果是上次的设备，优先连接
      if (lastAddress.length() > 0 && device.getAddress().toString() == lastAddress) {
        Serial.println("找到上次连接的CSC传感器，优先连接！");
        foundDevice = new BLEAdvertisedDevice(device);
        deviceFound = true;
        foundLastDevice = true;
        break;
      }
      
      // 如果是新设备，先保存，等遍历完再决定
      if (!foundLastDevice) {
        Serial.println("找到CSC传感器！");
        foundDevice = new BLEAdvertisedDevice(device);
        deviceFound = true;
      }
    }
  }
  
  pBLEScan->clearResults();
  
  if (deviceFound && foundDevice) {
    bool connected = connectToServer();
    if (connected) {
      // 连接成功，保存设备地址
      saveLastDeviceAddress(foundDevice->getAddress());
      Serial.println("=== 匹配成功，已保存设备地址 ===");
    }
    return connected;
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
    
    // 尝试获取CSC服务（先尝试标准UUID，再尝试完整UUID）
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(CSC_SERVICE_UUID));
    if (pRemoteService == nullptr) {
      Serial.println("尝试使用完整UUID...");
      pRemoteService = pClient->getService(BLEUUID(CSC_SERVICE_UUID_FULL));
    }
    
    // 如果还是找不到，尝试遍历所有服务查找CSC特征值
    if (pRemoteService == nullptr && AUTO_DETECT_CSC_DEVICE) {
      Serial.println("遍历所有服务查找CSC特征值...");
      // 注意：ESP32 BLE库的getServices()可能返回不同的类型
      // 这里先注释掉，如果标准UUID都找不到，可以手动指定服务UUID
      // 或者通过设备名称等其他方式识别
    }
    
    if (pRemoteService == nullptr) {
      Serial.println("未找到CSC服务");
      pClient->disconnect();
      return false;
    }
    
    // 获取Measurement特征值（尝试多种UUID格式）
    pCSCMeasurement = pRemoteService->getCharacteristic(BLEUUID(CSC_MEASUREMENT_UUID));
    if (pCSCMeasurement == nullptr) {
      Serial.println("尝试使用完整UUID获取Measurement特征值...");
      pCSCMeasurement = pRemoteService->getCharacteristic(BLEUUID(CSC_MEASUREMENT_UUID_FULL));
    }
    
    if (pCSCMeasurement == nullptr) {
      Serial.println("未找到Measurement特征值");
      pClient->disconnect();
      return false;
    }
    
    // 订阅通知
    if (pCSCMeasurement->canNotify()) {
      pCSCMeasurement->registerForNotify(notifyCallback);
      Serial.println("已订阅CSC Measurement通知");
    } else {
      Serial.println("警告: CSC Measurement不支持通知，将使用轮询方式读取");
    }
    
    // 获取Control Point特征值（可选）
    pCSCControlPoint = pRemoteService->getCharacteristic(BLEUUID(CSC_CONTROL_POINT_UUID));
    
    // 尝试获取电池服务（Battery Service, UUID: 0x180F）
    BLERemoteService* pBatteryService = pClient->getService(BLEUUID((uint16_t)0x180F));
    if (pBatteryService != nullptr) {
      // 获取电池电量特征值 (Battery Level, UUID: 0x2A19)
      pBatteryLevel = pBatteryService->getCharacteristic(BLEUUID((uint16_t)0x2A19));
      if (pBatteryLevel != nullptr) {
        Serial.println("找到电池服务，可以读取电量");
      } else {
        Serial.println("未找到电池电量特征值");
        pBatteryLevel = nullptr;
      }
    } else {
      Serial.println("设备不支持电池服务");
      pBatteryLevel = nullptr;
    }
    
    // 连接成功，保存设备地址
    saveLastDeviceAddress(foundDevice->getAddress());
    
    Serial.println("CSC服务连接成功，等待数据...");
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
    Serial.printf("[通知] 收到CSC数据，长度: %d 字节\n", length);
    Serial.print("[原始数据] ");
    for (size_t i = 0; i < length; i++) {
      Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
    
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
      lastReadDataLength = cscDataLength;
      free(cscDataBuffer);
      cscDataBuffer = nullptr;
      cscDataLength = 0;
      return data;
    }
  }
  
  // 否则尝试读取（轮询方式，某些设备可能不支持通知）
  // 注意：频繁读取可能影响性能，建议使用通知方式
  static unsigned long lastPollTime = 0;
  if (millis() - lastPollTime > 1000) {  // 每秒轮询一次
    String value = pCSCMeasurement->readValue();
    if (value.length() > 0) {
      Serial.printf("[轮询] 读取到CSC数据，长度: %d 字节\n", value.length());
      Serial.print("[原始数据] ");
      for (size_t i = 0; i < value.length(); i++) {
        Serial.printf("%02X ", (uint8_t)value[i]);
      }
      Serial.println();
      
      uint8_t* data = (uint8_t*)malloc(value.length());
      if (data) {
        memcpy(data, value.c_str(), value.length());
        lastReadDataLength = value.length();
        lastPollTime = millis();
        return data;
      }
    }
    lastPollTime = millis();
  }
  
  lastReadDataLength = 0;
  return nullptr;
}

size_t BLEManager::getLastDataLength() {
  return lastReadDataLength;
}

int8_t BLEManager::readBatteryLevel() {
  if (!isConnected() || !pBatteryLevel) {
    return -1;  // 未连接或设备不支持电池服务
  }
  
  try {
    String value = pBatteryLevel->readValue();
    if (value.length() > 0) {
      uint8_t level = (uint8_t)value[0];
      if (level > 100) {
        level = 100;  // 确保不超过100
      }
      return (int8_t)level;
    }
  } catch (...) {
    Serial.println("读取电池电量失败");
  }
  
  return -1;  // 读取失败
}

String BLEManager::getDeviceName() {
  if (foundDevice && foundDevice->haveName()) {
    return String(foundDevice->getName().c_str());
  }
  // 如果没有名称，返回设备地址
  if (foundDevice) {
    return String(foundDevice->getAddress().toString().c_str());
  }
  return String("");
}

int8_t BLEManager::getRSSI() {
  if (!isConnected()) {
    return 0;  // 未连接
  }
  
  // 尝试从 foundDevice 获取 RSSI（扫描时的值）
  if (foundDevice) {
    return foundDevice->getRSSI();
  }
  
  // 如果连接后需要实时 RSSI，可以通过 pClient 获取
  // 但 ESP32 BLE 库可能不直接支持，所以返回扫描时的值
  return 0;
}

void BLEManager::disconnect() {
  if (pClient && pClient->isConnected()) {
    pClient->disconnect();
  }
}

void BLEManager::clearLastDevice() {
  preferences.remove("last_device");
  Serial.println("已清除保存的设备地址");
}

void BLEManager::saveLastDeviceAddress(BLEAddress address) {
  String addrStr = address.toString();
  preferences.putString("last_device", addrStr);
  Serial.printf("已保存设备地址: %s\n", addrStr.c_str());
}

String BLEManager::loadLastDeviceAddress() {
  String addr = preferences.getString("last_device", "");
  if (addr.length() > 0) {
    Serial.printf("读取到上次的设备地址: %s\n", addr.c_str());
  }
  return addr;
}

bool BLEManager::connectToLastDevice() {
  String lastAddress = loadLastDeviceAddress();
  if (lastAddress.length() == 0) {
    return false;  // 没有保存的设备地址
  }
  
  Serial.printf("尝试快速连接到上次的设备: %s\n", lastAddress.c_str());
  
  // 使用保存的地址创建BLE地址对象
  BLEAddress addr(lastAddress.c_str());
  
  // 尝试直接连接（不扫描）
  if (pClient->connect(addr)) {
    Serial.println("快速连接成功！");
    
    // 验证是否为CSC设备
    // 尝试获取CSC服务
    BLERemoteService* pRemoteService = pClient->getService(BLEUUID(CSC_SERVICE_UUID));
    if (pRemoteService == nullptr) {
      pRemoteService = pClient->getService(BLEUUID(CSC_SERVICE_UUID_FULL));
    }
    
    if (pRemoteService == nullptr) {
      Serial.println("快速连接成功，但不是CSC设备，断开连接");
      pClient->disconnect();
      return false;
    }
    
    // 获取Measurement特征值
    pCSCMeasurement = pRemoteService->getCharacteristic(BLEUUID(CSC_MEASUREMENT_UUID));
    if (pCSCMeasurement == nullptr) {
      pCSCMeasurement = pRemoteService->getCharacteristic(BLEUUID(CSC_MEASUREMENT_UUID_FULL));
    }
    
    if (pCSCMeasurement == nullptr) {
      Serial.println("快速连接成功，但未找到Measurement特征值，断开连接");
      pClient->disconnect();
      return false;
    }
    
    // 订阅通知
    if (pCSCMeasurement->canNotify()) {
      pCSCMeasurement->registerForNotify(notifyCallback);
      Serial.println("已订阅CSC Measurement通知");
    }
    
    // 获取Control Point特征值（可选）
    pCSCControlPoint = pRemoteService->getCharacteristic(BLEUUID(CSC_CONTROL_POINT_UUID));
    
    Serial.println("快速连接并验证成功！");
    return true;
  } else {
    Serial.println("快速连接失败，设备可能不在范围内");
    return false;
  }
}

bool BLEManager::isCSCDevice(BLEAdvertisedDevice device) {
  // 仅通过Service UUID识别CSC设备
  if (device.haveServiceUUID()) {
    BLEUUID serviceUUID = device.getServiceUUID();
    String serviceUUIDStr = serviceUUID.toString();
    
    // 转换为小写进行比较（UUID格式可能不同）
    serviceUUIDStr.toLowerCase();
    String targetUUID = String(CSC_SERVICE_UUID);
    targetUUID.toLowerCase();
    String targetUUIDFull = String(CSC_SERVICE_UUID_FULL);
    targetUUIDFull.toLowerCase();
    
    // 检查是否匹配标准UUID（16位或完整格式）
    // 也检查UUID中是否包含"1816"（CSC标准UUID的核心部分）
    if (serviceUUIDStr == targetUUID || 
        serviceUUIDStr == targetUUIDFull ||
        serviceUUIDStr.indexOf("1816") >= 0) {
      Serial.printf("  -> ✓ 匹配CSC Service UUID: %s\n", serviceUUIDStr.c_str());
      return true;
    }
    
    // 打印所有服务UUID以便调试
    Serial.printf("  -> Service UUID: %s (不匹配CSC UUID)\n", serviceUUIDStr.c_str());
  } else {
    Serial.println("  -> 设备未广播Service UUID");
  }
  
  return false;
}

bool BLEManager::checkCSCService(BLERemoteService* service) {
  if (service == nullptr) {
    return false;
  }
  
  // 检查是否包含CSC Measurement特征值
  BLERemoteCharacteristic* pChar = service->getCharacteristic(BLEUUID(CSC_MEASUREMENT_UUID));
  if (pChar == nullptr) {
    pChar = service->getCharacteristic(BLEUUID(CSC_MEASUREMENT_UUID_FULL));
  }
  
  if (pChar != nullptr) {
    Serial.println("  -> 找到CSC Measurement特征值");
    return true;
  }
  
  return false;
}

