#include <Arduino.h>
#include <SoftwareSerial.h>

#define DEBUG true
#define gpsPin 5
#define RX 3
#define TX 2

#define PIN_SIM808_PWR A0   // Báo nguồn
#define PIN_SIM808_DATA A1  // Đang gửi dữ liệu
#define PIN_SIM808_GPS A2   // Báo GPS
#define PIN_SIM808_ERROR A3 // Báo cảnh - Báo lỗi

SoftwareSerial mySerial(RX, TX);
/*------------- USER INPUT --------------------------------*/
String deviceIMEI = "";
char *serverAddress;
char *serverPort;
char *gpsParamPrefix;
char *imeiParamPrefix;
/*---------------------------------------------------------*/
String GpsData = "";
//char Message[300];
int prefixIndex = -1, surfixIndex = -1;
byte inCommingByte;
char atCommand[50];

//bolean flags
boolean gprsReady = false;

boolean isGPRSReady(void);
String sendData(String, const int, boolean);
void checkGpsStatus(void);
void getIMEI(void);
void getSetUp(void);
void pwrGps(void);
void getGpsData(void);

void waitSim808WakeUp()
{
  while (sendData("AT", 1000, DEBUG).indexOf("OK") < 0)
  {
    delay(1000);
  }
}

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

  waitSim808WakeUp();

  sendData("ATE0", 1000, DEBUG);

  digitalWrite(TN_SIM_ON_PIN, HIGH);
  getIMEI();
  getSetUp();
  pwrGps();
  Serial.println("Finish set up!");
}

void loop()
{
  getGpsData();
  sendData("AT+HTTPINIT", 1200, DEBUG);
  String build = String("AT+HTTPPARA=\"URL\",\"http://") + String(serverAddress) + String(":") + String(serverPort) + String("/?") + String(gpsParamPrefix) + String("=") + String(GpsData) + String("&") + String(imeiParamPrefix) + String("="); //init the HTTP request
  build = String(build + deviceIMEI + String("\""));
  // String(deviceIMEI) +
  sendData(build, 4000, DEBUG);             // setting the httppara,
  sendData("AT+HTTPACTION=0", 3000, DEBUG); //submit the GET request
  String x = sendData("AT+HTTPREAD", 1000, DEBUG);
  Serial.println(x);
  sendData("AT+HTTPTERM", 1000, true); // terminate HTTP service                                                                     // read the data from the website you access
  //  sendGprsData();
}

void getIMEI()
{
  String x = sendData("AT+GSN", 1000, true);
  if (x.length() < 15)
  {
    getIMEI();
    delay(300);
  }
  else
  {
    int firstIndex = -1;
    for (int i = 0; i < x.length(); i++)
    {
      if (x.charAt(i) >= '0' && x.charAt(i) <= '9')
      {
        firstIndex = i;
        break;
      }
    }
    deviceIMEI = x.substring(firstIndex, firstIndex + 15);
    Serial.println("[" + deviceIMEI + "]");
    delay(100);
  }
}
void getSetUp()
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
      serverAddress = new char[strlen(pch) + 1];
      strncpy(serverAddress, pch, strlen(pch));
      serverAddress[strlen(pch)] = 0;
    }
    else if (i == 1)
    {
      serverPort = new char[strlen(pch) + 1];
      strncpy(serverPort, pch, strlen(pch));
      serverPort[strlen(pch)] = 0;
    }
    else if (i == 2)
    {
      gpsParamPrefix = new char[strlen(pch) + 1];
      strncpy(gpsParamPrefix, pch, strlen(pch));
      gpsParamPrefix[strlen(pch)] = 0;
    }
    else if (i == 3)
    {
      imeiParamPrefix = new char[strlen(pch) + 1];
      strncpy(imeiParamPrefix, pch, strlen(pch));
      imeiParamPrefix[strlen(pch)] = 0;
    }
    pch = strtok(NULL, " ");
    i++;
  }
  if (DEBUG)
  {
    Serial.println("INFOMATION");
    Serial.println("IMEI: " + String(deviceIMEI));
    Serial.println("Broker: " + String(serverAddress));
    Serial.println("Port: " + String(serverPort));
    Serial.println("User: " + String(gpsParamPrefix));
    Serial.println("Pass: " + String(imeiParamPrefix));
  }
  sendData("AT+HTTPTERM", 1000, true); // terminate HTTP service
  // sendData("AT+SAPBR=0,1", 1000, true);
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

void getGpsData()
{
  checkGpsStatus();
  GpsData = sendData("AT+CGNSINF", 1000, DEBUG);
  //+CGNSINF: 1,1,20161122182451.000,13.019292,77.686463,919.200,0.15,10.5,1,,0.9,2.0,1.8,,12,9,,,47,,
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
    GpsData.replace(" ", "");
  }
  else
  {
    GpsData = "NULL";
  }
}