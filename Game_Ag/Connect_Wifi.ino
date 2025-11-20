#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include "WS_QMI8658.h"
#include "WS_Matrix.h"

// 引用 WS_Matrix.cpp 中定义的 pixels 对象
extern Adafruit_NeoPixel pixels;
extern IMUdata Accel;

const float IMPACT_THRESHOLD_WIFI = 2.0;
const char* API_BASE_URL = "https://ag-drawing-board-482482693814.australia-southeast2.run.app/api";

// 定义全局画板数据 (在 WS_Matrix.h 中声明 extern)
ArtworkSlot artworks[10];
bool hasData = false;

// 绘制波纹动画辅助函数
void drawWifiRipple(uint8_t r, uint8_t g, uint8_t b, float speed, float density, float minRadius = 0.0) {
  pixels.clear();
  
  unsigned long t = millis();
  float phase = (t / 1000.0) * speed; 
  
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      float dx = row - 3.5;
      float dy = col - 3.5;
      float dist = sqrt(dx * dx + dy * dy);
      
      if (dist < minRadius) continue;
      
      float wave = sin(dist * density - phase);
      
      if (wave > 0) {
        float brightness = wave; 
        uint8_t finalR = (uint8_t)(r * brightness);
        uint8_t finalG = (uint8_t)(g * brightness);
        uint8_t finalB = (uint8_t)(b * brightness);
        
        pixels.setPixelColor(row * 8 + col, pixels.Color(finalR, finalG, finalB));
      }
    }
  }
  pixels.show();
}

// 解析 Hex 颜色 (#RRGGBB) 到 uint32_t
uint32_t parseHexColor(const char* hex) {
  if (!hex) return 0; // null is black
  if (hex[0] == '#') hex++;
  
  long number = strtol(hex, NULL, 16);
  uint8_t r = (number >> 16) & 0xFF;
  uint8_t g = (number >> 8) & 0xFF;
  uint8_t b = number & 0xFF;
  
  return pixels.Color(r, g, b);
}

// 判断一个 Slot 是否为空（全黑）
bool isSlotEmpty(const ArtworkSlot& slot) {
  for (int i = 0; i < 64; i++) {
    if (slot.grid[i] != 0) return false;
  }
  return true;
}

// 执行同步逻辑
void performSync() {
  WiFiClientSecure client;
  client.setInsecure(); // 忽略 SSL 证书验证
  HTTPClient http;
  
  int webVersion = 0;
  
  // 1. Get Sync Status
  Serial.println("Checking sync status...");
  if (http.begin(client, String(API_BASE_URL) + "/sync")) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      webVersion = doc["webVersion"];
    }
    http.end();
  }

  // 2. Get All Slots Data
  Serial.println("Downloading slots...");
  pixels.fill(pixels.Color(0, 0, 50)); // 蓝色加载动画
  pixels.show();

  if (http.begin(client, String(API_BASE_URL) + "/slots")) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(20480); 
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        JsonArray slots = doc.as<JsonArray>();
        int slotIdx = 0;
        for (JsonObject slot : slots) {
          if (slotIdx >= 10) break;
          
          JsonArray grid = slot["grid"];
          int pixelIdx = 0;
          for (int r = 0; r < 8; r++) {
            JsonArray row = grid[r];
            for (int c = 0; c < 8; c++) {
              if (pixelIdx < 64) {
                const char* colorStr = row[c];
                artworks[slotIdx].grid[pixelIdx] = parseHexColor(colorStr);
              }
              pixelIdx++;
            }
          }
          // 检查并标记是否为空
          artworks[slotIdx].isEmpty = isSlotEmpty(artworks[slotIdx]);
          slotIdx++;
        }
        hasData = true;
        pixels.fill(pixels.Color(0, 50, 0)); // 绿色成功
        pixels.show();
        delay(500);
      } else {
        Serial.println("JSON parsing failed");
        pixels.fill(pixels.Color(50, 0, 0)); // 红色失败
        pixels.show();
        delay(500);
      }
    }
    http.end();
  }

  // 3. Update Device Sync Version
  if (hasData) {
    Serial.println("Updating sync version...");
    if (http.begin(client, String(API_BASE_URL) + "/sync")) {
      http.addHeader("Content-Type", "application/json");
      String requestBody = "{\"deviceVersion\":" + String(webVersion) + "}";
      http.POST(requestBody);
      http.end();
    }
  }
}

void displayArtwork(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= 10) return;
  
  // 如果该位置为空且有数据，为了防止黑屏，我们尝试找到最近的一个非空位置
  // 但在这个函数里我们只负责绘制指定的 slot，逻辑控制由调用者负责
  
  for (int i = 0; i < 64; i++) {
    int r = i / 8;
    int c = i % 8;
    
    // 逆时针旋转90度映射
    int src_r = c;
    int src_c = 7 - r;
    int src_idx = src_r * 8 + src_c;
    
    if (src_idx >= 0 && src_idx < 64) {
        pixels.setPixelColor(i, artworks[slotIndex].grid[src_idx]);
    }
  }
  pixels.show();
}

// 获取下一个有效的非空 Slot 索引
int getNextValidSlot(int current, int direction) {
  int next = current;
  int attempts = 0;
  do {
    next = next + direction;
    if (next > 9) next = 0;
    if (next < 0) next = 9;
    attempts++;
    // 如果找到了非空的，或者所有都遍历完了（全空），就停止
  } while (artworks[next].isEmpty && attempts < 10);
  
  return next;
}

// 通用的画廊展示循环（处理交互和显示），供 Wifi模式 和 离线模式 复用
// isWifiMode: true 表示 Wifi连接状态，false 表示离线模式
void GalleryInteractionLoop(bool isWifiMode) {
  if (!hasData) {
    pixels.fill(pixels.Color(20, 0, 0)); // 无数据红色错误
    pixels.show();
    delay(1000);
    return;
  }

  // 找到第一个非空的作品
  int currentSlot = 0;
  if (artworks[currentSlot].isEmpty) {
    currentSlot = getNextValidSlot(currentSlot, 1);
  }
  
  displayArtwork(currentSlot);

  unsigned long lastTiltTime = 0;
  const unsigned long TILT_COOLDOWN = 500; 
  const float TILT_THRESHOLD = 0.3;

  while (true) {
    QMI8658_Loop();
    
    // 检测快速冲击（退出条件）
    float totalAccel = sqrt(Accel.x * Accel.x + Accel.y * Accel.y + (Accel.z + 1.0) * (Accel.z + 1.0));
    
    if (totalAccel > IMPACT_THRESHOLD_WIFI) {
      if (isWifiMode) {
        WiFi.disconnect();
      }
      pixels.clear();
      pixels.show();
      return; 
    }
    
    // 倾斜切换逻辑 (改为 X 轴)
    if (millis() - lastTiltTime > TILT_COOLDOWN) {
      // Accel.x > 阈值 => 向下倾斜 (或根据实际方向) => 下一张
      // Accel.x < -阈值 => 向上倾斜 => 上一张
      
      if (Accel.x > TILT_THRESHOLD) {
        // 下一张
        currentSlot = getNextValidSlot(currentSlot, 1);
        displayArtwork(currentSlot);
        lastTiltTime = millis();
      } else if (Accel.x < -TILT_THRESHOLD) {
        // 上一张
        currentSlot = getNextValidSlot(currentSlot, -1);
        displayArtwork(currentSlot);
        lastTiltTime = millis();
      }
    }
    
    delay(10);
  }
}

void BackendInteractionLoop() {
  GalleryInteractionLoop(true);
}

void OfflineGalleryLoop() {
  GalleryInteractionLoop(false);
}

void ConnectToWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin("Loading...", "9123456789");

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 10000;

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime < wifiTimeout)) {
    drawWifiRipple(0, 30, 30, 6.0, 1.6);
    delay(20);
  }

  if (WiFi.status() == WL_CONNECTED) {
    unsigned long successStartTime = millis();
    while (millis() - successStartTime < 1000) {
      drawWifiRipple(0, 150, 0, 12.0, 0.8, 0.0); 
      delay(10);
    }
    
    pixels.clear();
    pixels.show();

    performSync();
    
    BackendInteractionLoop();

  } else {
    unsigned long failStartTime = millis();
    while (millis() - failStartTime < 1500) {
      drawWifiRipple(150, 0, 0, 12.0, 0.8, 0.0);
      delay(10);
    }
    
    WiFi.disconnect();
    pixels.clear();
    pixels.show();
  }
}
