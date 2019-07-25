#ifndef _CAR_TRACKING_H_
#define _CAR_TRACKING_H_

#include <Arduino.h>
#include <SoftwareSerial.h>

#define MAX_MISSED_MESSAGE_PACKET 10        // Số lượng gói tin liên tiếp gửi không thành công, trước khi hiện đèn thông báo lỗi
#define MAX_OPEN_GPRS_FAIL_TIMES 15         // Số lần tối đa mở GPRS không thành công trước khi thực hiện đổ sáng chế độ máy bay rồi sang chế độ thường
#define MAX_WAIT_TIME_WHEN_LOST_GPRS 300000 // x phút đợi GPRS trước khi cho khởi động lại

#define DEBUG true // Sử dụng để bật hoặc tắt trạng thái debug
#define RX 3       // Chân RX mặc định
#define TX 2       // Chân TX mặc định

#define PIN_GPS_PWR 5      // Chân nguồn của GPS
#define PIN_SIM808_POWER 9 // Chân nguồn sim808

#define PIN_SIGNAL_PWR A0   // Báo nguồn
#define PIN_SIGNAL_DATA A1  // Đang gửi dữ liệu
#define PIN_SIGNAL_GPS A2   // Báo GPS
#define PIN_SIGNAL_ERROR A3 // Báo cảnh - Báo lỗi
/*------------------------------------ Danh sách các phương thức được định nghĩa ------------------------------------------*/
String sendAtCommand(String, const int, boolean);
void checkGpsStatus(void);
void powerPressHold(void);
void turnOffAllLed(void);
void turnOnAllLed(void);
void changeStateErrorSignalLed(void);
void BlinkErrorSignalLed(int, int);
void changeStateGpsSignalLed();
void blinkGpsSignalLed(int, int);
void changeStateDataSignalLed();
void blinkDataSignalLed(int, int);
void changeStatePowerSignalLed();
void blinkPowerSignalLed(int, int);
void blinkAll(int);
void initPinMode(void);
void resetBoardUno(void);
void waitForGprsAvailable(void);
void waitForSim808WakeUp(int);
void tryToTurnOnGprs(void);
void setupModule(void);
void initHttpAndBuildUrlQuery(void);
void sendGpsData(void);
String getBalanceAndWriteToSim808Serial(void);
void getIMEI(void);
void getSetUpFromInternet(void);
void powerOnGps(void);
boolean checkGprsReady(void);
void getGpsData(void);
void changeDataTypeGPS(void);
void phraseResult(String);
void resetGPSbyServer(void);
#endif