#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include "WS_QMI8658.h"
#include "WS_Matrix.h"

// 引用 WS_Matrix.cpp 中定义的 pixels 对象
extern Adafruit_NeoPixel pixels;
extern IMUdata Accel;

const float IMPACT_THRESHOLD_WIFI = 2.0;

// 绘制波纹动画辅助函数
// r, g, b: 基础颜色 (0-255)
// speed: 扩散速度 (数值越大越快)
// density: 波纹密度 (数值越大圈数越多)
// minRadius: 最小半径 (小于此半径的区域不渲染，用于模拟波纹划出)
void drawWifiRipple(uint8_t r, uint8_t g, uint8_t b, float speed, float density, float minRadius = 0.0) {
  pixels.clear();
  
  // 始终使用基于时间的连续相位，保证切换无缝
  unsigned long t = millis();
  float phase = (t / 1000.0) * speed; 
  
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      // 计算像素到中心的距离 (中心点 3.5, 3.5)
      float dx = row - 3.5;
      float dy = col - 3.5;
      float dist = sqrt(dx * dx + dy * dy);
      
      // 如果在挖空区域内，直接跳过
      if (dist < minRadius) continue;
      
      // 计算波形: sin(距离 * 密度 - 时间)
      float wave = sin(dist * density - phase);
      
      // 只渲染波峰部分 (wave > 0)，产生明显的亮暗条纹
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

// 后端交互模式循环
void BackendInteractionLoop() {
  while (true) {
    QMI8658_Loop();
    
    // 检测快速冲击（打断条件）
    float totalAccel = sqrt(Accel.x * Accel.x + Accel.y * Accel.y + (Accel.z + 1.0) * (Accel.z + 1.0));
    
    if (totalAccel > IMPACT_THRESHOLD_WIFI) {
      WiFi.disconnect();
      pixels.clear();
      pixels.show();
      return;
    }
    
    // 微弱的呼吸灯表示在线状态
    float breath = (sin(millis() / 500.0) + 1.0) * 0.2; 
    pixels.setPixelColor(0, pixels.Color((int)(0 * breath), (int)(0 * breath), (int)(150 * breath))); 
    pixels.show();
    
    delay(10);
  }
}

void ConnectToWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin("Loading...", "9123456789");

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 10000;

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime < wifiTimeout)) {
    // 播放等待动画：淡青色，规律的波纹，像是水波
    drawWifiRipple(0, 30, 30, 6.0, 1.6);
    delay(20);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // 1. 先播放 1.5 秒的连续波纹
    unsigned long successStartTime = millis();
    while (millis() - successStartTime < 1500) {
      drawWifiRipple(0, 150, 0, 12.0, 0.8, 0.0); // minRadius = 0
      delay(10);
    }
    
    // 2. 播放 0.5 秒的"划出"动画
    // 通过让 minRadius 从 0 增加到 8.0，实现中心挖空并扩散的效果
    // 由于相位公式不变，波纹位置绝对连续，无顿挫
    unsigned long fadeStartTime = millis();
    while (millis() - fadeStartTime < 500) {
      float progress = (millis() - fadeStartTime) / 500.0;
      if(progress > 1.0) progress = 1.0;
      
      float currentMinRadius = progress * 8.0; // 从 0 变到 8
      drawWifiRipple(0, 150, 0, 12.0, 0.8, currentMinRadius);
      delay(10);
    }

    // 请求公网 IP 地址
    HTTPClient http;
    http.begin("http://api.ipify.org");
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      payload.trim();

      if (payload.length() > 0) {
        char lastChar = payload.charAt(payload.length() - 1);
        if (isDigit(lastChar)) {
          int lastDigit = lastChar - '0';
          
          pixels.clear();
          uint32_t color = pixels.Color(0, 50, 50); 
          for (int i = 0; i < lastDigit; i++) {
            if (i < 64) pixels.setPixelColor(i, color);
          }
          pixels.show();
          delay(3000);
        }
      }
    } else {
      pixels.fill(pixels.Color(50, 50, 0)); 
      pixels.show();
      delay(500);
    }
    
    http.end();
    pixels.clear();
    pixels.show();
    BackendInteractionLoop();

  } else {
    // WiFi 连接失败：红色
    // 1. 播放 1.5 秒连续波纹
    unsigned long failStartTime = millis();
    while (millis() - failStartTime < 1500) {
      drawWifiRipple(150, 0, 0, 12.0, 0.8, 0.0);
      delay(10);
    }
    
    // 2. 播放 0.5 秒的"划出"动画
    unsigned long fadeStartTime = millis();
    while (millis() - fadeStartTime < 500) {
      float progress = (millis() - fadeStartTime) / 500.0;
      if(progress > 1.0) progress = 1.0;
      
      float currentMinRadius = progress * 8.0;
      drawWifiRipple(150, 0, 0, 12.0, 0.8, currentMinRadius);
      delay(10);
    }
    
    WiFi.disconnect();
    pixels.clear();
    pixels.show();
  }
}
