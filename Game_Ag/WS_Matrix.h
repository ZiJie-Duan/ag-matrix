#ifndef _WS_Matrix_H_
#define _WS_Matrix_H_
#include <Adafruit_NeoPixel.h>

#define RGB_Control_PIN   14       
#define Matrix_Row        8     
#define Matrix_Col        8       
#define RGB_COUNT         64     
#define MAX_PARTICLES     8      // 最大分裂粒子数

// 粒子结构体
struct Particle {
  float x, y;           // 位置（使用浮点数以实现平滑移动）
  float vx, vy;         // 速度
  bool active;          // 是否激活
};

// 目标点结构体（小游戏）
struct TargetPoint {
  uint8_t x, y;         // 位置
  bool active;          // 是否激活
  unsigned long spawnTime;  // 生成时间
  float brightness;     // 呼吸亮度 0.0-1.0
};

// 冲击波类型
enum RippleType {
  RIPPLE_RING = 0,      // 圆环冲击波
  RIPPLE_FIREWORK = 1,  // 烟花冲击波
  RIPPLE_RAINBOW = 2,   // 彩虹冲击波
  RIPPLE_SPIRAL = 3,    // 螺旋冲击波
  RIPPLE_PULSE = 4      // 脉冲冲击波（多层圆环）
};

// 冲击波结构体
struct Ripple {
  float centerX, centerY;  // 中心点
  float radius;            // 半径
  bool active;             // 是否激活
  unsigned long startTime; // 开始时间
  RippleType type;         // 冲击波类型
};

// 烟花粒子结构体
struct FireworkParticle {
  float x, y;              // 位置
  float vx, vy;            // 速度
  bool active;             // 是否激活
  uint8_t hue;             // 颜色（HSV色相）
};

#define MAX_FIREWORK_PARTICLES 16  // 烟花粒子数量（增加到16个）

void RGB_Matrix();                         

void Game(uint8_t X_EN,uint8_t Y_EN);
void Matrix_Init();
void TriggerSplitMode();                   // 触发分裂模式
void UpdateSplitMode();                    // 更新分裂模式
bool IsSplitModeActive();                  // 检查是否在分裂模式

// 小游戏函数
void UpdateMiniGame();                     // 更新小游戏（点收集）
void SpawnNewTarget();                     // 生成新目标点

// 游戏状态标志
extern bool miniGameEnabled;               // 小游戏是否启用
#endif
