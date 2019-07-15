#include <SoftwareSerial.h>
SoftwareSerial myGsm(3, 2);

String sendData(String command, const int timeout, boolean debug)
{
  while (myGsm.available())
    myGsm.read();
  String response = "";
  myGsm.println(command);
  long int time = millis();
  while ((time + timeout) > millis())
  {
    while (myGsm.available())
    {
      char c = myGsm.read();

      response += c;
    }
  }
  if (debug)
  {
    Serial.print(response);
  }
  return response;
}

void getSetup() {
  sendData("AT+CGATT=1", 1000, true);
  sendData("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 1000, true); //setting the SAPBR,connection type is GPRS
  sendData("AT+SAPBR=3,1,\"APN\",\"\"", 2000, true); //setting the APN,2nd parameter empty works for all networks
  sendData("AT+SAPBR=1,1", 1000, true);
  sendData("AT+HTTPINIT", 1200, true); //init the HTTP request
  sendData("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/apps/thinghttp/send_request?api_key=SLJ10VVO4K9FVGJX\"", 2000, true);// setting the httppara,
  sendData("AT+HTTPACTION=0", 4000, true); //submit the GET request
  String x = sendData("AT+HTTPREAD", 1000, true); // read the data from the website you access
  x = x.substring(x.indexOf("+HTTPREAD: ") + 11, x.indexOf("OK") + 2);
  Serial.println(x);
  sendData("AT+HTTPTERM", 500, true); // terminate HTTP service
}

void setup()
{
  myGsm.begin(9600);
  Serial.begin(9600);
  delay(500);
  String x = sendData("AT+GSN", 1000, true);
  x.replace("\r\n","");
  x = x.substring(x.indexOf("AT+GSN") + 6, x.indexOf("OK"));
  Serial.println("[" + x + "]");
  getSetup();
}

void loop()
{
}

void printSerialData()
{
  while (myGsm.available() != 0)
    Serial.write(myGsm.read());
}
