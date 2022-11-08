#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
//#include "EEPROM.h"

#include <BluetoothSerial.h>

#define WiFi_rst 0
//#define LENGTH(x) (strlen(x) + 1)
//#define EEPROM_SIZE 200 // EEPROM size
String ssid; // string variable to store ssid
String pss;  // string variable to store password
// 0 5 14 15 ，启动时候是 pwm 输出，是高电平，不能用来 控制电机，否则上电的时候，小车就会往前跑
// 34 35 36 39 只能 输入
// in1 in2 控制左后轮
#define in1 32
#define in2 33
////in3 in4 控制右后轮
#define in3 25
#define in4 26
//控制 右前轮
#define FRONT_IN1 4
#define FRONT_IN2 16
//控制 左前轮
#define FRONT_IN3 17
#define FRONT_IN4 18

#define MIN_SPEED 50
#define MAX_SPEED 255

#define STOP '0'
#define UP '1'
#define DOWN '2'
#define LEFT '3'
#define RIGHT '4'

#define UP_LEFT '5'
#define UP_RIGHT '6'
#define DOWN_LEFT '7'
#define DOWN_RIGHT '8'

#define LEFT_TRANSLATION '9'
#define RIGHT_TRANSLATION 'a'
#define WIFI_CONNECT_FAIL '0'
#define WIFI_CONNECT_SUCCESS '1'
const char *client_id = "你的小车id";                //客户端id，必须唯一，
const char *TOPIC = "car_control_你的小车id";        //后面为设备id ，
const char *TOPIC_STATUS = "home/status/你的小车id"; //用于控制端监听 上下线，
const char *MQTT = "car.mytx.tech";                           // MQTT 连接地址  // tcp://car.mytx.tech
// tcp://broker-cn.emqx.io
// client.setServer("broker-cn.emqx.io", 1883); // tcp://broker-cn.emqx.io
// 订阅信息主题
BluetoothSerial SerialBt; // esp32 经典蓝牙 4.0 以下
WiFiClient espClient;
PubSubClient client;
void turnPWM(char data, double speed, int angle);
void callback(char *topic, byte *payload, unsigned int length);
void turn(char data, int angle);
void serialReadCMD();
// void writeStringToFlash(const char *toStore, int startAddr);
// String readStringFromFlash(int startAddr);

long lastMsg = 0;
long lastReceiveMsg = 0;
char lastCommand;
int cc = 0;
String speedPercend = "00";

/***
 * 检测联网状态
 *
 * 读取flash 中保存的 wifi 账户密码进行wifi连接
 **/
bool AutoConfig()
{

  WiFi.begin();
  for (int i = 0; i < 10; i++)
  {
    int wstatus = WiFi.status();
    if (wstatus == WL_CONNECTED)
    {
      Serial.println("WIFI Connect Success");
      Serial.printf("SSID:%s", WiFi.SSID().c_str());
      Serial.printf(", PSW:%s\r\n", WiFi.psk().c_str());
      Serial.print("LocalIP:");
      Serial.print(WiFi.localIP());
      Serial.print(" ,GateIP:");
      Serial.println(WiFi.gatewayIP());
      return true;
    }
    else
    {
      Serial.println("WIFI Connect Waiting......");
      delay(1000);
    }
  }
  // Serial.println("WIFI Config Faild!");
  return false;
}
/***
 * 启动自动配网，需要app 配合进行 把联网信息 进行 udp 广播，esp32 进行数据拦截获取联网信息
 *
 * esp_err_t esp_wifi_restore(void) 清理wifi 信息
 **/
void SmartConfig()
{
  WiFi.mode(WIFI_AP_STA);
  Serial.println("WIFI Wait for Smartconfig");
  WiFi.beginSmartConfig();
  while (1)
  {
    Serial.print(".");
    if (WiFi.smartConfigDone())
    {
      Serial.println("WIFI SmartConfig Success");
      ssid = WiFi.SSID();
      pss = WiFi.psk();
      // writeStringToFlash(ssid.c_str(), 0); // storing ssid at address 0
      // writeStringToFlash(pss.c_str(), 40); // storing pss at address 40
      Serial.printf("SSID:%s", WiFi.SSID().c_str());
      Serial.printf(", PSW:%s\r\n", WiFi.psk().c_str());
      Serial.print("LocalIP:");
      Serial.print(WiFi.localIP());
      Serial.print(" ,GateIP:");
      Serial.println(WiFi.gatewayIP());
      WiFi.setAutoConnect(true); // 设置自动连接
      break;
    }
    delay(1000);
  }
}
/**
 * 连接wifi

 * */
void connectWifi(char *wifiName, char *wifiPwd)
{

  int count = 0;
  WiFi.begin(wifiName, wifiPwd);

  while (WiFi.status() != WL_CONNECTED)
  {
    if (count > 20)
    {
      SerialBt.println(WIFI_CONNECT_FAIL); //连接失败
      break;
    }

    delay(300);
    Serial.println('.');
    count++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(WiFi.localIP());
    SerialBt.println(WIFI_CONNECT_SUCCESS); //连接成功
  }
}
void stop()
{

  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);

  analogWrite(in1, LOW);
  analogWrite(in2, LOW);
  analogWrite(in3, LOW);
  analogWrite(in4, LOW);

  digitalWrite(FRONT_IN1, LOW);
  digitalWrite(FRONT_IN2, LOW);
  digitalWrite(FRONT_IN3, LOW);
  digitalWrite(FRONT_IN4, LOW);

  analogWrite(FRONT_IN1, LOW);
  analogWrite(FRONT_IN2, LOW);
  analogWrite(FRONT_IN3, LOW);
  analogWrite(FRONT_IN4, LOW);

  delay(50);
}

void setup()
{
  Serial.begin(115200);

  pinMode(WiFi_rst, INPUT);

  //直接写死wifi连接信息 无用使用app 配网
  connectWifi("Xiaomi_FDD9", "lihai2070*");
  delay(100);
  SerialBt.begin("CoolPlay");
  SerialBt.setPin("1234");
  //下面的while 循环是进入配网等待，如果前面已经调用了 connectWifi（）方法，无需再使用配网
  // while (!AutoConfig())
  // {
  //   // SmartConfig(); 这个esp 官方配置方式 不太好使，，

  //   serialReadCMD();
  // }

  client.setClient(espClient);
  client.setServer(MQTT, 1883);
  client.setCallback(callback);

  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  // pinMode(enA, OUTPUT);

  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  // pinMode(enB, OUTPUT);

  pinMode(FRONT_IN1, OUTPUT);
  pinMode(FRONT_IN2, OUTPUT);
  pinMode(FRONT_IN3, OUTPUT);
  pinMode(FRONT_IN4, OUTPUT);

  Serial.println("start ,,,,");
}
void callback(char *topic, byte *payload, unsigned int length)
{

  String angle = ""; // 000
  speedPercend = "";
  Serial.print("Message arrived [");
  Serial.print(topic); // 打印主题信息
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]); // 打印主题内容

    if (i > 0 && i <= 2)
    {
      speedPercend = speedPercend + (char)payload[i];
    }
    if (i > 2)
    {
      angle = angle + (char)payload[i];
    }
  }

  Serial.println();

  // if ((char)payload[0] == '1') {
  //   digitalWrite(BUILTIN_LED, HIGH);   // 亮灯
  // } else {
  //   digitalWrite(BUILTIN_LED, LOW);   // 熄灯
  // }

  // turn((char)payload[0], angle.toInt());

  turnPWM((char)payload[0], speedPercend.toInt() * MAX_SPEED / 100, angle.toInt());
  lastReceiveMsg = millis();
}

void reconnect()
{

  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect(client_id))
  {
    Serial.println("connected");
    // 连接成功时订阅主题
    client.subscribe(TOPIC);
  }
  else
  {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }
}

/**
 * 读取蓝牙命令消息
 **/
void serialReadCMD()
{

  while (SerialBt.available())
  {
    // chat []msg =    SerialBt.available
    // char *buffer;
    // SerialBt.readBytes(buffer, SerialBt.available());
    // turn(data, 0);
    // msg = msg + data;

    String data = SerialBt.readString();
    Serial.println(" WiFi data: " + data);
    String name = data.substring(0, data.indexOf("|"));
    String pwd = data.substring(data.indexOf("|") + 1);
    Serial.println(" WiFi name: " + name);
    Serial.println(" WiFi pwd: " + pwd);
    connectWifi(name.begin(), pwd.begin());
    delay(15);
  }
}

void loop()
{

  client.loop();

  long now = millis();
  if (now - lastMsg > 2000)
  {
    lastMsg = now;
    client.publish(TOPIC_STATUS, "{device:client_id,'status':'on'}");
    if (now - lastReceiveMsg > 2000) // 2s 没有收到命令，让小车停止,每2s 检测一次
    {
      turn(STOP, 0);
    }
  }

  // serialReadCMD();

  now = millis();
  while (digitalRead(WiFi_rst) == LOW)
  {
    // Wait till boot button is pressed
  }
  // check the button press time if it is greater than 3sec clear wifi cred and restart ESP
  if (millis() - now >= 3000)
  {
    Serial.println("Reseting the WiFi credentials");
    // writeStringToFlash("", 0);  // Reset the SSID
    // writeStringToFlash("", 40); // Reset the Password

    // esp_err_t esp_wifi_restore(); //清理wifi 信息 ,但是无效，清理后，还能 连接成功
    WiFi.disconnect(1, 1); //清理wifi 配网信息
    Serial.println("Wifi credentials erased");
    Serial.println("Restarting the ESP");
    delay(500);
    ESP.restart(); // Restart ESP
  }

  if (!client.connected())
  {
    reconnect();
  }
}

/**
 * 左后轮正方向旋转
 * */
void leftRearWheelPositiveRotate(int pwm)
{
  analogWrite(in1, LOW);
  analogWrite(in2, pwm);
}
/**
 * 左后轮反方向方向旋转
 * */
void leftRearWheelOppositeRotate(int pwm)
{
  analogWrite(in1, pwm);
  analogWrite(in2, LOW);
}

/**
 * 左后轮正方向旋转
 * */
void rightRearWheelPositiveRotate(int pwm)
{
  analogWrite(in3, LOW);
  analogWrite(in4, pwm);
}
/**
 * 右后轮反方向方向旋转
 * */
void rightRearWheelOppositeRotate(int pwm)
{
  analogWrite(in3, pwm);
  analogWrite(in4, LOW);
}

/**
 * 左前轮正方向旋转
 * */
void leftFrontWheelPositiveRotate(int pwm)
{
  analogWrite(FRONT_IN3, LOW);
  analogWrite(FRONT_IN4, pwm);
}
/**
 * 左前轮反方向方向旋转
 * */
void leftFrontWheelOppositeRotate(int pwm)
{
  analogWrite(FRONT_IN3, pwm);
  analogWrite(FRONT_IN4, LOW);
}
/**
 * 右前轮正方向旋转
 * */
void rightFrontWheelPositiveRotate(int pwm)
{
  analogWrite(FRONT_IN1, LOW);
  analogWrite(FRONT_IN2, pwm);
}
/**
 * 右前轮反方向方向旋转
 * */
void rightFrontWheelOppositeRotate(int pwm)
{
  analogWrite(FRONT_IN1, pwm);
  analogWrite(FRONT_IN2, LOW);
}
void turnPWM(char direction, double speed, int angle)
{
  Serial.println("pwm,,,,,,,,");
  Serial.println(angle);
  if (speed < MIN_SPEED)
  {
    speed = MIN_SPEED;
  }

  if (direction == STOP)
  {
    stop();
    Serial.println(" stop car....");
  }
  else if (direction == UP)
  {
    if (angle > 180 && angle <= 270) // 120-270度，左前转向：左边轮子 转慢一点，右边轮子转快一点；左边轮子pwm 输出
    {
      //   前进
      long pwmLeft = map(angle - 180, 0, 90, MIN_SPEED, speed);
      //左边的两个轮 给pwm 信号，速度减慢，实现向左转
      leftRearWheelPositiveRotate(pwmLeft);
      rightRearWheelPositiveRotate(speed);
      leftFrontWheelPositiveRotate(pwmLeft);
      rightFrontWheelPositiveRotate(speed);
    }

    if (angle > 90 && angle <= 180) // 90—150，左前转向：左边轮子 反转，右边轮子正转；左边轮子pwm 输出
    {
      //   前进
      long pwmLeft = map(180 - angle, 0, 90, MIN_SPEED, speed);
      //左边的两个轮 给pwm 信号，反转，实现原地左转
      leftRearWheelOppositeRotate(pwmLeft);
      rightRearWheelPositiveRotate(speed);
      leftFrontWheelOppositeRotate(pwmLeft);
      rightFrontWheelPositiveRotate(speed);
    }
    else if (angle > 270 && angle <= 360)
    { //   前进
      long pwmRight = map(360 - angle, 0, 90, MIN_SPEED, speed);
      //右边的两个轮 给pwm 信号，速度减慢，实现向 右转

      leftRearWheelPositiveRotate(speed);
      rightRearWheelPositiveRotate(pwmRight);
      leftFrontWheelPositiveRotate(speed);
      rightFrontWheelPositiveRotate(pwmRight);
    }
    else if (angle > 0 && angle <= 90)
    { //   前进
      long pwmRight = map(angle, 0, 90, MIN_SPEED, speed);
      //右边的两个轮 给pwm 信号，反转，实现向 原地右转
      leftRearWheelPositiveRotate(speed);
      rightRearWheelOppositeRotate(pwmRight);
      leftFrontWheelPositiveRotate(speed);
      rightFrontWheelOppositeRotate(pwmRight);
    }
  }
  else //后退
  {
    if (angle > 90 && angle <= 180)
    { //  左 后 倒车
      long pwmLeft = map(180 - angle, 0, 90, MIN_SPEED, speed);
      //左边边的两个轮 给pwm 信号，速度减慢，实现向 左后 转
      leftRearWheelOppositeRotate(pwmLeft);
      rightRearWheelOppositeRotate(speed);

      leftFrontWheelOppositeRotate(pwmLeft);
      rightFrontWheelOppositeRotate(speed);
    }

    if (angle > 180 && angle <= 270)
    { //  左 后 倒车
      long pwmLeft = map(270 - angle, 0, 90, MIN_SPEED, speed);
      //左边边的两个轮 给pwm 信号，并  正转，实现向 原地 左后 旋转
      leftRearWheelPositiveRotate(pwmLeft);
      rightRearWheelOppositeRotate(speed);

      leftFrontWheelPositiveRotate(pwmLeft);
      rightFrontWheelOppositeRotate(speed);
    }

    else if (angle > 0 && angle <= 90)
    { //   右后 倒车
      long pwmRight = map(angle, 0, 90, MIN_SPEED, speed);
      //右边的两个轮 给pwm 信号，速度减慢，实现向 右后 转
      leftRearWheelOppositeRotate(speed);
      rightRearWheelOppositeRotate(pwmRight);
      leftFrontWheelOppositeRotate(speed);
      rightFrontWheelOppositeRotate(pwmRight);
    }
    else if (angle > 270 && angle <= 360)
    { //   右后 倒车
      long pwmRight = map(360 - angle, 0, 90, MIN_SPEED, speed);
      //右边的两个轮 给pwm 信号，并正转，实现 原地右后 旋转
      leftRearWheelOppositeRotate(speed);
      rightRearWheelPositiveRotate(pwmRight);
      leftFrontWheelOppositeRotate(speed);
      rightFrontWheelPositiveRotate(pwmRight);
    }
  }

  lastCommand = direction;
}

void turn(char data, int angle)
{
  Serial.println(angle);
  switch (data)
  {
  case STOP:
    stop();
    Serial.println(" stop car....");
    break;
  case UP:
    if (lastCommand != data)
    {
      stop();
    }
    Serial.println(" 前进....");
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    digitalWrite(in3, LOW);
    digitalWrite(in4, HIGH);

    digitalWrite(FRONT_IN1, LOW);
    digitalWrite(FRONT_IN2, HIGH);
    digitalWrite(FRONT_IN3, LOW);
    digitalWrite(FRONT_IN4, HIGH);
    break;

  case DOWN:
    if (lastCommand != data)
    {
      stop();
    }
    Serial.println(" 后退....");
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);

    digitalWrite(FRONT_IN1, HIGH);
    digitalWrite(FRONT_IN2, LOW);
    digitalWrite(FRONT_IN3, HIGH);
    digitalWrite(FRONT_IN4, LOW);

    break;

  case LEFT: // left
    if (lastCommand != data)
    {
      stop();
    }
    Serial.println(" 原地左转....");
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    digitalWrite(in3, LOW);
    digitalWrite(in4, HIGH);

    digitalWrite(FRONT_IN1, HIGH);
    digitalWrite(FRONT_IN2, LOW);
    digitalWrite(FRONT_IN3, LOW);
    digitalWrite(FRONT_IN4, HIGH);

    break;

  case RIGHT: // to right
    if (lastCommand != data)
    {
      stop();
    }
    Serial.println(" 原地右转....");
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);

    digitalWrite(FRONT_IN1, LOW);
    digitalWrite(FRONT_IN2, HIGH);
    digitalWrite(FRONT_IN3, HIGH);
    digitalWrite(FRONT_IN4, LOW);

    break;

  case UP_LEFT: //左前移动
    if (lastCommand != data)
    {
      stop();
    }
    // 右后轮不转动
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    // 左后轮 向前转动
    digitalWrite(in3, LOW);
    digitalWrite(in4, HIGH);
    //  右前轮 向前转动
    digitalWrite(FRONT_IN1, LOW);
    digitalWrite(FRONT_IN2, HIGH);
    //左前轮 不转动
    digitalWrite(FRONT_IN3, LOW);
    digitalWrite(FRONT_IN4, LOW);

    break;

  case DOWN_RIGHT: //右后移动
    if (lastCommand != data)
    {
      stop();
    }

    // 右后轮不转动
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    // 左后轮 向后转动
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    //  右前轮 向后转动
    digitalWrite(FRONT_IN1, HIGH);
    digitalWrite(FRONT_IN2, LOW);
    //左前轮 不转动
    digitalWrite(FRONT_IN3, LOW);
    digitalWrite(FRONT_IN4, LOW);

    break;

  case UP_RIGHT: //右前移动

    if (lastCommand != data)
    {
      stop();
    }
    // 右后轮 向前转动
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    // 左后轮 不转动
    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
    //  右前轮 不转动
    digitalWrite(FRONT_IN1, LOW);
    digitalWrite(FRONT_IN2, LOW);
    //左前轮 向前移动
    digitalWrite(FRONT_IN3, LOW);
    digitalWrite(FRONT_IN4, HIGH);

    break;

  case DOWN_LEFT: //左后前移动

    if (lastCommand != data)
    {
      stop();
    }
    // 右后轮 向后转动
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    // 左后轮 不转动
    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
    //  右前轮 不转动
    digitalWrite(FRONT_IN1, LOW);
    digitalWrite(FRONT_IN2, LOW);
    //左前轮 向后移动
    digitalWrite(FRONT_IN3, HIGH);
    digitalWrite(FRONT_IN4, LOW);

    break;

  case LEFT_TRANSLATION: //左平移

    if (lastCommand != data)
    {
      stop();
    }
    // 右后轮 向后转动
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    // 左后轮   向前
    digitalWrite(in3, LOW);
    digitalWrite(in4, HIGH);
    //  右前轮 向前转动
    digitalWrite(FRONT_IN1, LOW);
    digitalWrite(FRONT_IN2, HIGH);
    //左前轮 向后移动
    digitalWrite(FRONT_IN3, HIGH);
    digitalWrite(FRONT_IN4, LOW);

    break;

  case RIGHT_TRANSLATION: //右平移

    if (lastCommand != data)
    {
      stop();
    }
    // 右后轮 向后转动
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    // 左后轮   向前
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    //  右前轮 向前转动
    digitalWrite(FRONT_IN1, HIGH);
    digitalWrite(FRONT_IN2, LOW);
    //左前轮 向后移动
    digitalWrite(FRONT_IN3, LOW);
    digitalWrite(FRONT_IN4, HIGH);

    break;

  default:
    break;
  }

  lastCommand = data;
}

// void writeStringToFlash(const char *toStore, int startAddr)
// {
//   int i = 0;
//   for (; i < LENGTH(toStore); i++)
//   {
//     EEPROM.write(startAddr + i, toStore[i]);
//   }
//   EEPROM.write(startAddr + i, '\0');
//   EEPROM.commit();
// }

// String readStringFromFlash(int startAddr)
// {
//   char in[128]; // char array of size 128 for reading the stored data
//   int i = 0;
//   for (; i < 128; i++)
//   {
//     in[i] = EEPROM.read(startAddr + i);
//   }
//   return String(in);
// }