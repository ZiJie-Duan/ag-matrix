#include "WS_QMI8658.h"
#include "WS_Matrix.h"

// English: Please note that the brightness of the lamp bead should not be too high, which can easily cause the temperature of the board to rise rapidly, thus damaging the board !!!
// Chinese: 请注意，灯珠亮度不要太高，容易导致板子温度急速上升，从而损坏板子!!! 
extern IMUdata Accel; 
IMUdata game;

// 声明 WiFi 连接函数 (在 Connect_Wifi.ino 中定义)
void ConnectToWifi();

void setup()
{
  QMI8658_Init();
  Matrix_Init();
  // 初始化不再连接WiFi，改为手势触发
}


uint8_t X_EN = 0, Y_EN = 0, Time_X_A = 0, Time_X_B = 0, Time_Y_A = 0, Time_Y_B = 0;
const float IMPACT_THRESHOLD = 2.0;  // 快速冲击阈值

// z轴手势检测变量
bool zAxisRaised = false;              // z轴是否已抬高
unsigned long zAxisRaisedTime = 0;     // z轴抬高的时间
const unsigned long Z_GESTURE_TIMEOUT = 2000;  // 手势超时时间（2秒）
const float Z_RAISED_THRESHOLD = -0.3;   // z轴抬高阈值（正常是-1.0左右，需要抬高约0.7）
const float Z_NORMAL_THRESHOLD = -0.9;   // z轴降低阈值（需要从抬高状态降低约0.6）
const float XY_STABLE_THRESHOLD = 0.4;   // x和y轴的稳定阈值（绝对值小于此值视为稳定，调高以降低速度要求）

void loop()
{
  QMI8658_Loop();
  
  // 检测快速冲击（任意方向的强烈加速度）
  float totalAccel = sqrt(Accel.x * Accel.x + Accel.y * Accel.y + (Accel.z + 1.0) * (Accel.z + 1.0));
  if(totalAccel > IMPACT_THRESHOLD) {
    // 触发或延长分裂模式（即使已在分裂模式中也会更新冲击时间）
    TriggerSplitMode();
    extern bool miniGameEnabled;
    miniGameEnabled = false;  // 关闭小游戏
    
    // 重置Z轴手势状态，确保分裂模式结束后回到默认模式，而不是误触发Z轴手势
    zAxisRaised = false;
  }
  
  // 如果在分裂模式中，更新分裂效果
  if(IsSplitModeActive()) {
    UpdateSplitMode();
    delay(10);
    return;  // 分裂模式下不执行正常游戏逻辑
  }
  
  // z轴手势检测（只在默认模式中）
  extern bool miniGameEnabled;
  // Removed fullScreenMode check
  if(!miniGameEnabled) {
    // 检查x和y轴是否稳定（接近水平）
    bool xyStable = (std::abs(Accel.x) < XY_STABLE_THRESHOLD && 
                     std::abs(Accel.y) < XY_STABLE_THRESHOLD);
    
    // 检测z轴抬高（需要x和y轴稳定）
    if(!zAxisRaised && Accel.z > Z_RAISED_THRESHOLD && xyStable) {
      zAxisRaised = true;
      zAxisRaisedTime = millis();
    }
    
    // 检测z轴降低（在抬高后2秒内，需要x和y轴稳定）
    if(zAxisRaised) {
      unsigned long elapsed = millis() - zAxisRaisedTime;
      
      if(Accel.z < Z_NORMAL_THRESHOLD && elapsed <= Z_GESTURE_TIMEOUT && xyStable) {
        // 手势完成，进入 WIFI 连接和后端交互模式
        zAxisRaised = false;
        ConnectToWifi(); // 这是一个阻塞调用，会接管控制权直到被打断
      } else if(elapsed > Z_GESTURE_TIMEOUT) {
        // 超时，重置
        zAxisRaised = false;
      } else if(!xyStable) {
        // 如果x或y轴移动了，取消手势
        zAxisRaised = false;
      }
    }
  } else {
    // 在非默认模式中，重置z轴检测状态
    zAxisRaised = false;
  }
  
  // 更新点收集小游戏（永远运行）
  UpdateMiniGame();
  
  // 只有在非分裂模式才响应倾斜控制 (Removed fullScreenMode check)
  if(!IsSplitModeActive() && (Accel.x > 0.15 || Accel.x < 0  || Accel.y > 0.15 || Accel.y < 0  || Accel.z > -0.9 || Accel.z < -1.1)){
    if(Accel.x > 0.15){
      Time_X_A = Time_X_A + Accel.x * 10;
      Time_X_B = 0;
    }
    else if(Accel.x < 0){
      Time_X_B = Time_X_B + std::abs(Accel.x) * 10;
      Time_X_A = 0;
    }
    else{
      Time_X_A = 0;
      Time_X_B = 0;
    }
    if(Accel.y > 0.15){
      Time_Y_A = Time_Y_A + Accel.y * 10;
      Time_Y_B = 0;
    }
    else if(Accel.y < 0){
      Time_Y_B = Time_Y_B + std::abs(Accel.y) * 10;
      Time_Y_A = 0;
    }
    else{
      Time_Y_A = 0;
      Time_Y_B = 0;
    }
    if(Time_X_A >= 10){
      X_EN = 1;
      Time_X_A = 0;
      Time_X_B = 0;
    }
    if(Time_X_B >= 10){
      X_EN = 2;
      Time_X_A = 0;
      Time_X_B = 0;
    }
    if(Time_Y_A >= 10){
      Y_EN = 2;
      Time_Y_A = 0;
      Time_Y_B = 0;
    }
    if(Time_Y_B >= 10){
      Y_EN = 1;
      Time_Y_A = 0;
      Time_Y_B = 0;
    }
    Game(X_EN,Y_EN);
    X_EN = 0;
    Y_EN = 0;
  }
  else {
    // 即使没有移动，也要刷新显示（为了小游戏）
    RGB_Matrix();
  }
  
  delay(10);
}
