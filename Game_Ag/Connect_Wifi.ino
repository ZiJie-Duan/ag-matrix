#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>

// 引用 WS_Matrix.cpp 中定义的 pixels 对象
extern Adafruit_NeoPixel pixels;

void ConnectToWifi() {
  // 设置为 Station 模式
  WiFi.mode(WIFI_STA);
  WiFi.begin("Loading...", "9123456789");

  // 等待连接，超时时间约10秒
  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
    delay(500);
    retry_count++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // 连接成功：屏幕纯白色闪烁三下
    for (int i = 0; i < 3; i++) {
      pixels.fill(pixels.Color(50, 50, 50)); // 白色 (亮度适中)
      pixels.show();
      delay(200);
      pixels.clear();
      pixels.show();
      delay(200);
    }

    // 请求公网 IP 地址
    HTTPClient http;
    // 使用 api.ipify.org 获取公网 IP (纯文本格式)
    http.begin("http://api.ipify.org");
    int httpCode = http.GET();

    if (httpCode > 0) {
      // 获取响应内容
      String payload = http.getString();
      payload.trim(); // 去除首尾空白字符

      if (payload.length() > 0) {
        // 获取最后一位字符
        char lastChar = payload.charAt(payload.length() - 1);
        
        // 确保是数字
        if (isDigit(lastChar)) {
          int lastDigit = lastChar - '0';
          
          // 将最后一位数字以亮起多少个 LED 灯珠表示出来
          pixels.clear();
          // 使用青色表示 IP 计数
          uint32_t color = pixels.Color(0, 50, 50); 
          
          // 即使是0也能显示吗？如果是0就不亮灯，符合"亮起多少个"的语义
          for (int i = 0; i < lastDigit; i++) {
            if (i < 64) { // 确保不超过矩阵范围
              pixels.setPixelColor(i, color);
            }
          }
          pixels.show();
          
          // 等待 3 秒
          delay(3000);
        }
      }
    } else {
      // 如果请求 IP 失败 (可选：闪烁黄色警告，或者直接跳过)
      // 这里选择闪烁一次黄色表示获取 IP 失败但 WiFi 已连接
      pixels.fill(pixels.Color(50, 50, 0)); 
      pixels.show();
      delay(500);
    }
    
    http.end();
    
    // 清除显示，准备进入默认流程
    pixels.clear();
    pixels.show();

  } else {
    // WiFi 连接失败：红色闪烁三下
    for (int i = 0; i < 3; i++) {
      pixels.fill(pixels.Color(50, 0, 0)); // 红色
      pixels.show();
      delay(200);
      pixels.clear();
      pixels.show();
      delay(200);
    }
  }
}
