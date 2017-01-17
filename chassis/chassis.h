#define PIN_LEFT  2 //外部中断0,左轮
#define PIN_RIGHT 3 //外部中断1,右轮

#define max_linear       20 //最大线速度cm/秒
#define max_turn_line    18 //最大转弯线速度
//#define max_angular    1.45
#define max_linear_pwd   255

#define hole_number 31 //码盘孔数
#define diameter 18.535 //轮cm直径
#define diamete_ratio 1 //左轮相对于右轮轮径比系数,往左偏,调小,往右偏调大
#define center_speed 220 //小车电机的PWM功率初始值
#define gear_ratio 75 //转速比
#define car_width 27 //小车宽度
#define car_length 27 //小车长度

#define MOTOR_LEFT_PIN1 7 //L298P直流电机驱动板的左轮电机使能端口连接到数字接口5
#define MOTOR_LEFT_PIN2 6 //L298P直流电机驱动板的左轮电机转向端口连接到数字接口4
#define MOTOR_RIGHT_PIN1 8 //连接小车右轮电机的使能端口到数字接口6
#define MOTOR_RIGHT_PIN2 9 //连接小车右轮电机的转向端口到数字接口7
