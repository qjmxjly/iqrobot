/**
 *  iqrobot 底盘 arduino 驱动
 *  
 */

#include "chassis.h"        // 头文件定义
#include <PID_v1.h> // pid 控制库
#include <ros.h>
#include <ros/time.h>
#include <geometry_msgs/Twist.h>

int val_right_count_target = 0; //小车右轮码盘每秒计数PID调节目标值,根据这个值PID val_rigth;
int val_right = 0; //小车右轮电机的PWM功率值
int val_left_count_target = 0; //小车左轮码盘每秒计数PID调节目标值,根据这个值PID val_left;
int val_left = 0; //左轮电机PWM功率值。以左轮为基速度,PID调节右轮的速度
int count_left = 0;  //左轮编码器码盘脉冲计数值;用于PID调整
int count_right = 0; //右轮编码器码盘脉冲计数值;用于PID调整

char run_direction = 'f'; //f:前进;b:后退;s:stop
int linear = 0;//15; //cm/second线速度
int angular = 0;//1; //角速度,ros的angular.z

///转弯半径一定要大于小车宽度的一半,也就是linear / angular一定是大于13.5,也就是最小转弯半径是13.5
/////////
unsigned long left_old_time = 0, right_old_time = 0; // 时间标记
unsigned long time1 = 0, time2 = 0; // 时间标记
unsigned long last_command_millis = 0;

////ros
ros::NodeHandle nh;

//////PID
double left_Setpoint, left_Input, left_Output, left_setpoint;
double left_kp = 1, left_ki = 0.005, left_kd = 0.0001; //kp = 0.040,ki = 0.0005,kd =0.0011;
PID left_PID(&left_Input, &left_Output, &left_Setpoint, left_kp, left_ki, left_kd, DIRECT);

double right_Setpoint, right_Input, right_Output, right_setpoint;
double right_kp = 1, right_ki = 0.005, right_kd = 0.0001; //kp = 0.040,ki = 0.0005,kd =0.0011;
PID right_PID(&right_Input, &right_Output, &right_Setpoint, right_kp, right_ki, right_kd, DIRECT);


void message_cb(const geometry_msgs::Twist& vel)
{
  linear = vel.linear.x * 100; //ROS中的单位是m/s;这里换算成cm的单位
  angular = vel.angular.z;
}

ros::Subscriber<geometry_msgs::Twist> sub("/chassis/cmd_vel", message_cb);

void pidSetup() {
  //定义外部中断0和1的中断子程序Code(),中断触发为下跳沿触发
  //当编码器码盘的OUT脉冲信号发生下跳沿中断时，
  //将自动调用执行中断子程序Code()。
  left_old_time = millis();
  right_old_time = millis();
  attachInterrupt(0, Code1, FALLING);//小车左车轮电机的编码器脉冲中断函数
  attachInterrupt(1, Code2, FALLING);//小车右车轮电机的编码器脉冲中断函数

  left_PID.SetOutputLimits(-254, 254);
  left_PID.SetSampleTime(500);
  left_PID.SetMode(AUTOMATIC);
  left_PID.SetTunings(left_kp, left_ki, left_kd);

  right_PID.SetOutputLimits(-254, 254);
  right_PID.SetSampleTime(500);
  right_PID.SetMode(AUTOMATIC);
  right_PID.SetTunings(right_kp, right_ki, right_kd);
}

void setup() {
  Serial.begin(57600);    // 启动串口通信，波特率为9600b/s
  // reserve 200 bytes for the inputString

  pinMode(MOTOR_LEFT_PIN1, OUTPUT);   //直流电机驱动板的控制端口设置为输出模式
  pinMode(MOTOR_LEFT_PIN2, OUTPUT);
  pinMode(MOTOR_RIGHT_PIN1, OUTPUT);
  pinMode(MOTOR_RIGHT_PIN2, OUTPUT);

  pidSetup();

  nh.initNode();
  nh.subscribe(sub);
  //broadcaster.init(nh);


}



void loop() {
  nh.spinOnce();
  delay(1);
  runMotor();
}

void resetMotor() {
  val_left_count_target = 0;
  left_Setpoint = 0;
  val_right_count_target = 0;
  right_Setpoint = 0;
  linear = 0;
  angular = 0;
  run_direction = 's';  
  stopMotor();
}

void runMotor() {
  
  if (angular == 0) { //直行
    if (linear > 0) { //前进
      if((millis() - last_command_millis) > AUTO_STOP_INTERVAL) {
       last_command_millis = millis();
        //Serial.println("Forward");
      }
      if (linear > max_linear)
        linear = max_linear;

      float linear_left = linear; //左内圈线速度
      float linear_right = linear; //右外圈线速度


      val_right_count_target = linear_right * gear_ratio / (diameter / hole_number); //左内圈线速度对应的孔数
      val_left_count_target = linear_left * gear_ratio / (diameter * diamete_ratio / hole_number); //右外圈线速度对应的孔数

      val_right = linear_right * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,左轮
      val_left = linear_left * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,右

      left_Setpoint = val_left_count_target;
      right_Setpoint = val_right_count_target;

      advance();
    } 
    else if (linear < 0) { //后退
      if((millis() - last_command_millis) > AUTO_STOP_INTERVAL) {
        last_command_millis = millis();
        //Serial.println("back");
      }

      int abslinear = abs(linear);
      if (abslinear > max_linear)
        abslinear = max_linear;
      float linear_left = abslinear; //左内圈线速度
      float linear_right = abslinear; //右外圈线速度


      val_right_count_target = linear_right * gear_ratio / (diameter * diamete_ratio / hole_number); //左内圈线速度对应的孔数
      val_left_count_target = linear_left * gear_ratio / (diameter / hole_number); //右外圈线速度对应的孔数

      val_right = linear_right * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,左轮
      val_left = linear_left * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,右轮

      left_Setpoint = val_left_count_target;
      right_Setpoint = val_right_count_target;

      back();
    } 
    else {
      stopMotor();
    }

  } 
  else if (angular > 0) { //左转
    
    if (linear > max_turn_line) //////限制最大转弯线速度
    {
      angular = angular * max_turn_line / linear;
    } 
    
    linear = linear > max_turn_line ? max_turn_line : linear;
    
    float radius = linear / angular; //计算半径

    if (radius < car_width / 2) ///////如果计算的转弯半径小于最小半径,则设置为最小转弯半径
      radius = car_width / 2;

    float radius_left = radius - car_width / 2; //左内圈半径
    float radius_right = radius + car_width / 2; //右外圈半径

    float linear_left = radius_left * angular; //左内圈线速度
    float linear_right = radius_right * angular; //右外圈线速度


    val_right_count_target = linear_right * gear_ratio / (diameter / hole_number); //左内圈线速度对应的孔数
    val_left_count_target = linear_left * gear_ratio / (diameter * diamete_ratio / hole_number); //右外圈线速度对应的孔数

    val_right = linear_right * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,左轮
    val_left = linear_left * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,右轮

    left_Setpoint = val_left_count_target;
    right_Setpoint = val_right_count_target;

    advance();

  } 
  else if (angular < 0) { //右转

    if (linear > max_turn_line) //////限制最大转弯线速度
    {
      angular = angular * max_turn_line / linear;
    } 
    
    linear = linear > max_turn_line ? max_turn_line : linear;
    
    float radius = linear / angular; //计算半径

    if (radius < car_width / 2) ///////如果计算的转弯半径小于最小半径,则设置为最小转弯半径
      radius = car_width / 2;

    float radius_left = radius + car_width / 2;
    float radius_right = radius - car_width / 2;

    float linear_left = radius_left * angular;
    float linear_right = radius_right * angular;

    val_right_count_target = linear_right * gear_ratio / (diameter / hole_number); //左内圈线速度对应的孔数
    val_left_count_target = linear_left * gear_ratio / (diameter * diamete_ratio / hole_number); //右外圈线速度对应的孔数

    val_right = linear_right * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,左轮
    val_left = linear_left * (max_linear_pwd / max_linear); //根据轮径参数计算出来的线速度对应的PWD值,右轮

    left_Setpoint = val_left_count_target;
    right_Setpoint = val_right_count_target;

    advance();
  } 
  else {
    stopMotor();
  }

}

//子程序程序段
void advance()//前进
{
  run_direction = 'f';
  digitalWrite(MOTOR_LEFT_PIN1, HIGH);
  digitalWrite(MOTOR_LEFT_PIN2, LOW);
  analogWrite(MOTOR_LEFT_PIN1, val_left);
  digitalWrite(MOTOR_RIGHT_PIN1, HIGH);
  digitalWrite(MOTOR_RIGHT_PIN2, LOW);
  analogWrite(MOTOR_RIGHT_PIN1, val_right);
}

void back()//后退
{
  run_direction = 'b';
  digitalWrite(MOTOR_LEFT_PIN1, LOW);
  digitalWrite(MOTOR_LEFT_PIN2, HIGH);
  analogWrite(MOTOR_LEFT_PIN2, val_left);
  digitalWrite(MOTOR_RIGHT_PIN1, LOW);
  digitalWrite(MOTOR_RIGHT_PIN2, HIGH);
  analogWrite(MOTOR_RIGHT_PIN2, val_right);
}

void stopMotor()//停止
{
  digitalWrite(MOTOR_LEFT_PIN1, LOW);
  digitalWrite(MOTOR_LEFT_PIN2, LOW);
  digitalWrite(MOTOR_RIGHT_PIN1, LOW);
  digitalWrite(MOTOR_RIGHT_PIN2, LOW);
}
// 左侧车轮电机的编码器码盘计数中断子程序
void Code1()
{
  //为了不计入噪音干扰脉冲，
  //当2次中断之间的时间大于2ms时，计一次有效计数
  if ((millis() - time1) > 2) {
    //当编码器码盘的OUT脉冲信号下跳沿每中断一次，
    count_left++; // 编码器码盘计数加一
    if ((millis() - left_old_time) > 100) { ////////100ms进行一次PID调节
      if (run_direction != 's')
        PID_left();
      left_old_time = millis();
      count_left = 0;//把脉冲计数值清零，以便计算下一秒的脉冲计数
    }
    time1 == millis();
  }
}
// 右侧车轮电机的编码器码盘计数中断子程序
void Code2()
{
  if ((millis() - time2) > 2) {
    //当编码器码盘的OUT脉冲信号下跳沿每中断一次，
    count_right++; // 编码器码盘计数加一
    if ((millis() - right_old_time) > 100) { ////////100ms进行一次PID调节
      if (run_direction != 's')
        PID_right();
      right_old_time = millis();
      count_right = 0; //把脉冲计数值清零，以便计算下一秒的脉冲计数
    }

    time2 == millis();
  }
}

void PID_left() {

  left_Input = count_left * 10;
  left_PID.Compute();

  val_left = val_left + left_Output;
  if (val_left > 255)
    val_left = 255;
  if (val_left < 0)
    val_left = 0;
  if (run_direction == 'f') //根据刚刚调节后的小车电机PWM功率值，及时修正小车前进或者后退状态
    advance();
  if (run_direction == 'b')
    back();
}
void PID_right() {

  right_Input = count_right * 10;
  right_PID.Compute();
  val_right = val_right + right_Output;
  if (val_right > 255)
    val_right = 255;
  if (val_right < 0)
    val_right = 0;
  if (run_direction == 'f') //根据刚刚调节后的小车电机PWM功率值，及时修正小车前进或者后退状态
    advance();
  if (run_direction == 'b')
    back();
}

