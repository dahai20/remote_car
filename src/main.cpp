#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <BluetoothSerial.h>

// 0 5 14 15 ，启动时候是 pwm 输出，是高电平，不能用着 控制电机，否则上电的时候，小车就会往前跑
// 34 35 36 39 只能 输入
#define in1 32
#define in2 33
#define in3 25
#define in4 26

#define FRONT_IN1 4
#define FRONT_IN2 16
#define FRONT_IN3 17
#define FRONT_IN4 18

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
const char *TOPIC = "car_control";
const char *client_id = "clientId-ApjJZcy9Dh";
// 订阅信息主题
BluetoothSerial SerialBt; // esp32 经典蓝牙 4.0 以下
WiFiClient espClient;
PubSubClient client;
void callback(char *topic, byte *payload, unsigned int length);
void turn(char data);
long lastMsg = 0;
long lastReceiveMsg = 0;
/**
 * 连接wifi

 * */
void connectWifi()
{

  WiFi.begin("Xiaomi_FDD9", "lihai2070*");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.println('.');
  }
  Serial.println(WiFi.localIP());
}
void stop()
{
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(FRONT_IN1, LOW);
  digitalWrite(FRONT_IN2, LOW);
  digitalWrite(FRONT_IN3, LOW);
  digitalWrite(FRONT_IN4, LOW);

  delay(50);
}

void setup()
{
  connectWifi();
  client.setClient(espClient);
  // client.setServer("192.168.31.35", 1883);//tcp://broker-cn.emqx.io
  client.setServer("broker-cn.emqx.io", 1883); // tcp://broker-cn.emqx.io
  client.setCallback(callback);

  SerialBt.begin("LiXiangSmartCar");
  SerialBt.setPin("1234");
  Serial.begin(9600);
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
  Serial.print("Message arrived [");
  Serial.print(topic); // 打印主题信息
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]); // 打印主题内容
  }
  Serial.println();

  // if ((char)payload[0] == '1') {
  //   digitalWrite(BUILTIN_LED, HIGH);   // 亮灯
  // } else {
  //   digitalWrite(BUILTIN_LED, LOW);   // 熄灯
  // }

  turn((char)payload[0]);
  lastReceiveMsg = millis();
}

void reconnect()
{
  while (!client.connected())
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
}

/**
 * 读取蓝牙命令消息
 **/
void serialReadCMD()
{
  if (SerialBt.available())
  {
    char data = SerialBt.read();
    turn(data);
  }
  delay(15);
}

void loop()
{

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000)
  {
    lastMsg = now;
    client.publish("home/status/", "{device:client_id,'status':'on'}");
  }
  if (now - lastReceiveMsg > 3000) // 3s 没有收到命令，让小车停止
  {
    turn('0');
  }

  // serialReadCMD();
}

void turn(char data)
{

  switch (data)
  {
  case STOP:
    stop();
    Serial.println(" 停止....");
    break;
  case UP:
    stop();
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
    stop();

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
    stop();
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
    stop();
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
    stop();
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
    stop();

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

    stop();
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

    stop();
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

    stop();
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

    stop();
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
}