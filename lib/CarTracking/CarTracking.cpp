#include <CarTracking.h>

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
String gpsData = ""; // Biến lưu trữ giá trị trả về của GPS
String currentLocationDataType = "RMC";
String networkLocation = "";
int MissMessageCount = 0; // Biến lưu trữ số gói tin lỗi liên tiếp hiện tại của thiết bị
int nextActionCode = 0;   // Biến lưu trữ giá trị của hành động tiếp theo để yêu cầu cho module thực hiện

/*------------------------------------- Danh sách các cờ logic ----------------------------------------------------------*/
boolean isSettingUp = true;          // Cờ kiểm tra có phải đang tiến hành hàm setup() hay không
boolean isSendingData = false;       // Cờ kiểm tra xem có đang gửi dữ liệu
boolean isGps3dFixed = false;        // Cờ kiểm tra xem tọa độ của GPS có được định vị Loaction 3d fixed hay chưa
boolean isGettingGpsData = false;    // Cờ kiểm tra xem có đang lấy dữ liệu GPS hay không
boolean isOverMissedMessage = false; // Cờ kiểm tra xem thiết bị có gửi mất gói tin vượt quá giới hạn hay chưa
boolean isGprsAvailable = false;     // Cờ kiểm tra xem GPRS có đang hoạt động và có thể gửi tin hay không
boolean isGetRawGpsLocationData = false;
boolean isAlwaysGetNetworkLocation = false;

/*----------------------------------- Khai báo cấu trúc chứa dữ liệu trả về từ server ------------------------------------*/
struct _SendResult
{
    bool sendStatusCode = false;
    int nextActionCode = 0;
} sendResult;

/*------------------------------------ Tường minh các phương thức đã được khai báo ---------------------------------------*/
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
    res = sendAtCommand("AT+HTTPACTION=0", 4000, DEBUG);                                                                                        // Đẩy các giá trị đi
    if (res.indexOf("+HTTPACTION: 0,60") >= 0)
    {                                             // Nếu kết quả trả về thuộc dạng 60x, thì cố gắng làm lại
        sendAtCommand("AT+HTTPTERM", 1000, true); // Đóng dịnh vụ HTTP đã có
        getSetUpFromInternet();                   // Gọi lại thiết lập
        return;                                   // Trở về để ngăn cản việc gọi các lệnh dưới
    }
    res = sendAtCommand("AT+HTTPREAD", 1000, DEBUG);                             // Đọc dữ liệu trả về
    res = res.substring(res.indexOf("+HTTPREAD: ") + 11, res.indexOf("OK") - 1); // Chuẩn hóa, loại bỏ prefix và surfix
    res = res.substring(res.indexOf("\r\n"));                                    // Xóa dấu xuống dòng
    res.replace("\r\n", "");                                                     // Xóa dấu xuống dòng
    char str[res.length()];                                                      // Khai báo biến tạm str để phục vụ việc cắt chuối để lấy tham số thiết lập
    res.toCharArray(str, res.length());                                          // Chuyển string thành mảng char
    char *pch;                                                                   // Con trỏ tạm
    pch = strtok(str, " ");                                                      // Ngắt chuỗi bởi dấu cánh
    int i = 0;                                                                   // Đếm số chuỗi đã ngắt

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
    sendAtCommand("AT+HTTPTERM", 1000, DEBUG); // Ngắt dịch vụ HTTP
    if (strlen(serverAddress) < 5 ||
        strlen(serverPort) < 1 ||
        strlen(gpsParamPrefix) < 1 ||
        strlen(imeiParamPrefix) < 1 ||
        strlen(balanceParamPrefix) < 1 ||
        strlen(checkBalanceSyntax) < 1)
    {
        resetBoardUno();
        return;
    }
    else if (DEBUG)
    {
        Serial.println("\n\n===== GETTED INFOMATION ======");
        Serial.print("* Server Address: [");
        Serial.print(serverAddress);
        Serial.println("]");
        Serial.print("* Server Port: [");
        Serial.print(serverPort);
        Serial.println("]");
        Serial.print("* GPS Prefix: [");
        Serial.print(gpsParamPrefix);
        Serial.println("]");
        Serial.print("* IMEI Prefix: [");
        Serial.print(imeiParamPrefix);
        Serial.println("]");
        Serial.print("* Balance Prefix: [");
        Serial.print(balanceParamPrefix);
        Serial.println("]");
        Serial.print("* Balance Check Syntax: [");
        Serial.print(checkBalanceSyntax);
        Serial.println("]");
        Serial.println("=========================\r\n");
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
    sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
    blinkGpsSignalLed(50, 10);
    setDigitalSignal(PIN_SIGNAL_GPS, LOW);
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
        Serial.println("Wait for sim Ready");
    while (sendAtCommand("AT+CREG?", 1000, DEBUG).indexOf("+CREG: 0,1") < 0)
    {
        BlinkErrorSignalLed(10, 200);
    }
    if (DEBUG)
        Serial.println("Sim was ready!");
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

// Hàm khởi tạo HTTP và xây dựng các tham số cho truy vấn trong chuỗi url, sau đó gửi qua sim808 luôn
void initHttpAndBuildUrlQuery()
{
    String balanceMsgRecived = "";                              // Khởi tạo biến chứa tin nhắn phản hồi từ việc yêu cầu kiểm tra tài khoản
    if (nextActionCode == 1)                                    // Nếu có yêu cầu kiểm tra tài khoản từ phía server
        balanceMsgRecived = getBalanceAndWriteToSim808Serial(); // Thì tiến hành gọi phương thức kiểm tra và lưu trũ giá trị tin nhắn phản hồi vào biến
    sendAtCommand("AT+HTTPINIT", 1200, DEBUG);                  // Tiến hành khởi tạo giao thức HTTP nếu nó chưa có, có rồi thì sẽ trả về ERROR, nên cứ kệ
    /* Khu vực tiến hành xây dụng các tham số, đồng thời gửi qua sim808 thông qua serial tức thì
     * Các tham số ngoài này sẽ là mặc định cho mọi gói tin, bao gồm gps=<giá trị gps>&imei=<imei của thiết bị>
     */
    sim808Serial.write("AT+HTTPPARA=\"URL\",\"http://");
    if (DEBUG)
        Serial.write("AT+HTTPPARA=\"URL\",\"http://");

    // Viết địa chỉ máy chủ vào terminal
    sim808Serial.write(serverAddress);
    if (DEBUG)
        Serial.write(serverAddress);

    sim808Serial.write(":");
    if (DEBUG)
        Serial.write(":");

    // Đưa port
    sim808Serial.write(serverPort);
    if (DEBUG)
        Serial.write(serverPort);

    // Bắt đầu query
    sim808Serial.write("/?");
    if (DEBUG)
        Serial.write("/?");

    // tiền tố tham số 'gps'
    sim808Serial.write(gpsParamPrefix);
    if (DEBUG)
        Serial.write(gpsParamPrefix);

    sim808Serial.write("=");
    if (DEBUG)
        Serial.write("=");

    // giá trị gps
    sim808Serial.write(gpsData.c_str());
    if (DEBUG)
        Serial.write(gpsData.c_str());

    sim808Serial.write("&");
    if (DEBUG)
        Serial.write("&");

    // tiền tố tham số imei
    sim808Serial.write(imeiParamPrefix);
    if (DEBUG)
        Serial.write(imeiParamPrefix);

    sim808Serial.write("=");
    if (DEBUG)
        Serial.write("=");

    // Giá trị imei
    sim808Serial.write(deviceIMEI.c_str());
    if (DEBUG)
        Serial.write(deviceIMEI.c_str());

    // Đây sẽ là vùng giá trị tùy chọn, khi có yêu cầu kiểm tra tài khoản từ phía server
    if (nextActionCode == 1)
    {
        sim808Serial.write("&");
        if (DEBUG)
            Serial.write("&");

        // Tiền tố tham só tài khoản
        sim808Serial.write(balanceParamPrefix);
        if (DEBUG)
            Serial.write(balanceParamPrefix);

        sim808Serial.write("=");
        if (DEBUG)
            Serial.write("=");

        // thực hiện xử lý để in ra giá trị là nôi dung của tin nhan kiểm tra tài khoản
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
    if (isAlwaysGetNetworkLocation || sendResult.nextActionCode == 401)
    {
        sim808Serial.write("&nl=");
        if (DEBUG)
            Serial.write("&nl=");
        int commaCount = 0;
        for (size_t i = 0; i < networkLocation.length(), commaCount <= 3; i++)
        { // Sau khi nhận đuuợc tin nhắn, tiến hành lập qua từng ký tự trong tin nhắn này
            if (networkLocation.charAt(i) != '\r' && networkLocation.charAt(i) != '\n')
            { // Nếu ký tự này không phải là một trong các ký tự xuống hàng thì tiếp tục các lệnh sau, không thì tiến hành bỏ qua phần trong này
                if (networkLocation.charAt(i) == ' ' || networkLocation.charAt(i) == '/')
                { // Nếu ký tự hiện tại là dấu cách, để đảm bảo tiêu chuẩn cho HTTP request URL, tiến hành đổi dấu cách thành dấu '_' để tạo tính liên kết trong URL
                    sim808Serial.write('_');
                    if (DEBUG)
                        Serial.write('_');
                }
                else
                { // Nếu ký tự này là các ký tự thông thường, thì tiến hành in ra một cách nhẹ nhàng :))
                    if (networkLocation.charAt(i) == ',')
                        commaCount++;
                    sim808Serial.write(networkLocation.charAt(i));
                    if (DEBUG)
                        Serial.write(networkLocation.charAt(i));
                }
            }
        }
    }
    // phần đưa ra loại chuẩn của gps hiện tại
    sim808Serial.write("&type=");
    if (DEBUG)
        Serial.write("&type=");
    sim808Serial.write(currentLocationDataType.c_str());
    if (DEBUG)
        Serial.write(currentLocationDataType.c_str());
    if (isGetRawGpsLocationData)
    {
        sim808Serial.write("|GPS");
        if (DEBUG)
            Serial.write("|GPS");
    }
    else
    {
        sim808Serial.write("|GNSS");
        if (DEBUG)
            Serial.write("|GNSS");
    }
    // Để kết thúc phần xây dựng các tham số, ta tiến hành gửi lệnh ngắt dòng để xác nhận
    sim808Serial.write("\"\r\n");
    if (DEBUG)
        Serial.write("\"\r\n");
    blinkDataSignalLed(50, 15); // Đợi khoảng 1.5s để lệnh được gửi hoàn tất
}

// hàm xử lý và lấy kêt quả trả về từ server
void phraseResult(String res)
{
    // phân tích dữ liệu về việc gửi thành công hay không
    sendResult.sendStatusCode = res.indexOf("{1,") >= 0;
    if (res.indexOf(",0}") >= 0)
        sendResult.nextActionCode = 0;
    else if (res.indexOf(",1}") >= 0)
        sendResult.nextActionCode = 1;
    else if (res.indexOf(",100}") >= 0)
        sendResult.nextActionCode = 100;
    else if (res.indexOf(",101}") >= 0)
        sendResult.nextActionCode = 101;
    else if (res.indexOf(",102}") >= 0)
        sendResult.nextActionCode = 102;
    else if (res.indexOf(",103}") >= 0)
        sendResult.nextActionCode = 103;
    else if (res.indexOf(",200}") >= 0)
        sendResult.nextActionCode = 200;
    else if (res.indexOf(",201}") >= 0)
        sendResult.nextActionCode = 201;
    else if (res.indexOf(",202}") >= 0)
        sendResult.nextActionCode = 202;
    else if (res.indexOf(",300}") >= 0)
    {
        sendResult.nextActionCode = 300;
        isGetRawGpsLocationData = false;
    }
    else if (res.indexOf(",301}") >= 0)
    {
        sendResult.nextActionCode = 301;
        isGetRawGpsLocationData = true;
    }
    else if (res.indexOf(",400}") >= 0)
    {
        isAlwaysGetNetworkLocation = false;
        sendResult.nextActionCode = 400;
    }
    else if (res.indexOf(",401}") >= 0)
    {
        sendResult.nextActionCode = 401;
        isAlwaysGetNetworkLocation = false;
    }
    else if (res.indexOf(",402}") >= 0)
    {
        sendResult.nextActionCode = 402;
        isAlwaysGetNetworkLocation = true;
    }
}

// hàm gửi giữ liệu GPS đã lấy được đến server
void sendGpsData()
{
    String res = "";            // Biến tạm để lưu trữ giá trị kết quả trả về từ các lệnh AT cần thiết phía bên dưới
    isSendingData = true;       // Cho biết rằng đang trong quá trình gửi dữ liệu
    initHttpAndBuildUrlQuery(); // Tiến hành gọi hàm thiết lập HTTP và gửi đi các tham số trong URL query

    sendResult.nextActionCode = 0;
    res = sendAtCommand("AT+HTTPACTION=0", 3000, DEBUG); // Tiến hành đẩy toàn bộ dữ liệu đã chuẩn bị lên server, thời gian đẩy phụ thuộc vào độ dài tin, nên khi có yêu cầu kiểm tra tài khoản, chiều dài tin nhắn hầu như gấp đôi nên tiến hành tăng thời gian gửi lên
    if (res.indexOf("+HTTPACTION: 0,60") < 0)
    {                                                    // Nếu kết quả của hành động này không thuộc mã 60x (601 hoặc 604) thì đã gửi thành công, và thực hiện các lệnh dưới
        res = sendAtCommand("AT+HTTPREAD", 1000, DEBUG); // Tiến hành đọc dữ liệu
        if (DEBUG)
            Serial.println("Result from server: " + res);
        sendAtCommand("AT+HTTPTERM", 1000, true); // Đóng dịnh vụ HTTP đã có
        isSendingData = false;                    // Thông báo việc gửi tin nhắn đã hoàn tất
        phraseResult(res);                        // Phân tích dữ liệu kết quả trả về từ server sau khi hoàn tất gửi gói tin
        if (sendResult.sendStatusCode)
        {                         // Sau tất cả, nếu cờ kiểm tra thành công là đúng
            MissMessageCount = 0; // Thiết lập cờ đếm số tin nhắn gửi sai liên tiếp về 0
            // setDigitalSignal(PIN_SIGNAL_DATA, LOW);  // tắt đèn tín hiệu gửi 1 lúc
            // delay(200);                              // Chờ
            // setDigitalSignal(PIN_SIGNAL_DATA, HIGH); // Bật lại một lúc lâu
            // delay(1000);                             // Chờ
            // setDigitalSignal(PIN_SIGNAL_DATA, LOW);  // và cuối cùng là tắt đi
            if (DEBUG)
                Serial.println("Send message successful!");
        }
        else
        {
            MissMessageCount++; // Nếu cờ kiểm tra thành công là false (sai) thì tiến hành tăng cờ đếm số tin nhắn lỗi liên tiếp lên 1
            if (DEBUG)
                Serial.println("Send message fail!");
        }
        if (MissMessageCount >= MAX_MISSED_MESSAGE_PACKET)
        {                                                 // Nếu số tin nhắn lỗi liên tiếp đếm được lớn hơn hoặc bằng với @MAX_MISSED_MESSAGE_PACKET, thì
            MissMessageCount = MAX_MISSED_MESSAGE_PACKET; // Đặt só tin nhắn lỗi vẫn bằng giới hạn, k cho tăng nữa, tránh bị tràn bộ đệm (Buffer overflow)
            setDigitalSignal(PIN_SIGNAL_ERROR, HIGH);     // Thiết lập đèn hiển thị lỗi sáng lên liên tục
            waitForGprsAvailable();                       // Kiểm tra, đợi GPRS có ổn không trước khi gửi, nếu không thì thực hiện các hàm bật lại ở phía trên
        }
    }
    else if (res.indexOf("+HTTPACTION: 0,601") < 0)
    {                                 // Có nghĩa là server hiện không khả dụng, tức lỗi 604
        BlinkErrorSignalLed(1000, 5); // Nháy đèn lỗi 5 lần trong 10s
    }
    else
    {                                 // Là lỗi 601, tức do thiết bị có mạng yếu hoặc không thiết lập kết nối thành công
        BlinkErrorSignalLed(1000, 3); // Nháy đèn lỗi 3 lần trong 6s
        waitForGprsAvailable();       // Kiểm tra, đợi GPRS có ổn không trước khi gửi, nếu không thì thực hiện các hàm bật lại ở phía trên
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

// Hàm kiểm tra trạng thái của GPS
void checkGpsStatus()
{
    String x = sendAtCommand("AT+CGPSSTATUS?", 1000, DEBUG); // Gửi lệnh kiểm tra trạng thái
    delay(50);                                               // Đợi sương sương 50ms
    if (x.indexOf("Location 3D Fix") < 0 && x.indexOf("Location 2D Fix") < 0)
    {                                          // Nếu vị trí chưa phải Location 2D|3D Fix thì
        isGps3dFixed = false;                  // Đặt cờ trạng thái fixed là false (sai)
        setDigitalSignal(PIN_SIGNAL_GPS, LOW); // tắt đèn hiệu GPS đi
    }
    else
    { // Ngược lại bật đèn hiệu lên và đặt cờ là true
        isGps3dFixed = true;
        setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
    }
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

// hàm sửa loại dữ liệu trả về của GNSS hay GPS
void changeDataTypeGPS()
{
    switch (sendResult.nextActionCode)
    {
    case 100:
        currentLocationDataType = "RMC";
        sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
        blinkGpsSignalLed(50, 10);
        break;
    case 101:
        currentLocationDataType = "GSV";
        sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
        blinkGpsSignalLed(50, 10);
        break;
    case 102:
        currentLocationDataType = "GSA";
        sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
        blinkGpsSignalLed(50, 10);
        break;
    case 103:
        currentLocationDataType = "GGA";
        sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
        blinkGpsSignalLed(50, 10);
        break;
    default:
        break;
    }
    if (DEBUG && sendResult.nextActionCode >= 100 && sendResult.nextActionCode <= 103)
    {
        Serial.println("Change location data type to [" + currentLocationDataType + "]");
    }
}

// hàm reset lại GPS theo cách được server chỉ định
void resetGPSbyServer()
{
    switch (sendResult.nextActionCode)
    {
    case 200:
        currentLocationDataType = "RMC";
        sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
        sendAtCommand("AT+CGPSRST=0", 2000, DEBUG);
        break;
    case 201:
        currentLocationDataType = "RMC";
        sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
        sendAtCommand("AT+CGPSRST=1", 2000, DEBUG);
        break;
    case 202:
        currentLocationDataType = "RMC";
        sendAtCommand("AT+CGNSSEQ=" + currentLocationDataType, 1000, DEBUG); //read GPRMC data
        sendAtCommand("AT+CGPSRST=2", 2000, DEBUG);
        break;
    default:
        break;
    }
}

// Hàm lấy trí trị GPRS
void getGpsData()
{
    changeDataTypeGPS();
    resetGPSbyServer();
    checkGpsStatus();
    if (isGps3dFixed)
        isGettingGpsData = true;
    // Region for get location
    if (!isGetRawGpsLocationData)
    {
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
    }
    else
    {
        gpsData = sendAtCommand("AT+CGPSINF=0", 1000, DEBUG);
        //+CGPSINF: 0,2234.931817,11357.122485,92.461185,20141031041141.000,88,12,0.000000,0.000000
        int prefixIndex = -1, // Cờ chỉ mục tiền tố trong chuỗi GPS, dùng để tìm prefix có dạng +CGNSINF:
            surfixIndex = -1; // Cờ chỉ mục hậu tố trong chuỗi GPS, dùng để tìm surfix có dạng OK
        // Cut prefix
        prefixIndex = gpsData.indexOf("+CGPSINF:");
        surfixIndex = gpsData.lastIndexOf("OK");
        surfixIndex = (surfixIndex >= 0 && surfixIndex < prefixIndex ? surfixIndex + 2 : gpsData.length());
        if (prefixIndex >= 0)
        {
            gpsData = gpsData.substring(prefixIndex, surfixIndex);
            gpsData.replace("\r\n", ""); // delete all new line
            gpsData.replace(" ", "");
            gpsData.replace("+CGPSINF:", "");
            gpsData.replace("OK", "");
        }
        else
        {
            gpsData = "NULL";
        }
    }

    if (isAlwaysGetNetworkLocation || sendResult.nextActionCode == 401)
    {
        networkLocation = sendAtCommand("AT+CIPGSMLOC=1,1", 1000, DEBUG);
        //+CGPSINF: 0,2234.931817,11357.122485,92.461185,20141031041141.000,88,12,0.000000,0.000000
        int prefixIndex = -1, // Cờ chỉ mục tiền tố trong chuỗi GPS, dùng để tìm prefix có dạng +CGNSINF:
            surfixIndex = -1; // Cờ chỉ mục hậu tố trong chuỗi GPS, dùng để tìm surfix có dạng OK
        // Cut prefix
        prefixIndex = networkLocation.indexOf("+CIPGSMLOC:");
        surfixIndex = networkLocation.lastIndexOf("OK");
        surfixIndex = (surfixIndex >= 0 && surfixIndex < prefixIndex ? surfixIndex + 2 : networkLocation.length());
        if (prefixIndex >= 0)
        {
            networkLocation = networkLocation.substring(prefixIndex, surfixIndex);
            networkLocation.replace("\r\n", ""); // delete all new line
            networkLocation.replace(" ", "");
            networkLocation.replace("+CIPGSMLOC:", "");
            networkLocation.replace("OK", "");
        }
        else
        {
            networkLocation = "NULL";
        }
    }
    // end region for get location
    if (isGps3dFixed)
    {
        setDigitalSignal(PIN_SIGNAL_GPS, HIGH);
        isGettingGpsData = false;
    }
    else
        setDigitalSignal(PIN_SIGNAL_GPS, LOW);
}