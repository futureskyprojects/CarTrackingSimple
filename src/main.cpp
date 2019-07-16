#include <Arduino.h>
#include <SoftwareSerial.h>
#include <avr/interrupt.h>

#define MAX_MISSED_MESSAGE 10
#define MAX_OPEN_GPRS_FAIL 15

#define DEBUG false
#define gpsPin 5
#define RX 3
#define TX 2

#define PIN_SIM808_POWER 9 // Chân nguồn sim808

#define PIN_SIGNAL_PWR A0   // Báo nguồn
#define PIN_SIGNAL_DATA A1  // Đang gửi dữ liệu
#define PIN_SIGNAL_GPS A2   // Báo GPS
#define PIN_SIGNAL_ERROR A3 // Báo cảnh - Báo lỗi

int signalPwr = LOW;
int signalData = LOW;
int signalGps = LOW;
int signalError = LOW;

int PIN_LIST[] = {PIN_SIGNAL_PWR, PIN_SIGNAL_DATA, PIN_SIGNAL_GPS, PIN_SIGNAL_ERROR};
int *signalAddr[] = {&signalPwr, &signalData, &signalGps, &signalError};

void setDigitalSignal(int Pin, int state)
{
  for (size_t i = 0; i < sizeof(PIN_LIST) / sizeof(int); i++)
  {
    if (Pin == PIN_LIST[i])
    {
      **(signalAddr + i) = state;
      digitalWrite(PIN_LIST[i], **(signalAddr + i));
    }
  }
}

SoftwareSerial mySerial(RX, TX);
/*------------- USER INPUT --------------------------------*/
String deviceIMEI = "";
char *serverAddress;
char *serverPort;
char *gpsParamPrefix;
char *imeiParamPrefix;
/*---------------------------------------------------------*/
String GpsData = "";
int prefixIndex = -1, surfixIndex = -1;
byte inCommingByte;
char atCommand[50];

//bolean flags
boolean isSettingUp = true;
boolean gprsReady = false;
boolean isSendingData = false;
boolean isGps3dFixed = false;
boolean isGettingGpsData = false;
boolean isOverMissedMessage = false;

int MissMessageCount = 0;

boolean isGPRSReady(void);
String sendData(String, const int, boolean);
void checkGpsStatus(void);
void getIMEI(void);
void getSetUp(void);
void pwrGps(void);
void getGpsData(void);

// Kéo nút nguồn
void powerPressHold()
{
  digitalWrite(PIN_SIM808_POWER, LOW);
  delay(1000);
  digitalWrite(PIN_SIM808_POWER, HIGH);
  delay(3000);
}

// Tắt tất cả các led hiệu
void turnOffAllLed()
{
  for (int j = 0; j < sizeof(PIN_LIST) / sizeof(int); j++)
  {
    setDigitalSignal(PIN_LIST[j], LOW);
  }
}

// Bật tất cả các led hiệu
void turnOnAllLed()
{
  for (int j = 0; j < sizeof(PIN_LIST) / sizeof(int); j++)
  {
    setDigitalSignal(PIN_LIST[j], HIGH);
  }
}

// Change state error led
void changeStateErrorSignalLed()
{
  if (signalError > 0)
    setDigitalSignal(PIN_SIGNAL_ERROR, LOW);
  else
    setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);
}

// Blink error led
void BlinkErrorSignalLed(int delay_ms = 500, int loop = 1)
{
  for (int i = 0; i < loop; i++)
  {
    delay(delay_ms);
    changeStateErrorSignalLed();
    delay(delay_ms);
    changeStateErrorSignalLed();
  }
}

// change state gps led
void changeStateGpsSignalLed()
{
  if (signalData > 0)
    setDigitalSignal(PIN_SIGNAL_GPS, LOW);
  else
    setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
}
// Blink gps led
void blinkGpsSignalLed(int delay_ms = 500, int loop = 1)
{
  for (int i = 0; i < loop; i++)
  {
    delay(delay_ms);
    changeStateGpsSignalLed();
    delay(delay_ms);
    changeStateGpsSignalLed();
  }
}

// change state data led
void changeStateDataSignalLed()
{
  if (signalData > 0)
    setDigitalSignal(PIN_SIGNAL_DATA, LOW);
  else
    setDigitalSignal(PIN_SIGNAL_DATA, HIGH);
}

// Blink power led
void blinkDataSignalLed(int delay_ms = 500, int loop = 1)
{
  for (int i = 0; i < loop; i++)
  {
    delay(delay_ms);
    changeStateDataSignalLed();
    delay(delay_ms);
    changeStateDataSignalLed();
  }
}

// change state power led
void changeStatePowerSignalLed()
{
  if (signalPwr > 0)
    setDigitalSignal(PIN_SIGNAL_PWR, LOW);
  else
    setDigitalSignal(PIN_SIGNAL_PWR, HIGH);
}

// Blink power led
void blinkPowerSignalLed(int delay_ms = 500, int loop = 1)
{
  for (int i = 0; i < loop; i++)
  {
    delay(delay_ms);
    changeStatePowerSignalLed();
    delay(delay_ms);
    changeStatePowerSignalLed();
  }
}

// Nháy tất cả các led hiệu theo số lần nhất định là loop, mặc định là 1 lần
void blinkAll(int loop = 1)
{
  for (int i = 0; i < loop; i++)
  {
    turnOffAllLed();
    delay(300);
    turnOnAllLed();
    delay(300);
  }
  turnOffAllLed();
}

void initPinMode()
{
  pinMode(PIN_SIM808_POWER, OUTPUT);
  pinMode(PIN_SIGNAL_DATA, OUTPUT);
  pinMode(PIN_SIGNAL_ERROR, OUTPUT);
  pinMode(PIN_SIGNAL_GPS, OUTPUT);
  pinMode(PIN_SIGNAL_PWR, OUTPUT);
  // Nháy thử kiểm tra các đèn xem có hoạt động không, để xem cái nào cháy, cái nào không
  blinkAll(2);
}
void resetBoardUno()
{
  asm volatile("jmp 0");
}

// Đợi cho sim808 thức dậy, nếu số lỗi vượt quá maxErr thì tiến hành bật nguồn
void waitSim808WakeUp(int maxErr = 5)
{
  int errCount = 0;
  while (sendData("AT", 1000, DEBUG).indexOf("OK") < 0)
  {
    blinkPowerSignalLed(100, 5);
    if (errCount >= maxErr)
    {
      turnOffAllLed();
      powerPressHold();
      resetBoardUno();
      return;
    }
    errCount++;
  }
  if (errCount < maxErr)
    setDigitalSignal(PIN_SIGNAL_PWR, HIGH);
}

void setup()
{
  Serial.begin(9600);   // Đặt serial baudrate của UNO là 9600
  mySerial.begin(9600); // Đặt serial baudrate của Sim808 là 9600 tương đương

  initPinMode();

  waitSim808WakeUp();
  sendData("ATE0", 1000, DEBUG);

  getIMEI();
  getSetUp();
  pwrGps();
  isSettingUp = false;
  setDigitalSignal(PIN_SIGNAL_PWR, HIGH); // Bật đèn hiệu nguồn lên luôn, thể hiện cho việc đã setup xong
}

void sendGpsData()
{
  while (!isGPRSReady()) // Wait until gprs available
  {
    if (MissMessageCount < MAX_MISSED_MESSAGE)
    {
      BlinkErrorSignalLed(200, 5);
      isOverMissedMessage = false;
    }
    else
    {
      isOverMissedMessage = true;
      setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);
    }
  }

  isSendingData = true;
  sendData("AT+HTTPINIT", 1200, DEBUG);
  String build = String("AT+HTTPPARA=\"URL\",\"http://") + String(serverAddress) + String(":") + String(serverPort) + String("/?") + String(gpsParamPrefix) + String("=") + String(GpsData) + String("&") + String(imeiParamPrefix) + String("="); //init the HTTP request
  build = String(build + deviceIMEI + String("\""));
  sendData(build, 4000, DEBUG);             // setting the httppara,
  sendData("AT+HTTPACTION=0", 3000, DEBUG); //submit the GET request
  String x = sendData("AT+HTTPREAD", 1000, DEBUG);
  mySerial.println("AT+HTTPTERM"); // terminate HTTP service
  isSendingData = false;
  if (x.indexOf("1") >= 0)
  {
    MissMessageCount = 0;
    setDigitalSignal(PIN_SIGNAL_DATA, LOW);
    delay(200);
    setDigitalSignal(PIN_SIGNAL_DATA, HIGH);
    delay(1000);
    setDigitalSignal(PIN_SIGNAL_DATA, LOW);
  }
  else
    MissMessageCount++;
  if (MissMessageCount >= MAX_MISSED_MESSAGE)
  {
    MissMessageCount = MAX_MISSED_MESSAGE;
    setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);
  }
}
void loop()
{
  waitSim808WakeUp(6);
  getGpsData();
  sendGpsData();
}

void getIMEI()
{
  int MAX_INTERVAL = 50;
  unsigned long startMilis = millis();
  String x = sendData("AT+GSN", 1000, true);
  if (x.length() < 15)
  {
    getIMEI();
    blinkPowerSignalLed(300, 1);
  }
  else
  {
    int firstIndex = -1;
    for (int i = 0; i < x.length(); i++)
    {
      if (millis() - startMilis >= MAX_INTERVAL)
      {
        startMilis = millis();
        changeStatePowerSignalLed();
      }
      if (x.charAt(i) >= '0' && x.charAt(i) <= '9')
      {
        firstIndex = i;
        break;
      }
    }
    deviceIMEI = x.substring(firstIndex, firstIndex + 15);
    blinkPowerSignalLed(1, 2);
  }
}
void getSetUp()
{
  mySerial.println("AT+CGATT=0");
  blinkPowerSignalLed(500, 2);
  int MAX_INTERVAL = 50;
  unsigned long startMillis = millis();
  int countTurnOnGprsFail = 0;
  while (sendData("AT+CGATT=1", 1000, true).indexOf("OK") < 0)
  {
    blinkPowerSignalLed(50, 10);
    countTurnOnGprsFail++;
    if (countTurnOnGprsFail > MAX_OPEN_GPRS_FAIL)
    {
      setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);
      mySerial.println("AT+CFUN=0");
      delay(5000);
      mySerial.println("AT+CFUN=1");
      delay(5000);
    }
    else
      blinkPowerSignalLed(50, 10);
  }
  if (countTurnOnGprsFail > MAX_OPEN_GPRS_FAIL)
  {
    delay(5000);
    waitSim808WakeUp();
  }
  countTurnOnGprsFail = 0;
  setDigitalSignal(PIN_SIGNAL_ERROR, LOW);
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
    if (millis() - startMillis > MAX_INTERVAL)
    {
      startMillis = millis();
      changeStatePowerSignalLed();
    }

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
  sendData("AT+HTTPTERM", 1000, true); // terminate HTTP service
}
void checkGpsStatus()
{
  String x = sendData("AT+CGPSSTATUS?", 1000, DEBUG);
  delay(50);
  if (x.indexOf("Location 3D Fix") < 0)
  {
    isGps3dFixed = false;
    setDigitalSignal(PIN_SIGNAL_GPS, LOW);
  }
  else
  {
    isGps3dFixed = true;
    setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
  }
}

void pwrGps()
{
  pinMode(gpsPin, OUTPUT);    // Đặt chân của GPS là chân ra
  digitalWrite(gpsPin, LOW);  // Nhấn chân GPS
  blinkGpsSignalLed(500, 5);  // Giữ 5s
  digitalWrite(gpsPin, HIGH); // Thả ra để thực hFiện bật GPS bằng chân cứng

  blinkPowerSignalLed(50, 10);
  sendData("AT+CGNSPWR=1", 1000, DEBUG); //power ON GPS/GNS module
  blinkGpsSignalLed(50, 20);
  sendData("AT+CGPSPWR=1", 1000, DEBUG); //power ON GPS/GNS module
  blinkGpsSignalLed(50, 20);
  sendData("AT+CGNSSEQ=RMC", 1000, DEBUG); //read GPRMC data
  blinkGpsSignalLed(50, 10);
  setDigitalSignal(PIN_SIGNAL_GPS, LOW);
}
String sendData(String command, const int timeout, boolean debug)
{
  int MAX_INTERVAL = 100;
  unsigned long startMillis = millis();

  String response = "";
  mySerial.println(command);
  long int time = millis();
  while ((time + timeout) > millis())
  {

    if (millis() - startMillis > MAX_INTERVAL)
    {
      startMillis = millis();
      if (isSettingUp)
        changeStatePowerSignalLed();
      if (isSendingData && !isOverMissedMessage)
        changeStateDataSignalLed();
      if (isGps3dFixed && isGettingGpsData)
        changeStateGpsSignalLed();
    }

    while (mySerial.available())
    {
      char c = mySerial.read();
      response += c;
    }
  }
  Serial.println(command + response);
  return response;
}

boolean isGPRSReady()
{
  String status = sendData("AT+CGATT?", 1000, DEBUG);
  if (status.indexOf("+CGATT: 1") < 0)
    return false;
  else
    return true;
}

void getGpsData()
{
  checkGpsStatus();
  if (isGps3dFixed)
    isGettingGpsData = true;
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
  if (isGps3dFixed)
  {
    setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
    isGettingGpsData = false;
  }
  else
    setDigitalSignal(PIN_SIGNAL_GPS, LOW);
}