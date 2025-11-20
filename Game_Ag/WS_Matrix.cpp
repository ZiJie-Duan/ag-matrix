#include "WS_Matrix.h"
#include "WS_QMI8658.h"
// English: Please note that the brightness of the lamp bead should not be too high, which can easily cause the temperature of the board to rise rapidly, thus damaging the board !!!
// Chinese: 请注意，灯珠亮度不要太高，容易导板子温度急速上升，从而损坏板子!!! 
uint8_t RGB_Data[3] = {30,30,30}; 
uint8_t Matrix_Data[8][8];  
Adafruit_NeoPixel pixels(RGB_COUNT, RGB_Control_PIN, NEO_RGB + NEO_KHZ800);

// 引入加速度计数据
extern IMUdata Accel;

// 分裂模式相关变量
Particle particles[MAX_PARTICLES];
bool splitModeActive = false;
unsigned long splitModeStartTime = 0;
unsigned long lastImpactTime = 0;               // 最后一次检测到冲击的时间
bool gatheringStarted = false;                  // 是否已开始聚拢阶段
unsigned long gatherStartTime = 0;              // 聚拢开始时间
int targetParticleIndex = 0;                    // 聚拢目标粒子的索引
bool splitTriggeredFromGame = false;            // 记录分裂是否从游戏中触发
const unsigned long SPLIT_PHASE_TIME = 2000;    // 分裂飞散阶段：2秒
const unsigned long GATHER_PHASE_TIME = 2000;   // 聚拢阶段：2秒
const unsigned long IMPACT_COOLDOWN = 800;      // 多久没有冲击才开始聚拢（0.8秒）
const unsigned long GAME_ACTIVATION_THRESHOLD = 2000;  // 超过此时长激活小游戏（3秒）
const float VELOCITY_DECAY = 0.95;              // 速度衰减系数
const float GATHER_FORCE = 0.15;                // 聚拢力度
const float GRAVITY_FORCE = 0.08;               // 倾斜重力影响系数

// 游戏状态标志
bool miniGameEnabled = false;                   // 小游戏是否启用

// 点收集游戏相关变量
TargetPoint target;
Ripple ripple;
FireworkParticle fireworkParticles[MAX_FIREWORK_PARTICLES];
const unsigned long TARGET_LIFETIME = 3000;     // 目标存活时间 3秒
const unsigned long TOUCH_TIME_REQUIRED = 500;  // 需要静止的时间 0.5秒
const unsigned long RIPPLE_DURATION = 800;      // 冲击波持续时间
unsigned long touchStartTime = 0;               // 开始触碰的时间
bool isTouching = false;                        // 是否正在触碰目标
float touchBrightness = 0.0;                    // 触碰时的亮度增强 

void RGB_Matrix() {
  for (int row = 0; row < Matrix_Row; row++) {
    for (int col = 0; col < Matrix_Col; col++) {
      int pixelIndex = row * 8 + col;
      bool pixelDrawn = false;
      
      // 点收集游戏渲染
      if(miniGameEnabled) {
        // 检查是否是目标点
        if(target.active && row == target.x && col == target.y) {
          // 红色呼吸灯效果（降低亮度到1/3）
          uint8_t red = (uint8_t)(40 * target.brightness);  // 从120降到40
          pixels.setPixelColor(pixelIndex, pixels.Color(red, 0, 0));
          pixelDrawn = true;
        }
        // 检查是否在冲击波范围内
        else if(ripple.active) {
        if(ripple.type == RIPPLE_RING) {
          // 圆环冲击波
          float dx = row - ripple.centerX;
          float dy = col - ripple.centerY;
          float dist = sqrt(dx * dx + dy * dy);
          
          float ringThickness = 0.8;
          if(dist >= ripple.radius - ringThickness && dist <= ripple.radius + ringThickness) {
            float intensity = 1.0 - (dist - (ripple.radius - ringThickness)) / (2 * ringThickness);
            uint8_t brightness = (uint8_t)(intensity * 128);  // 降低至50%
            pixels.setPixelColor(pixelIndex, pixels.Color(brightness, brightness, brightness));
            pixelDrawn = true;
          }
        }
        else if(ripple.type == RIPPLE_RAINBOW) {
          // 彩虹圆环冲击波
          float dx = row - ripple.centerX;
          float dy = col - ripple.centerY;
          float dist = sqrt(dx * dx + dy * dy);
          
          float ringThickness = 1.2;
          if(dist >= ripple.radius - ringThickness && dist <= ripple.radius + ringThickness) {
            float intensity = 1.0 - (dist - (ripple.radius - ringThickness)) / (2 * ringThickness);
            // 根据距离计算色相，形成彩虹效果
            uint16_t hue = (uint16_t)((dist / ripple.radius) * 65535);
            uint32_t color = pixels.ColorHSV(hue, 255, (uint8_t)(intensity * 128));  // 降低至50%
            pixels.setPixelColor(pixelIndex, color);
            pixelDrawn = true;
          }
        }
        else if(ripple.type == RIPPLE_FIREWORK) {
          // 烟花粒子效果
          for(int i = 0; i < MAX_FIREWORK_PARTICLES; i++) {
            if(fireworkParticles[i].active) {
              int px = (int)(fireworkParticles[i].x + 0.5);
              int py = (int)(fireworkParticles[i].y + 0.5);
              if(px == row && py == col) {
                uint32_t color = pixels.ColorHSV(fireworkParticles[i].hue * 256, 255, 128);  // 降低至50%
                pixels.setPixelColor(pixelIndex, color);
                pixelDrawn = true;
                break;
              }
            }
          }
        }
        else if(ripple.type == RIPPLE_SPIRAL) {
          // 方形冲击波 (替代原有的螺旋冲击波)
          float dx = abs(row - ripple.centerX);
          float dy = abs(col - ripple.centerY);
          
          // 切比雪夫距离定义了方形轮廓
          float dist = (dx > dy) ? dx : dy;
          
          float ringThickness = 0.8;
          
          if(dist >= ripple.radius - ringThickness && dist <= ripple.radius + ringThickness) {
            float intensity = 1.0 - (dist - (ripple.radius - ringThickness)) / (2 * ringThickness);
            
            // 随着距离改变颜色 (青色 -> 蓝色)
            uint8_t green = (uint8_t)(intensity * (128 - dist * 10)); 
            uint8_t blue = (uint8_t)(intensity * 128);
            if(green < 0) green = 0;
            
            pixels.setPixelColor(pixelIndex, pixels.Color(0, green, blue));
            pixelDrawn = true;
          }
        }
        else if(ripple.type == RIPPLE_PULSE) {
          // 脉冲冲击波（多层圆环）
          float dx = row - ripple.centerX;
          float dy = col - ripple.centerY;
          float dist = sqrt(dx * dx + dy * dy);
          
          // 创建3个波浪圆环
          for(int wave = 0; wave < 3; wave++) {
            float waveRadius = ripple.radius - wave * 2.5;
            if(waveRadius > 0) {
              float ringThickness = 0.7;
              if(dist >= waveRadius - ringThickness && dist <= waveRadius + ringThickness) {
                float intensity = 1.0 - (wave * 0.3);
                intensity *= 1.0 - abs(dist - waveRadius) / ringThickness;
                uint8_t brightness = (uint8_t)(intensity * 128);  // 降低至50%
                // 不同波浪不同颜色
                if(wave == 0) pixels.setPixelColor(pixelIndex, pixels.Color(brightness, 0, brightness));      // 紫色
                else if(wave == 1) pixels.setPixelColor(pixelIndex, pixels.Color(0, brightness, brightness)); // 青色
                else pixels.setPixelColor(pixelIndex, pixels.Color(brightness, brightness, 0));               // 黄色
                pixelDrawn = true;
                break;
              }
            }
          }
        }
        }  // 结束点收集游戏的 else if(ripple.active)
      }  // 结束点收集游戏的 if(miniGameEnabled)
      
      // 正常显示玩家光点或背景
      if(!pixelDrawn) {
        if(Matrix_Data[row][col] == 1) {
          pixels.setPixelColor(pixelIndex, pixels.Color(RGB_Data[0], RGB_Data[1], RGB_Data[2]));   
        }
        else {
          pixels.setPixelColor(pixelIndex, pixels.Color(0, 0, 0)); 
        }
      }
    }
  }
  pixels.show();
}


uint8_t x=4,y=4;
void Game(uint8_t X_EN,uint8_t Y_EN) 
{
  Matrix_Data[x][y] = 0;
  if(X_EN && Y_EN){
    if(X_EN == 1)
      x=x+1;
    else
      x=x-1;
    if(Y_EN == 1)
      y=y+1;
    else
      y=y-1;
  }
  else if(X_EN){
    if(X_EN == 1)
      x=x+1;
    else
      x=x-1;
  }
  else if(Y_EN){
    if(Y_EN == 1)
      y=y+1;
    else
      y=y-1;
  }
  if(x < 0) x = 0;
  if(x == 8) x = 7;
  if(x > 8) x = 0;
  if(y < 0) y = 0;
  if(y == 8) y = 7;
  if(y > 8) y = 0;
  printf("%d\r\n",y);
  Matrix_Data[x][y]=1;
  RGB_Matrix();
}
void Matrix_Init() {
  pixels.begin();
  // English: Please note that the brightness of the lamp bead should not be too high, which can easily cause the temperature of the board to rise rapidly, thus damaging the board !!!
  // Chinese: 请注意，灯珠亮度不要太高，容易导致板子温度急速上升，从而损坏板子!!! 
  pixels.setBrightness(60);                       // set brightness  
  memset(Matrix_Data, 0, sizeof(Matrix_Data)); 
  
  // 初始化粒子数组
  for(int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = false;
  }
  
  // 初始化分裂模式标志
  splitTriggeredFromGame = false;
  
  // 初始化游戏状态
  miniGameEnabled = false;
  
  // 初始化点收集游戏变量
  target.active = false;
  ripple.active = false;
  isTouching = false;
  touchBrightness = 0.0;
  
  // 初始化烟花粒子
  for(int i = 0; i < MAX_FIREWORK_PARTICLES; i++) {
    fireworkParticles[i].active = false;
  }
}

// 触发分裂模式
void TriggerSplitMode() {
  unsigned long currentTime = millis();
  lastImpactTime = currentTime;  // 更新最后冲击时间
  
  if(splitModeActive) {
    // 如果已经在分裂模式中，持续晃动会延长分裂，重置聚拢状态
    if(gatheringStarted) {
      gatheringStarted = false;  // 取消聚拢，回到分裂阶段
    }
    return;  // 不重新初始化粒子
  }
  
  // 首次触发分裂模式
  splitModeActive = true;
  splitModeStartTime = currentTime;
  gatheringStarted = false;
  
  // 记录当前是否在游戏中
  splitTriggeredFromGame = miniGameEnabled;
  
  // 随机选择一个粒子作为聚拢目标
  targetParticleIndex = random(0, MAX_PARTICLES);
  
  // 创建8个粒子，向8个主要方向发射（上下左右+四个对角）
  for(int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].x = x;
    particles[i].y = y;
    particles[i].active = true;
    
    // 给每个粒子不同的速度方向，均匀分布在8个方向
    float angle = (i * 360.0 / MAX_PARTICLES) * 0.017453; // 转换为弧度
    particles[i].vx = cos(angle) * 1.0;  // 初始速度提高一点
    particles[i].vy = sin(angle) * 1.0;
  }
}

// 处理粒子间碰撞
void HandleParticleCollisions() {
  const float COLLISION_DISTANCE = 0.8;  // 碰撞检测距离
  const float BOUNCE_FACTOR = 0.9;       // 碰撞反弹系数
  
  // 检测所有粒子对
  for(int i = 0; i < MAX_PARTICLES; i++) {
    if(!particles[i].active) continue;
    
    for(int j = i + 1; j < MAX_PARTICLES; j++) {
      if(!particles[j].active) continue;
      
      // 计算两个粒子之间的距离
      float dx = particles[j].x - particles[i].x;
      float dy = particles[j].y - particles[i].y;
      float distance = sqrt(dx * dx + dy * dy);
      
      // 如果距离小于碰撞阈值，发生碰撞
      if(distance < COLLISION_DISTANCE && distance > 0.01) {
        // 归一化方向向量
        float nx = dx / distance;
        float ny = dy / distance;
        
        // 计算相对速度
        float dvx = particles[i].vx - particles[j].vx;
        float dvy = particles[i].vy - particles[j].vy;
        
        // 相对速度在碰撞法线方向的投影
        float dotProduct = dvx * nx + dvy * ny;
        
        // 如果粒子正在接近（不是远离），才处理碰撞
        if(dotProduct > 0) {
          // 简化的弹性碰撞：交换速度分量
          float impulse = dotProduct * BOUNCE_FACTOR;
          
          particles[i].vx -= impulse * nx;
          particles[i].vy -= impulse * ny;
          particles[j].vx += impulse * nx;
          particles[j].vy += impulse * ny;
          
          // 分离粒子，避免重叠
          float overlap = COLLISION_DISTANCE - distance;
          float separateX = nx * overlap * 0.5;
          float separateY = ny * overlap * 0.5;
          
          particles[i].x -= separateX;
          particles[i].y -= separateY;
          particles[j].x += separateX;
          particles[j].y += separateY;
        }
      }
    }
  }
}

// 更新分裂模式
void UpdateSplitMode() {
  if(!splitModeActive) return;
  
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - splitModeStartTime;
  unsigned long timeSinceLastImpact = currentTime - lastImpactTime;
  
  // 判断是否应该开始聚拢阶段
  // 条件：没有在聚拢 且 距离最后一次冲击超过IMPACT_COOLDOWN时间
  if(!gatheringStarted && timeSinceLastImpact > IMPACT_COOLDOWN) {
    gatheringStarted = true;
    gatherStartTime = currentTime;
  }
  
  // 清空矩阵
  memset(Matrix_Data, 0, sizeof(Matrix_Data));
  
  bool allGathered = true;  // 所有粒子是否都聚拢到中心
  int activeCount = 0;
  
  // 更新每个粒子位置
  for(int i = 0; i < MAX_PARTICLES; i++) {
    if(!particles[i].active) continue;
    
    // 阶段1：飞散阶段（未开始聚拢）
    if(!gatheringStarted) {
      // 应用倾斜重力效果（加速度计影响）
      // Accel.x: 左右倾斜, Accel.y: 前后倾斜
      particles[i].vx += Accel.x * GRAVITY_FORCE;
      particles[i].vy -= Accel.y * GRAVITY_FORCE;  // Y轴取反
      
      // 粒子向外飞行
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;
      
      // 边界碰撞反弹
      if(particles[i].x <= 0 || particles[i].x >= 7) {
        particles[i].vx *= -0.8;  // 反弹并损失能量
        if(particles[i].x < 0) particles[i].x = 0;
        if(particles[i].x > 7) particles[i].x = 7;
      }
      if(particles[i].y <= 0 || particles[i].y >= 7) {
        particles[i].vy *= -0.8;
        if(particles[i].y < 0) particles[i].y = 0;
        if(particles[i].y > 7) particles[i].y = 7;
      }
      
      allGathered = false;
    }
    // 阶段2：聚拢阶段（开始聚拢后）
    else {
      // 如果是目标粒子，它继续受重力影响自由移动
      if(i == targetParticleIndex) {
        // 目标粒子受重力影响
        particles[i].vx += Accel.x * GRAVITY_FORCE * 0.3;
        particles[i].vy -= Accel.y * GRAVITY_FORCE * 0.3;
        
        // 速度衰减
        particles[i].vx *= 0.9;
        particles[i].vy *= 0.9;
        
        // 更新位置
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        
        // 边界限制
        if(particles[i].x < 0) particles[i].x = 0;
        if(particles[i].x > 7) particles[i].x = 7;
        if(particles[i].y < 0) particles[i].y = 0;
        if(particles[i].y > 7) particles[i].y = 7;
      }
      // 其他粒子向目标粒子聚拢
      else {
        // 计算到目标粒子的方向
        float dx = particles[targetParticleIndex].x - particles[i].x;
        float dy = particles[targetParticleIndex].y - particles[i].y;
        float distance = sqrt(dx * dx + dy * dy);
        
        // 如果已经非常接近目标，停止移动
        if(distance < 0.3) {
          particles[i].x = particles[targetParticleIndex].x;
          particles[i].y = particles[targetParticleIndex].y;
          particles[i].vx = 0;
          particles[i].vy = 0;
        } else {
          // 向目标粒子施加吸引力
          float forceX = (dx / distance) * GATHER_FORCE;
          float forceY = (dy / distance) * GATHER_FORCE;
          
          // 应用速度衰减
          particles[i].vx *= 0.85;
          particles[i].vy *= 0.85;
          
          // 添加向心力
          particles[i].vx += forceX;
          particles[i].vy += forceY;
          
          // 在聚拢阶段也添加较弱的重力影响（减半）
          particles[i].vx += Accel.x * GRAVITY_FORCE * 0.5;
          particles[i].vy -= Accel.y * GRAVITY_FORCE * 0.5;  // Y轴取反
          
          // 更新位置
          particles[i].x += particles[i].vx;
          particles[i].y += particles[i].vy;
          
          allGathered = false;
        }
      }
    }
    
    // 在矩阵上显示粒子
    int px = (int)(particles[i].x + 0.5);  // 四舍五入
    int py = (int)(particles[i].y + 0.5);
    if(px >= 0 && px < 8 && py >= 0 && py < 8) {
      Matrix_Data[px][py] = 1;
      activeCount++;
    }
  }
  
  // 在飞散阶段处理粒子间碰撞
  if(!gatheringStarted) {
    HandleParticleCollisions();
  }
  
  // 如果所有粒子都聚拢到目标粒子，结束分裂模式
  if(allGathered && gatheringStarted) {
    splitModeActive = false;
    gatheringStarted = false;
    
    // 使用目标粒子的位置作为新的光点位置
    x = (uint8_t)(particles[targetParticleIndex].x + 0.5);
    y = (uint8_t)(particles[targetParticleIndex].y + 0.5);
    // 确保在范围内
    if(x >= 8) x = 7;
    if(y >= 8) y = 7;
    Matrix_Data[x][y] = 1;
    
    // 判断是否从游戏中触发的分裂
    if(splitTriggeredFromGame) {
      // 如果是从游戏中触发，100%退出游戏
      miniGameEnabled = false;
      splitTriggeredFromGame = false;  // 重置标志
    } else {
      // 从默认模式触发，根据持续时长决定是否激活小游戏
      if(elapsed >= GAME_ACTIVATION_THRESHOLD) {
        // 持续晃动时间超过阈值（3秒），100%激活小游戏
        miniGameEnabled = true;
        SpawnNewTarget();
      } else {
        // 短暂晃动（快速触发并合并），不激活游戏
        miniGameEnabled = false;
      }
    }
  }
  
  RGB_Matrix();
}

// 检查是否在分裂模式
bool IsSplitModeActive() {
  return splitModeActive;
}

// 生成新目标点
void SpawnNewTarget() {
  target.active = true;
  target.spawnTime = millis();
  
  // 随机生成位置，避免和玩家当前位置重叠
  do {
    target.x = random(0, 8);
    target.y = random(0, 8);
  } while(target.x == x && target.y == y);
  
  target.brightness = 0.0;
}

// 更新点收集小游戏
void UpdateMiniGame() {
  // 如果小游戏未启用，清理所有状态并返回
  if(!miniGameEnabled) {
    target.active = false;
    ripple.active = false;
    isTouching = false;
    touchBrightness = 0.0;
    // 清空烟花粒子
    for(int i = 0; i < MAX_FIREWORK_PARTICLES; i++) {
      fireworkParticles[i].active = false;
    }
    return;  // 不执行后续逻辑
  }
  
  unsigned long currentTime = millis();
  
  // 如果没有活跃目标，生成新目标
  if(!target.active) {
    SpawnNewTarget();
  }
  
  // 更新目标点呼吸灯效果
  if(target.active) {
    unsigned long elapsed = currentTime - target.spawnTime;
    
    // 呼吸灯效果（使用正弦波，降低频率和最大亮度）
    float breathPhase = (elapsed % 2500) / 2500.0;  // 2.5秒一个呼吸周期（更慢）
    float sinValue = sin(breathPhase * 3.14159 * 2 - 3.14159 / 2);  // 从-π/2开始
    target.brightness = 0.2 + 0.15 * (sinValue + 1.0);  // 亮度范围 0.2-0.5，从最低开始
    
    // 检查是否超时
    if(elapsed >= TARGET_LIFETIME) {
      target.active = false;
      isTouching = false;
      return;
    }
    
    // 检查玩家是否在目标点上
    if(x == target.x && y == target.y) {
      if(!isTouching) {
        // 刚碰到目标点，立即触发冲击波！
        isTouching = true;
        
        // 随机选择冲击波类型（5种）
        ripple.type = (RippleType)random(0, 5);
        
        // 触发冲击波
        ripple.active = true;
        ripple.centerX = target.x;
        ripple.centerY = target.y;
        ripple.radius = 0.0;
        ripple.startTime = currentTime;
        
        // 如果是烟花类型，初始化烟花粒子
        if(ripple.type == RIPPLE_FIREWORK) {
          for(int i = 0; i < MAX_FIREWORK_PARTICLES; i++) {
            fireworkParticles[i].active = true;
            fireworkParticles[i].x = target.x;
            fireworkParticles[i].y = target.y;
            
            // 向不同方向发射（16个方向，适中速度）
            float angle = (i * 360.0 / MAX_FIREWORK_PARTICLES) * 0.017453;
            fireworkParticles[i].vx = cos(angle) * 0.7;  // 降低到0.7，更容易看清
            fireworkParticles[i].vy = sin(angle) * 0.7;
            
            // 随机颜色
            fireworkParticles[i].hue = random(0, 256);
          }
        }
        
        // 关闭目标点
        target.active = false;
        isTouching = false;
        touchBrightness = 0.0;
      }
    } else {
      // 离开目标点
      isTouching = false;
    }
  }
  
  // 更新冲击波
  if(ripple.active) {
    unsigned long elapsed = currentTime - ripple.startTime;
    
    if(ripple.type == RIPPLE_FIREWORK) {
      // 更新烟花粒子（炸开后快速减速）
      bool anyActive = false;
      for(int i = 0; i < MAX_FIREWORK_PARTICLES; i++) {
        if(fireworkParticles[i].active) {
          // 更新位置
          fireworkParticles[i].x += fireworkParticles[i].vx;
          fireworkParticles[i].y += fireworkParticles[i].vy;
          
          // 速度衰减（快速衰减，营造炸开后减速的效果）
          fireworkParticles[i].vx *= 0.95;  // 从0.97降到0.90，快速减速
          fireworkParticles[i].vy *= 0.95;
          
          // 检查是否飞出屏幕或超时（保持1200ms）
          if(fireworkParticles[i].x < -3 || fireworkParticles[i].x > 10 ||
             fireworkParticles[i].y < -3 || fireworkParticles[i].y > 10 ||
             elapsed >= 1200) {
            fireworkParticles[i].active = false;
          } else {
            anyActive = true;
          }
        }
      }
      
      // 如果所有粒子都消失了或超时，结束冲击波
      if(!anyActive || elapsed >= 1200) {
        ripple.active = false;
      }
    } 
    else {
      // 其他冲击波的扩散（统一扩散速度）
      ripple.radius = (elapsed / (float)RIPPLE_DURATION) * 12.0;
      
      // 检查是否结束
      if(elapsed >= RIPPLE_DURATION || ripple.radius > 12.0) {
        ripple.active = false;
      }
    }
  }
}