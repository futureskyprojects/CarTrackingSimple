#include <Arduino.h>
#include <GsmMqtt.h>
#include <SoftwareSerial.h>

#define DEBUG true
#define gpsPin 5
#define RX 3
#define TX 2

#define TN_SIM_ON_PIN A3
#define TN_SERVER_CONNECTED_PIN A2
#define TN_GPS_FIX_PIN A1
#define TN_SEND_DATA_PIN A0

SoftwareSerial mySerial(RX, TX);
/*------------- USER INPUT --------------------------------*/
char *deviceName;
char *mqttBroker; // mqtt broker ip
char *mqttPort;   //mqtt broker port
char *mqttUser;
char *mqttPass;
char *mqttTopic;
String sosNum = "";
/*---------------------------------------------------------*/
String GpsData = "";
//char Message[300];
int prefixIndex = -1, surfixIndex = -1;
byte inCommingByte;
char atCommand[50];
byte mqttMessage[127];
int mqttMessageLength = 0;

//bolean flags
boolean gprsReady = false;
boolean mqttSent = false;
boolean isGetted = false;

void hangup(void);
void call(void);
void sos(void);
String buildJson(void);
void sendMQTTMessage(char *, char *, char *, char *, char *);
boolean isGPRSReady(void);
void sendGprsData(void);
String sendData(String, const int, boolean);
void checkGpsStatus(void);
void getIMEI(void);
void setMqttSetup(void);
void pwrGps(void);
void getGpsData(void);

void setup()
{
  pinMode(TN_GPS_FIX_PIN, OUTPUT);
  pinMode(TN_SEND_DATA_PIN, OUTPUT);
  pinMode(TN_SERVER_CONNECTED_PIN, OUTPUT);
  pinMode(TN_SIM_ON_PIN, OUTPUT);

  digitalWrite(TN_GPS_FIX_PIN, HIGH);
  digitalWrite(TN_SEND_DATA_PIN, HIGH);
  digitalWrite(TN_SERVER_CONNECTED_PIN, HIGH);
  digitalWrite(TN_SIM_ON_PIN, HIGH);

  delay(500);

  digitalWrite(TN_GPS_FIX_PIN, LOW);
  digitalWrite(TN_SEND_DATA_PIN, LOW);
  digitalWrite(TN_SERVER_CONNECTED_PIN, LOW);
  digitalWrite(TN_SIM_ON_PIN, LOW);
  
  delay(500);

  Serial.begin(9600);   // Đặt serial baudrate của UNO là 9600
  mySerial.begin(9600); // Đặt serial baudrate của Sim808 là 9600 tương đương

  while (sendData("AT", 1000, DEBUG).indexOf("OK") < 0)
  {
    delay(1000);
  }

  digitalWrite(TN_SIM_ON_PIN, HIGH);

  if (!isGetted)
  {
    getIMEI();
    setMqttSetup();
    isGetted = true;
    setup();
  }
  else
  {
    pwrGps();
    Serial.println("Finish set up!");
  }
}

void loop()
{
  getGpsData();
  sendGprsData();
}

void getIMEI()
{
  String x = sendData("AT+GSN", 1000, true);
  x.replace("\r\n", "");
  x = x.substring(x.indexOf("AT+GSN") + 6, x.indexOf("OK"));
  if (x.length() < 15)
  {
    getIMEI();
    delay(300);
  }
  else
  {
    x = "IMEI" + x;
    deviceName = new char[x.length() + 1];
    x.toCharArray(deviceName, x.length() + 1);
    delay(100);
  }
}
void setMqttSetup()
{
  sendData("AT+CGATT=0", 1000, true);
  delay(1000);
  while (sendData("AT+CGATT=1", 1000, true).indexOf("OK") < 0)
  {
    delay(1000);
  }
  sendData("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 1000, DEBUG); //setting the SAPBR,connection type is GPRS
  sendData("AT+SAPBR=3,1,\"APN\",\"\"", 2000, DEBUG);         //setting the APN,2nd parameter empty works for all networks
  sendData("AT+SAPBR=1,1", 1000, DEBUG);
  sendData("AT+HTTPINIT", 1200, DEBUG);                                                                                            //init the HTTP request
  sendData("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/apps/thinghttp/send_request?api_key=SLJ10VVO4K9FVGJX\"", 2000, DEBUG); // setting the httppara,
  sendData("AT+HTTPACTION=0", 4000, DEBUG);                                                                                        //submit the GET request
  String x = sendData("AT+HTTPREAD", 1000, DEBUG);                                                                                 // read the data from the website you access
  x = x.substring(x.indexOf("+HTTPREAD: ") + 11, x.indexOf("OK") - 1);
  x = x.substring(x.indexOf("\r\n"));
  x.replace("\r\n", "");
  char str[x.length()];
  x.toCharArray(str, x.length());
  char *pch;
  pch = strtok(str, " ");
  int i = 0;
  while (pch != NULL)
  {
    if (i == 0)
    {
      mqttBroker = new char[strlen(pch) + 1];
      strncpy(mqttBroker, pch, strlen(pch));
      mqttBroker[strlen(pch)] = 0;
    }
    else if (i == 1)
    {
      mqttPort = new char[strlen(pch) + 1];
      strncpy(mqttPort, pch, strlen(pch));
      mqttPort[strlen(pch)] = 0;
    }
    else if (i == 2)
    {
      mqttUser = new char[strlen(pch) + 1];
      strncpy(mqttUser, pch, strlen(pch));
      mqttUser[strlen(pch)] = 0;
    }
    else if (i == 3)
    {
      mqttPass = new char[strlen(pch) + 1];
      strncpy(mqttPass, pch, strlen(pch));
      mqttPass[strlen(pch)] = 0;
    }
    else if (i == 4)
    {
      mqttTopic = new char[strlen(pch) + 1];
      strncpy(mqttTopic, pch, strlen(pch));
      mqttTopic[strlen(pch)] = 0;
    }
    else if (i == 5)
    {
      sosNum = "ATD" + String(pch) + ";";
    }
    pch = strtok(NULL, " ");
    i++;
  }
  if (DEBUG)
  {
    Serial.println("INFOMATION");
    Serial.println("IMEI: " + String(deviceName));
    Serial.println("Broker: " + String(mqttBroker));
    Serial.println("Port: " + String(mqttPort));
    Serial.println("User: " + String(mqttUser));
    Serial.println("Pass: " + String(mqttPass));
    Serial.println("Topic: " + String(mqttTopic));
    Serial.println("SOS Num: " + String(sosNum));
  }
  sendData("AT+HTTPTERM", 1000, true); // terminate HTTP service
  sendData("AT+SAPBR=0,1", 1000, true);
}
void checkGpsStatus()
{
  String x = sendData("AT+CGPSSTATUS?", 1000, DEBUG);
  if (x.indexOf("Location 3D Fix") < 0)
  {
    // Serial.println(x);
    digitalWrite(TN_GPS_FIX_PIN, LOW);
  }
  else
  {
    digitalWrite(TN_GPS_FIX_PIN, HIGH);
  }
}

void pwrGps()
{
  pinMode(gpsPin, OUTPUT);    // Đặt chân của GPS là chân ra
  digitalWrite(gpsPin, LOW);  // Nhấn chân GPS
  delay(5000);                // Giữ 5s
  digitalWrite(gpsPin, HIGH); // Thả ra để thực hiện bật GPS bằng chân cứng

  Serial.print("GPS/GSM/GPRS Initializing");
  for (int i = 0; i < 10; i++) // Đợi tầm 10 s để GNS hoàn thành khởi tạo
  {
    Serial.print(".");
    delay(10);
  }
  sendData("AT+CGNSPWR=1", 1000, DEBUG); //power ON GPS/GNS module
  delay(2000);
  sendData("AT+CGPSPWR=1", 1000, DEBUG); //power ON GPS/GNS module
  delay(2000);
  sendData("AT+CGNSSEQ=RMC", 1000, DEBUG); //read GPRMC data
  delay(1000);
}
String sendData(String command, const int timeout, boolean debug)
{
  String response = "";
  mySerial.println(command);
  long int time = millis();
  while ((time + timeout) > millis())
  {
    while (mySerial.available())
    {
      char c = mySerial.read();

      response += c;
    }
  }
  if (debug)
  {
    Serial.println("----");
    Serial.println(command + ">");
    Serial.print(response);
    Serial.println("----");
  }
  return response;
}

void sendGprsData()
{
  gprsReady = isGPRSReady();
  if (gprsReady == false)
  {
    Serial.println("GPRS Not Ready");
  }
  if (gprsReady == true)
  {
    prefixIndex = -1;
    surfixIndex = -1;
    // Cut prefix
    prefixIndex = GpsData.indexOf("+CGNSINF:");
    surfixIndex = GpsData.lastIndexOf("OK");
    surfixIndex = (surfixIndex >= 0 && surfixIndex < prefixIndex ? surfixIndex + 2 : GpsData.length());
    if (prefixIndex >= 0)
    {
      GpsData = GpsData.substring(prefixIndex, surfixIndex);
      GpsData.replace("\r\n", ""); // delete all new line
      Serial.println(GpsData);
    }
    // Concert data
    char gpsData[GpsData.length() + 1];
    GpsData.toCharArray(gpsData, GpsData.length() + 1);
    //clientID, IP, Port, Topic, Message
    sendMQTTMessage(deviceName, mqttBroker, mqttPort, mqttTopic, gpsData);
  }
}

boolean isGPRSReady()
{
  mySerial.println("AT");
  mySerial.println("AT+CGATT?");
  while (mySerial.available())
    inCommingByte = (char)mySerial.read();
  if (inCommingByte > -1)
    return true;
  else
    return false;
}

void blinkDataLed(int seconds)
{
  for (int i = 0; i < seconds * 10; i++)
  {
    digitalWrite(TN_SEND_DATA_PIN, LOW);
    delay(50);
    digitalWrite(TN_SEND_DATA_PIN, HIGH);
    delay(50);
  }
}
void sendMQTTMessage(char *clientId, char *brokerUrl, char *brokerPort, char *topic, char *message)
{
  digitalWrite(TN_SEND_DATA_PIN, HIGH);
  mySerial.println("AT"); // Sends AT command to wake up cell phone
  Serial.println("send AT to wake up mySerial");
  delay(1000); // Wait a second
  // digitalWrite(13, HIGH);
  mySerial.println("AT+CSTT=\"www\",\"\",\"\""); // Puts phone into mySerial mode
  Serial.println("AT+CSTT=\"www\",\"\",\"\"");
  delay(2000); // Wait a second
  mySerial.println("AT+CIICR");
  Serial.println("AT+CIICR");
  delay(2000);
  mySerial.println("AT+CIFSR");
  Serial.println("AT+CIFSR");
  delay(1000);
  strcpy(atCommand, "AT+CIPSTART=\"TCP\",\"");
  strcat(atCommand, brokerUrl);
  strcat(atCommand, "\",\"");
  strcat(atCommand, brokerPort);
  strcat(atCommand, "\"");
  if (sendData(atCommand, 2000, DEBUG).indexOf("CONNECT") < 0)
  {
    digitalWrite(TN_SERVER_CONNECTED_PIN, LOW);
    delay(1000);
    return;
  }
  else
    digitalWrite(TN_SERVER_CONNECTED_PIN, HIGH);

  // mySerial.println(atCommand);
  // Serial.println(atCommand);
  // delay(1000);

  mySerial.println("AT+CIPSEND");
  Serial.println("AT+CIPSEND");
  delay(1000);

  mqttMessageLength = 14 + strlen(clientId);
  Serial.println(mqttMessageLength);
  mqtt_connect_message(mqttMessage, clientId);
  for (int j = 0; j < mqttMessageLength; j++)
    mySerial.write(mqttMessage[j]); // Message contents
  mySerial.write(byte(26));         // (signals end of message)
  Serial.println("Sending message");

  blinkDataLed(1);
  digitalWrite(TN_SEND_DATA_PIN, LOW);
  mySerial.println("AT+CIPSEND");
  Serial.println("AT+CIPSEND");
  delay(1000);
  mqttMessageLength = 4 + strlen(topic) + strlen(message);
  mqtt_publish_message(mqttMessage, topic, message);
  for (int k = 0; k < mqttMessageLength; k++)
    mySerial.write(mqttMessage[k]);
  mySerial.write(byte(26)); // (signals end of message)
  Serial.println("");
  Serial.println("-------------Sent-------------"); // Message contents
  blinkDataLed(2);
}

void call(void)
{
  sendData("AT+CSQ", 1000, DEBUG); // Kiểm tra chất lượng tín hiệu
  Serial.println("Making a call!");
  mySerial.println(sosNum); // xxxxxxxxx là số mà muốn sim thực hiện gọi đến (Ở đây là số của Nghĩa :D)
  if (mySerial.available())
    Serial.print((unsigned char)mySerial.read());
}

void hangup(void)
{
  mySerial.println("ATH"); // Kết thúc cuộc gọi
  if (mySerial.available())
    Serial.print((unsigned char)mySerial.read());
}
void getGpsData()
{
  checkGpsStatus();
  //+CGNSINF: 1,1,20161122182451.000,13.019292,77.686463,919.200,0.15,10.5,1,,0.9,2.0,1.8,,12,9,,,47,,
  GpsData = sendData("AT+CGNSINF", 1000, DEBUG);
}