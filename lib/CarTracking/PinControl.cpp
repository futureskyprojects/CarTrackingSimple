#include <CarTracking.h>
int signalPwr = LOW;   // Biến lưu trữ trạng thái của đèn hiệu nguồn
int signalData = LOW;  // Biến lưu trữ trạng thái của đền hiệu dữ liệu
int signalGps = LOW;   // Biến lưu trữ dữ liệu của đèn hiệu GPS
int signalError = LOW; // Biến lưu trữ trạng thái của đèn hiệu lỗi

int PIN_LIST[] = {PIN_SIGNAL_PWR, PIN_SIGNAL_DATA, PIN_SIGNAL_GPS, PIN_SIGNAL_ERROR}; // Danh sách các chân đèn hiệu
int *signalAddr[] = {&signalPwr, &signalData, &signalGps, &signalError};              // Danh sách trạng thái của các chân đèn hiệu

// Khởi tạo và thiết lập các chân pin cho các led tín hiệu
void initPinMode()
{
    // Đặt tất cả chân led tín hiệu của led là chân đầu ra hết, tức đều là OUTPUT
    pinMode(PIN_SIM808_POWER, OUTPUT);
    pinMode(PIN_SIGNAL_DATA, OUTPUT);
    pinMode(PIN_SIGNAL_ERROR, OUTPUT);
    pinMode(PIN_SIGNAL_GPS, OUTPUT);
    pinMode(PIN_SIGNAL_PWR, OUTPUT);
    // Nháy thử kiểm tra các đèn xem có hoạt động không, để xem cái nào cháy, cái nào không
    blinkAll(2);
}

// Kéo nút nguồn, phườn thức này khiến cho sim808 đổi trạng thái từ bật sang tắt và ngược lại
void powerPressHold()
{
    digitalWrite(PIN_SIM808_POWER, LOW);
    delay(1000);
    digitalWrite(PIN_SIM808_POWER, HIGH);
    delay(3000);
}

// Hàm cập nhật trạng thái tín hiệu cho các chân nằm trong danh sách đèn hiệu
void setDigitalSignal(int Pin, int state)
{
    for (size_t i = 0; i < sizeof(PIN_LIST) / sizeof(int); i++) // Quét trong danh sách các chân đèn hiệu
    {
        if (Pin == PIN_LIST[i]) // Nếu chân nhập vào có trong danh sách chân đèn hiệu
        {
            **(signalAddr + i) = state;                    // Thì cập nhật trạng thái của chân đó trong danh sách trạng thái của các chân đèn hiệu
            digitalWrite(PIN_LIST[i], **(signalAddr + i)); // Sau đó thực hiện cập nhật trạng thái số của chân
        }
    }
}
// Tắt tất cả các led hiệu, tất cả các led hiệu trong danh sách sẽ được tắt, tức set về mức LOW (0) cho tất cả
void turnOffAllLed()
{
    for (size_t j = 0; j < (int)sizeof(PIN_LIST) / sizeof(int); j++)
    {
        setDigitalSignal(PIN_LIST[j], LOW);
    }
}

// Bật tất cả các led hiệu, tất cả các led hiệu sẽ được bật, tức set về mức HIGH (1) cho tất cả
void turnOnAllLed()
{
    for (size_t j = 0; j < sizeof(PIN_LIST) / sizeof(int); j++)
    {
        setDigitalSignal(PIN_LIST[j], HIGH);
    }
}

// Đổi trạng thái của led hiệu lỗi, khi led đang trại thái HIGH (1) thì chuyển sang LOW (0) và ngược lại
void changeStateErrorSignalLed()
{
    if (signalError > 0)
        setDigitalSignal(PIN_SIGNAL_ERROR, LOW);
    else
        setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);
}

// Nháy led hiệu lỗi theo định kỳ @delay_ms cho bật và tắt, lặp lại @loop lần. Như vậy tổng thời gian cho @loop lần nháy sẽ là 2*@delay_ms*@loop
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

// Đổi trạng thái của led hiệu Gps, tức là khi đang ở HIGH (1) sẽ chuyển sang LOW (0) và ngược lại
void changeStateGpsSignalLed()
{
    if (signalData > 0)
        setDigitalSignal(PIN_SIGNAL_GPS, LOW);
    else
        setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
}
// Nháy led hiệu GPS theo định kỳ @delay_ms cho bật và tắt, lặp lại @loop lần. Như vậy tổng thời gian cho @loop lần nháy sẽ là 2*@delay_ms*@loop
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

// Đổi trạng thái led tín hiệu đang gửi dữ liệu, tức là khi đang ở HIGH (1) sẽ chuyển thành LOW (0) và ngược lại
void changeStateDataSignalLed()
{
    if (signalData > 0)
        setDigitalSignal(PIN_SIGNAL_DATA, LOW);
    else
        setDigitalSignal(PIN_SIGNAL_DATA, HIGH);
}

// Nháy led hiệu đang gửi dữ liệu theo định kỳ @delay_ms cho bật và tắt, lặp lại @loop lần. Như vậy tổng thời gian cho @loop lần nháy sẽ là 2*@delay_ms*@loop
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

// Hàm đổi trạng thái chân của led hiệu nguồn sim808, nếu chân đang ở trạng thái HIGH (1) thì sẽ chuyển sang LOW (0) và ngược lại
void changeStatePowerSignalLed()
{
    if (signalPwr > 0)
        setDigitalSignal(PIN_SIGNAL_PWR, LOW);
    else
        setDigitalSignal(PIN_SIGNAL_PWR, HIGH);
}

// Nháy led hiệu nguồn của sim808 theo định kỳ @delay_ms cho bật và tắt, lặp lại @loop lần. Như vậy tổng thời gian cho @loop lần nháy sẽ là 2*@delay_ms*@loop
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

// Nháy tất cả các led hiệu theo số lần nhất định là @loop, mặc định là 1 lần
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
