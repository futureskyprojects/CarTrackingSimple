#include <CarTracking.h>

int signalPwr = LOW;   // Biến lưu trữ trạng thái của đèn hiệu nguồn
int signalData = LOW;  // Biến lưu trữ trạng thái của đền hiệu dữ liệu
int signalGps = LOW;   // Biến lưu trữ dữ liệu của đèn hiệu GPS
int signalError = LOW; // Biến lưu trữ trạng thái của đèn hiệu lỗi

int PIN_LIST[] = {PIN_SIGNAL_PWR, PIN_SIGNAL_DATA, PIN_SIGNAL_GPS, PIN_SIGNAL_ERROR}; // Danh sách các chân đèn hiệu
int *signalAddr[] = {&signalPwr, &signalData, &signalGps, &signalError};              // Danh sách trạng thái của các chân đèn hiệu

SoftwareSerial sim808Serial(RX, TX); // Tiến hành khai báo cổng giao tiếp serial cho sim808
String deviceIMEI = "";              // Biến lưu trữ IMEI của thiết bị

/* ------------------------------------ Danh sách các biến toàn cục, được setup 1 lần từ server ----------------------------- */
char *serverAddress;      // Biến lưu trữ địa chỉ của server
char *serverPort;         // Biến lưu trữ cổng kết nối đến server
char *gpsParamPrefix;     // Biến lưu trữ giá trị định hình cho tham số của gps trong url query
char *imeiParamPrefix;    // Biến lưu trữ giá trị định hình cho tham số của imei trong url query
char *balanceParamPrefix; // Biến lưu trữ giá trị định hình cho tham số của balance (tin nhắn tài khoản) trong url query
char *checkBalanceSyntax; // Biến lưu trữ cú pháp tin nhắn kiểm tra tài khoản của sim hiện hành

/*------------------------------------- Khai báo các biến toàn cục và chiếm hữu sẵn bộ nhớ cho chúng --------------------*/
String gpsData = "";      // Biến lưu trữ giá trị trả về của GPS
int MissMessageCount = 0; // Biến lưu trữ số gói tin lỗi liên tiếp hiện tại của thiết bị

/*------------------------------------- Danh sách các cờ logic ----------------------------------------------------------*/
boolean isSettingUp = true;               // Cờ kiểm tra có phải đang tiến hành hàm setup() hay không
boolean isSendingData = false;            // Cờ kiểm tra xem có đang gửi dữ liệu
boolean isGps3dFixed = false;             // Cờ kiểm tra xem tọa độ của GPS có được định vị Loaction 3d fixed hay chưa
boolean isGettingGpsData = false;         // Cờ kiểm tra xem có đang lấy dữ liệu GPS hay không
boolean isOverMissedMessage = false;      // Cờ kiểm tra xem thiết bị có gửi mất gói tin vượt quá giới hạn hay chưa
boolean isGprsAvailable = false;          // Cờ kiểm tra xem GPRS có đang hoạt động và có thể gửi tin hay không
boolean isRequestCheckSimBalance = false; // Cờ kiểm tra xem có yêu cầu kiểm tra tài khoản từ server hay không

/*------------------------------------ Tường minh các phương thức đã được khai báo ---------------------------------------*/
void setDigitalSignal(int Pin, int state) // Hàm cập nhật trạng thái tín hiệu cho các chân nằm trong danh sách đèn hiệu
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

// Kéo nút nguồn, phườn thức này khiến cho sim808 đổi trạng thái từ bật sang tắt và ngược lại
void powerPressHold()
{
    digitalWrite(PIN_SIM808_POWER, LOW);
    delay(1000);
    digitalWrite(PIN_SIM808_POWER, HIGH);
    delay(3000);
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

// Phương thức reset Board điều khiển Arduino UNO, sử dụng lệnh assembly để dịch thanh ghi về vị trí đầu tiên.
void resetBoardUno()
{
    asm volatile("jmp 0");
}

// Đợi cho đến khi GPRS khả dụng, nếu só lần kiểm tra vượt quá @MAX_WAIT_TIME_WHEN_LOST_GPRS thì tiến hành thử bật lại GPRS
void waitForGprsAvailable()
{
    int waitCount = 0;
    while (!checkGprsReady()) // Wait until gprs available
    {
        BlinkErrorSignalLed(200, 5);
        delay(1000);
        if (waitCount > MAX_WAIT_TIME_WHEN_LOST_GPRS)
        {
            tryToTurnOnGprs();
            break;
        }
    }
}

// Đợi cho sim808 thức dậy, nếu số lỗi vượt quá @maxErr thì tiến hành bật lại nguồn và kiểm tra lại
void waitForSim808WakeUp(int maxErr = 5)
{
    int errCount = 0;
    while (sendAtCommand("AT", 1000, DEBUG).indexOf("OK") < 0)
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

// Thử bật lại GPRS, nếu bật thử vượt quá số lần @MAX_OPEN_GPRS_FAIL_TIMES thì tiến hành chuyển mạch sang chế độ máy bay, đợi một lúc và chuyển lại chế độ bình thường, sau đó tiến hành thử bật lại, sau khi bật lại thành công, tiến hành đợi mạch thức dậy, sau khi hoàn tất, tiến hành kiểm tra lại GPRS có khả dụng hay không
void tryToTurnOnGprs()
{
    int countTurnOnGprsFail = 0;
    while (sendAtCommand("AT+CGATT=1", 1000, true).indexOf("OK") < 0)
    {
        blinkPowerSignalLed(50, 10);
        countTurnOnGprsFail++;
        if (countTurnOnGprsFail > MAX_OPEN_GPRS_FAIL_TIMES)
        {
            setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);
            sim808Serial.println("AT+CFUN=0");
            delay(5000);
            sim808Serial.println("AT+CFUN=1");
            delay(5000);
        }
        else
            blinkPowerSignalLed(50, 10);
    }
    if (countTurnOnGprsFail > MAX_OPEN_GPRS_FAIL_TIMES)
    {
        delay(5000);
        waitForSim808WakeUp();
    }
    waitForGprsAvailable();
    countTurnOnGprsFail = 0;
}

// Phương thức tổng hợp thiết lập cho mạch, bao gồm việc lấy các thông số cần thiết như IMEI, và các cấu hình từ xa từ mạng internet
void setupModule()
{
    isSettingUp = true;       // Cho biết đang trong chế độ setup
    Serial.begin(9600);       // Đặt serial baudrate của UNO là 9600
    sim808Serial.begin(9600); // Đặt serial baudrate của Sim808 là 9600 tương đương

    if (DEBUG)
        Serial.println("Start setting up...");
    if (DEBUG)
        Serial.println("Start init Pin Mode...");
    initPinMode(); // Thiết lập các pinMode đầu tiên để có thể thực hiện thông báo qua đèn hiệu
    if (DEBUG)
        Serial.println("Finish init Pin mode.");
    if (DEBUG)
        Serial.println("Waking up device....");
    waitForSim808WakeUp();              // Đợi cho mạch thức dậy, nếu không, tiến hành khởi dộng lại mạch
    sendAtCommand("ATE0", 1000, DEBUG); // Tắt chế độ echo (vọng) lệnh đã gửi
    if (DEBUG)
        Serial.println("Waked up and turned off echo AT.");
    if (DEBUG)
        Serial.println("Getting device IMEI...");
    getIMEI(); // tiến hành lấy IMEI của thiết bị
    if (DEBUG)
        Serial.println("Getted IMEI.");
    if (DEBUG)
        Serial.println("Getting set up from internet...");
    getSetUpFromInternet(); // Tiến hành lấy các thông tin từ internet
    if (DEBUG)
        Serial.println("Finish set up from internet...");
    if (DEBUG)
        Serial.println("Powering on GPS...");
    powerOnGps(); // Bật GPS lên
    if (DEBUG)
        Serial.println("Powerd on GPS.");
    isSettingUp = false;                    // Cho biết đã kết thúc chế độ setup
    setDigitalSignal(PIN_SIGNAL_PWR, HIGH); // Bật đèn hiệu nguồn lên luôn, thể hiện cho việc đã setup xong
    if (DEBUG)
        Serial.println("Finish Main Set up!");
}

// Hàm khởi tạo HTTP và xây dựng các tham số cho truy vấn trong chuỗi url, sau đó gửi qua sim808 luôn
void initHttpAndBuildUrlQuery()
{
    String balanceMsgRecived = "";                              // Khởi tạo biến chứa tin nhắn phản hồi từ việc yêu cầu kiểm tra tài khoản
    if (isRequestCheckSimBalance)                               // Nếu có yêu cầu kiểm tra tài khoản từ phía server
        balanceMsgRecived = getBalanceAndWriteToSim808Serial(); // Thì tiến hành gọi phương thức kiểm tra và lưu trũ giá trị tin nhắn phản hồi vào biến
    sendAtCommand("AT+HTTPINIT", 1200, DEBUG);                  // Tiến hành khởi tạo giao thức HTTP nếu nó chưa có, có rồi thì sẽ trả về ERROR, nên cứ kệ
    /* Khu vực tiến hành xây dụng các tham số, đồng thời gửi qua sim808 thông qua serial tức thì
     * Các tham số ngoài này sẽ là mặc định cho mọi gói tin, bao gồm gps=<giá trị gps>&imei=<imei của thiết bị>
     */
    sim808Serial.write("AT+HTTPPARA=\"URL\",\"http://");
    if (DEBUG)
        Serial.write("AT+HTTPPARA=\"URL\",\"http://");

    sim808Serial.write(serverAddress);
    if (DEBUG)
        Serial.write(serverAddress);

    sim808Serial.write(":");
    if (DEBUG)
        Serial.write(":");

    sim808Serial.write(serverPort);
    if (DEBUG)
        Serial.write(serverPort);

    sim808Serial.write("/?");
    if (DEBUG)
        Serial.write("/?");

    sim808Serial.write(gpsParamPrefix);
    if (DEBUG)
        Serial.write(gpsParamPrefix);

    sim808Serial.write("=");
    if (DEBUG)
        Serial.write("=");

    sim808Serial.write(gpsData.c_str());
    if (DEBUG)
        Serial.write(gpsData.c_str());

    sim808Serial.write("&");
    if (DEBUG)
        Serial.write("&");

    sim808Serial.write(imeiParamPrefix);
    if (DEBUG)
        Serial.write(imeiParamPrefix);

    sim808Serial.write("=");
    if (DEBUG)
        Serial.write("=");

    sim808Serial.write(deviceIMEI.c_str());
    if (DEBUG)
        Serial.write(deviceIMEI.c_str());

    // Đây sẽ là vùng giá trị tùy chọn, khi có yêu cầu kiểm tra tài khoản từ phía server
    if (isRequestCheckSimBalance)
    {
        sim808Serial.write("&");
        if (DEBUG)
            Serial.write("&");

        sim808Serial.write(balanceParamPrefix);
        if (DEBUG)
            Serial.write(balanceParamPrefix);

        sim808Serial.write("=");
        if (DEBUG)
            Serial.write("=");
        for (size_t i = 0; i < balanceMsgRecived.length(); i++)
        { // Sau khi nhận đuuợc tin nhắn, tiến hành lập qua từng ký tự trong tin nhắn này
            if (balanceMsgRecived.charAt(i) != '\r' && balanceMsgRecived.charAt(i) != '\n')
            { // Nếu ký tự này không phải là một trong các ký tự xuống hàng thì tiếp tục các lệnh sau, không thì tiến hành bỏ qua phần trong này
                if (balanceMsgRecived.charAt(i) == ' ')
                { // Nếu ký tự hiện tại là dấu cách, để đảm bảo tiêu chuẩn cho HTTP request URL, tiến hành đổi dấu cách thành dấu '_' để tạo tính liên kết trong URL
                    sim808Serial.write('_');
                    if (DEBUG)
                        Serial.write('_');
                }
                else
                { // Nếu ký tự này là các ký tự thông thường, thì tiến hành in ra một cách nhẹ nhàng :))
                    sim808Serial.write(balanceMsgRecived.charAt(i));
                    if (DEBUG)
                        Serial.write(balanceMsgRecived.charAt(i));
                }
            }
        }
    }
    // Để kết thúc phần xây dựng các tham số, ta tiến hành gửi lệnh ngắt dòng để xác nhận
    sim808Serial.write("\"\r\n");
    if (DEBUG)
        Serial.write("\"\r\n");
    blinkDataSignalLed(50, 15); // Đợi khoảng 1.5s để lệnh được gửi hoàn tất
}

// hàm gửi giữ liệu GPS đã lấy được đến server
void sendGpsData()
{
    String res = "";            // Biến tạm để lưu trữ giá trị kết quả trả về từ các lệnh AT cần thiết phía bên dưới
    waitForGprsAvailable();     // Kiểm tra, đợi GPRS có ổn không trước khi gửi, nếu không thì thực hiện các hàm bật lại ở phía trên
    isSendingData = true;       // Cho biết rằng đang trong quá trình gửi dữ liệu
    initHttpAndBuildUrlQuery(); // Tiến hành gọi hàm thiết lập HTTP và gửi đi các tham số trong URL query

    res = sendAtCommand("AT+HTTPACTION=0", 3000 + (isRequestCheckSimBalance ? 2000 : 0), DEBUG); // Tiến hành đẩy toàn bộ dữ liệu đã chuẩn bị lên server, thời gian đẩy phụ thuộc vào độ dài tin, nên khi có yêu cầu kiểm tra tài khoản, chiều dài tin nhắn hầu như gấp đôi nên tiến hành tăng thời gian gửi lên
    if (res.indexOf("+HTTPACTION: 0,60") < 0)
    {                                                    // Nếu kết quả của hành động này không thuộc mã 60x (601 hoặc 604) thì đã gửi thành công, và thực hiện các lệnh dưới
        res = sendAtCommand("AT+HTTPREAD", 1000, DEBUG); // Tiến hành đọc dữ liệu
        sim808Serial.println("AT+HTTPTERM");             // Ngắt dịch vụ HTTP sau khi đã hoàn tất
        isSendingData = false;                           // Thông báo việc gửi tin nhắn đã hoàn tất
        boolean isSendSuccessed = false;                 // Khai báo cờ kiểm tra gửi tin nhắn đã chắc chắn thành công hay chưa
        if (res.indexOf("{1,1}") >= 0)
        {                                                      // Nếu lệnh trả về là {1,1}, số 1 đầu tiên là gửi thành công, số thứ 2 là mã lệnh, ở đây có mã lệnh yêu cầu kiểm tra tài khoản
            isRequestCheckSimBalance = isSendSuccessed = true; // Đặt cờ yêu cầu kiểm tra tài khỏan, và cờ gửi thành công thành true (đúng)
        }
        else if (res.indexOf("{1,0}") >= 0)
        { // Nếu lệnh trả về là {1,0}, số 1 đầu tiên tương tự, nhưng số 0 biể thị không yêu cầu kiểm tra tài khoản
            // Vì vậy tiến hành đặt cờ yêu cầu kiểm tra tài khoản là false (sai), và cờ gửi tin nhắn thành công là phủ định của chúng true (đúng)
            isSendSuccessed = !(isRequestCheckSimBalance = false);
        }
        else
        {
            // Các trường hợp khác coi như là sai, và set cả hai cờ về false (sai)
            isSendSuccessed = isRequestCheckSimBalance = false;
        }
        if (isSendSuccessed)
        {                                            // Sau tất cả, nếu cờ kiểm tra thành công là đúng
            MissMessageCount = 0;                    // Thiết lập cờ đếm số tin nhắn gửi sai liên tiếp về 0
            setDigitalSignal(PIN_SIGNAL_DATA, LOW);  // tắt đèn tín hiệu gửi 1 lúc
            delay(200);                              // Chờ
            setDigitalSignal(PIN_SIGNAL_DATA, HIGH); // Bật lại một lúc lâu
            delay(1000);                             // Chờ
            setDigitalSignal(PIN_SIGNAL_DATA, LOW);  // và cuối cùng là tắt đi
        }
        else
            MissMessageCount++; // Nếu cờ kiểm tra thành công là false (sai) thì tiến hành tăng cờ đếm số tin nhắn lỗi liên tiếp lên 1
        if (MissMessageCount >= MAX_MISSED_MESSAGE_PACKET)
        {                                                 // Nếu số tin nhắn lỗi liên tiếp đếm được lớn hơn hoặc bằng với @MAX_MISSED_MESSAGE_PACKET, thì
            MissMessageCount = MAX_MISSED_MESSAGE_PACKET; // Đặt só tin nhắn lỗi vẫn bằng giới hạn, k cho tăng nữa, tránh bị tràn bộ đệm (Buffer overflow)
            setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);     // Thiết lập đèn hiển thị lỗi sáng lên liên tục
        }
    }
    else if (res.indexOf("+HTTPACTION: 0,601") < 0)
    {                                 // Có nghĩa là server hiện không khả dụng, tức lỗi 604
        BlinkErrorSignalLed(1000, 5); // Nháy đèn lỗi 5 lần trong 10s
    }
    else
    {                                 // Là lỗi 601, tức do thiết bị có mạng yếu hoặc không thiết lập kết nối thành công
        BlinkErrorSignalLed(1000, 3); // Nháy đèn lỗi 3 lần trong 6s
    }
}

// Hàm kiểm tra tài khoản
String getBalanceAndWriteToSim808Serial()
{
    sendAtCommand("AT+CMGD=1,4", 1000, DEBUG);                                                                 // Xóa tất cả hộp thư đến, đề phòng hộp thư đầy
    String balanceMsgRecived = sendAtCommand("AT+CUSD=1,\"" + String(checkBalanceSyntax) + "\"", 5000, DEBUG); // Tiến hành gửi lệnh kiểm tra tài khoản
    if (balanceMsgRecived.indexOf("+CUSD: 2") >= 0)
    { // Nếu trả về kết quả này, tức là cú pháp kiểm tra tài khoản bị sai
        return "Syntax wrong";
    }
    else if (balanceMsgRecived.indexOf("+CUSD: 0, \"") >= 0)
    {                                                  // Nếu trả về kết quả này thì kết quả kiểm tra thành công, tiến hành chuẩn hóa
        balanceMsgRecived.replace("\r\n", "");         // Xóa bỏ tất cả các ký tự xuống hàng
        balanceMsgRecived.replace("\", 15", "");       // Xóa bỏ cuỗi surfix ", 15 của chuỗi
        balanceMsgRecived.replace("OK", "");           // Xóa chữ OK đầu chuỗi
        balanceMsgRecived.replace("+CUSD: 0, \"", ""); // Xóa prefix +CUSD: 0,
        return balanceMsgRecived;                      // Gửi chuỗi đã được chuẩn hóa đi
    }
    else
    { // Nếu không nằm trong tất cả các trường hợp trên, lệnh kiểm tra tài khoản đã thất bại
        return "Get Fail";
    }
}

// Hàm lấy IMEI của thiết bị
void getIMEI()
{
    size_t MAX_INTERVAL = 50;                       // Thiết lập khoảng thời gian giãn cách giữa các lần nháy đèn
    unsigned long startMilis = millis();            // Thời gian bắt đầu đoạn kiểm tra nháy
    String x = sendAtCommand("AT+GSN", 1000, true); // Tiens hành gửi lệnh AT lấy IMEI
    if (x.length() < 15)
    {                                // Chuỗi IMEI có độ dài là 15 ký tự, cộng các râu ria thì chắc chắn lớn hơn 15, nhưng nếu trả về nhở hơn 15 thì có vấn đế
        getIMEI();                   // tiến hành lấy IMEI lại
        blinkPowerSignalLed(300, 1); // nháy nhẹ vài cái
    }
    else
    {                        // Nếu thỏa mãn về độ dài, tiến hành chuẩn hóa chuỗi IMEI
        int firstIndex = -1; // Khai báo biến Index, là nơi mà ta gặp ký tự số đầu tiên trong chuỗi
        for (size_t i = 0; i < x.length(); i++)
        { // Lặp trong chuỗi IMEI
            if (millis() - startMilis >= MAX_INTERVAL)
            {                                // nếu bây giờ cách khoảng thời gian bắt đầu đủ rồi thì thiến hành nháy led và set thời gian kiểm tra lại là lúc này
                startMilis = millis();       // Đặt thời gian bắt đầu mới là lúc này
                changeStatePowerSignalLed(); // tiến hành lật trạng thái của led
            }
            if (x.charAt(i) >= '0' && x.charAt(i) <= '9')
            {                   // Nếu ta gặp được ký tự số
                firstIndex = i; // tiến hành đặt chỉ mục đầu là i
                break;          // thoát ngay khỏi vòng lặp
            }
        }
        deviceIMEI = x.substring(firstIndex, firstIndex + 15); // Tiến hành cắt chuỗi IMEI là từ chỉ mục đầu đến hết độ dài của IMEI là 15 ký tự
        blinkPowerSignalLed(1, 2);                             // nháy nhẹ con led
    }
}

// Hàm lấy các giá trị thiết lập từ phía internet
void getSetUpFromInternet()
{
    String res = "";                    // Khai báo biến chứa kết quả tạm thời
    sim808Serial.println("AT+CGATT=0"); // Tắt kết nối mạng GPRS
    blinkPowerSignalLed(500, 2);        // Đợi 2s
    tryToTurnOnGprs();                  // Thử bật lại mạng GPRS

    setDigitalSignal(PIN_SIGNAL_ERROR, LOW);                                                                                                    // Tắt đèn lỗi, nếu nó có sáng
    sendAtCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 1000, DEBUG);                                                                            // Đặt loại kết nối cho SAPBR là GPRS
    sendAtCommand("AT+SAPBR=3,1,\"APN\",\"\"", 2000, DEBUG);                                                                                    // Thiết lập APN, các trường thứ 2 trở đi trống cho phép làm việc với tất cả các mạng
    sendAtCommand("AT+SAPBR=1,1", 1000, DEBUG);                                                                                                 // Bật SAPBR
    sendAtCommand("AT+HTTPINIT", 1200, DEBUG);                                                                                                  // Khởi tạo HTTP                                                                                    //init the HTTP request
    res = sendAtCommand("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/apps/thinghttp/send_request?api_key=SLJ10VVO4K9FVGJX\"", 2000, DEBUG); // Thiết lập các tham số trong HTTP
    if (res.indexOf("+HTTPACTION: 0,60") >= 0)
    {                                             // Nếu kết quả trả về thuộc dạng 60x, thì cố gắng làm lại
        sendAtCommand("AT+HTTPTERM", 1000, true); // Đóng dịnh vụ HTTP đã có
        getSetUpFromInternet();                   // Gọi lại thiết lập
        return;                                   // Trở về để ngăn cản việc gọi các lệnh dưới
    }
    sendAtCommand("AT+HTTPACTION=0", 4000, DEBUG);                       // Đẩy các giá trị đi
    String x = sendAtCommand("AT+HTTPREAD", 1000, DEBUG);                // Đọc dữ liệu trả về
    x = x.substring(x.indexOf("+HTTPREAD: ") + 11, x.indexOf("OK") - 1); // Chuẩn hóa, loại bỏ prefix và surfix
    x = x.substring(x.indexOf("\r\n"));                                  // Xóa dấu xuống dòng
    x.replace("\r\n", "");                                               // Xóa dấu xuống dòng
    char str[x.length()];                                                // Khai báo biến tạm str để phục vụ việc cắt chuối để lấy tham số thiết lập
    x.toCharArray(str, x.length());                                      // Chuyển string thành mảng char
    char *pch;                                                           // Con trỏ tạm
    pch = strtok(str, " ");                                              // Ngắt chuỗi bởi dấu cánh
    int i = 0;                                                           // Đếm số chuỗi đã ngắt

    size_t MAX_INTERVAL = 50;             // Thời gian giới hạn cho led nháy
    unsigned long startMillis = millis(); // Thời gian khởi đầu đoạn lặp
    while (pch != NULL)
    { // Lặp cho đến hết chuỗi con trỏ đó
        if (millis() - startMillis > MAX_INTERVAL)
        {                                // Nếu thời gian hiện tại các thời gian đầu vòng lặp đủ với thời gian nháy led thì tiến hành cho lật led
            startMillis = millis();      // Gán thời gian bắt đầu mới, tức là thời gian lúc này
            changeStatePowerSignalLed(); // Lật trạng thái của led
        }
        if (i == 0)
        { // Nếu là chuỗi đầu thì gán cho dịa chỉ server
            serverAddress = new char[strlen(pch) + 1];
            strncpy(serverAddress, pch, strlen(pch));
            serverAddress[strlen(pch)] = 0;
        }
        else if (i == 1)
        { // Tiếp theo gán cho cổng server
            serverPort = new char[strlen(pch) + 1];
            strncpy(serverPort, pch, strlen(pch));
            serverPort[strlen(pch)] = 0;
        }
        else if (i == 2)
        { // Tiếp theo gán cho gps para
            gpsParamPrefix = new char[strlen(pch) + 1];
            strncpy(gpsParamPrefix, pch, strlen(pch));
            gpsParamPrefix[strlen(pch)] = 0;
        }
        else if (i == 3)
        { // gán cho imei para
            imeiParamPrefix = new char[strlen(pch) + 1];
            strncpy(imeiParamPrefix, pch, strlen(pch));
            imeiParamPrefix[strlen(pch)] = 0;
        }
        else if (i == 4)
        { // gán cho balance para
            balanceParamPrefix = new char[strlen(pch) + 1];
            strncpy(balanceParamPrefix, pch, strlen(pch));
            balanceParamPrefix[strlen(pch)] = 0;
        }
        else if (i == 5)
        { // Gán cho cú pháp kiểm tra tài khoản
            checkBalanceSyntax = new char[strlen(pch) + 1];
            strncpy(checkBalanceSyntax, pch, strlen(pch));
            checkBalanceSyntax[strlen(pch)] = 0;
        }
        pch = strtok(NULL, " "); // Cho các dấu cách là null để bỏ qua
        i++;                     // tăng biến số đếm
    }
    sendAtCommand("AT+HTTPTERM", 1000, true); // Ngắt dịch vụ HTTP
}

// Hàm kiểm tra trạng thái của GPS
void checkGpsStatus()
{
    String x = sendAtCommand("AT+CGPSSTATUS?", 1000, DEBUG); // Gửi lệnh kiểm tra trạng thái
    delay(50);                                               // Đợi sương sương 50ms
    if (x.indexOf("Location 3D Fix") < 0)
    {                                          // Nếu vị trí cưa phải Location 3D Fix thì
        isGps3dFixed = false;                  // Đặt cờ trạng thái fixed là false (sai)
        setDigitalSignal(PIN_SIGNAL_GPS, LOW); // tắt đèn hiệu GPS đi
    }
    else
    { // Ngược lại bật đèn hiệu lên và đặt cờ là true
        isGps3dFixed = true;
        setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
    }
}

// Hàm bật GPS
void powerOnGps()
{
    pinMode(PIN_GPS_PWR, OUTPUT);    // Đặt chân của GPS là chân ra
    digitalWrite(PIN_GPS_PWR, LOW);  // Nhấn chân GPS
    blinkGpsSignalLed(500, 5);       // Giữ 5s
    digitalWrite(PIN_GPS_PWR, HIGH); // Thả ra để thực hFiện bật GPS bằng chân cứng

    blinkPowerSignalLed(50, 10);
    sendAtCommand("AT+CGNSPWR=1", 1000, DEBUG); //power ON GPS/GNS module
    blinkGpsSignalLed(50, 20);
    sendAtCommand("AT+CGPSPWR=1", 1000, DEBUG); //power ON GPS/GNS module
    blinkGpsSignalLed(50, 20);
    sendAtCommand("AT+CGNSSEQ=RMC", 1000, DEBUG); //read GPRMC data
    blinkGpsSignalLed(50, 10);
    setDigitalSignal(PIN_SIGNAL_GPS, LOW);
}

// Hàm gửi lệnh AT chuẩn
String sendAtCommand(String command, const int timeout, boolean debug)
{
    size_t MAX_INTERVAL = 100;

    String response = "";
    sim808Serial.println(command);
    unsigned long startMillis = millis();
    const unsigned long timeStartMillis = millis();
    while ((timeStartMillis + timeout) > millis())
    {

        if (millis() - startMillis > MAX_INTERVAL)
        {
            startMillis = millis();
            if (isSettingUp)
                changeStatePowerSignalLed();
            if (isSendingData && !isOverMissedMessage && isGprsAvailable)
                changeStateDataSignalLed();
            if (isGps3dFixed && isGettingGpsData)
                changeStateGpsSignalLed();
        }

        while (sim808Serial.available())
        {
            char c = sim808Serial.read();
            response += c;
        }
    }
    if (DEBUG)
        Serial.println(command + response);
    return response;
}

// Hàm kiểm tra xem GPRS có sẵng sáng hay chưa
boolean checkGprsReady()
{
    String status = sendAtCommand("AT+CGATT?", 1000, DEBUG);
    if (status.indexOf("+CGATT: 1") < 0)
        return isGprsAvailable = false;
    else
        return isGprsAvailable = true;
}

// Hàm lấy trí trị GPRS
void getGpsData()
{
    checkGpsStatus();
    if (isGps3dFixed)
        isGettingGpsData = true;
    gpsData = sendAtCommand("AT+CGNSINF", 1000, DEBUG);
    //+CGNSINF: 1,1,20161122182451.000,13.019292,77.686463,919.200,0.15,10.5,1,,0.9,2.0,1.8,,12,9,,,47,,
    int prefixIndex = -1, // Cờ chỉ mục tiền tố trong chuỗi GPS, dùng để tìm prefix có dạng +CGNSINF:
        surfixIndex = -1; // Cờ chỉ mục hậu tố trong chuỗi GPS, dùng để tìm surfix có dạng OK
    // Cut prefix
    prefixIndex = gpsData.indexOf("+CGNSINF:");
    surfixIndex = gpsData.lastIndexOf("OK");
    surfixIndex = (surfixIndex >= 0 && surfixIndex < prefixIndex ? surfixIndex + 2 : gpsData.length());
    if (prefixIndex >= 0)
    {
        gpsData = gpsData.substring(prefixIndex, surfixIndex);
        gpsData.replace("\r\n", ""); // delete all new line
        gpsData.replace(" ", "");
        gpsData.replace("+CGNSINF:", "");
        gpsData.replace("OK", "");
    }
    else
    {
        gpsData = "NULL";
    }
    if (isGps3dFixed)
    {
        setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
        isGettingGpsData = false;
    }
    else
        setDigitalSignal(PIN_SIGNAL_GPS, LOW);
}