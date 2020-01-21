// This is a super simple demo program for ESP8266's that can be used by SPRESENSE
// Serial2 @ 115200 baud.
#include <Wire.h>

#include "DHT12.h"
#include "Seeed_BMP280.h"

#include "Ambient_spr.h"

#define SSID "xxxxxxxx"      //your wifi ssid here
#define PASS "xxxxxxxx"   //your wifi key here

#define LED_PIN LED0

#define REPLYBUFFSIZ 0xFFFF
char replybuffer[REPLYBUFFSIZ];
uint8_t getReply(char *send, uint16_t timeout = 500, boolean echo = true);
uint8_t espreadline(uint16_t timeout = 500, boolean multiline = false);
boolean sendCheckReply(char *send, char *reply, uint16_t timeout = 500);

enum {WIFI_ERROR_NONE=0, WIFI_ERROR_AT, WIFI_ERROR_RST, WIFI_ERROR_SSIDPWD, WIFI_ERROR_SERVER, WIFI_ERROR_UNKNOWN};

DHT12 dht12;          //Preset scale CELSIUS and ID 0x5c.
BMP280 bmp280;

Ambient ambient;

unsigned int channelId = xxxx; // AmbientのチャネルID
const char* writeKey = "xxxxxxxx"; // ライトキー


float pressure;
float temp;
// float altitude;
float dht12_temp;
float dht12_humidity;


void setup()
{
  pinMode(LED_PIN, OUTPUT);

  //blink LED0 to indicate power up
  for(int i = 0; i<3; i++)
  {
   digitalWrite(LED_PIN,HIGH);
   delay(50);
   digitalWrite(LED_PIN,LOW);
   delay(100);
  }

  // Serial debug console
  Serial.begin(115200);

  //Open software serial for chatting to ESP
  Serial2.begin(115200);

  Serial.println(F("SPRESENSE ESP8266 Demo"));

//  Serial.begin(115200);
  if(!bmp280.init()){
    Serial.println("Device error!");
  }

  ambient.begin(channelId, writeKey); // チャネルIDとライトキーを指定してAmbientの初期化

  //connect to the wifi
  byte err = setupWiFi();

  if (err) {
    // error, print error code
    Serial.print("setup error:");  Serial.println((int)err);

    debugLoop();
  }

  // success, print IP
  getIP();

  //set TCP server timeout
  //sendCheckReply("AT+CIPSTO=0", "OK");
}

void loop()
{
  pressure = bmp280.getPressure();
  temp = bmp280.getTemperature();
//  altitude = bmp280.calcAltitude(pressure);
  dht12_temp = dht12.readTemperature();
  dht12_humidity = dht12.readHumidity();

  //get and print temperatures
  Serial.print("Temp(BMP280): ");
  Serial.print(temp);
  Serial.println(" C"); // The unit for  Celsius because original arduino don't support speical symbols

  Serial.print("Temp(DHT12) : ");
  Serial.print(dht12_temp);
  Serial.println(" C"); // The unit for  Celsius because original arduino don't support speical symbols

  Serial.print("Pressure: ");
  Serial.print(pressure / 100);
  Serial.println(" hPa");

  //get and print altitude data
 /* Serial.print("Altitude: ");
  Serial.print(altitude);
  Serial.println(" m");
*/

  Serial.print("Humidity: ");
  Serial.print(dht12_humidity);
  Serial.println(" %RH");

  Serial.println("\n");//add a line between output of different times.

// データをAmbientに送信する
  ambient.set(1, String(temp).c_str());
  ambient.set(2, String(dht12_temp).c_str());
  ambient.set(3, String(pressure).c_str());
  ambient.set(4, String(dht12_humidity).c_str());
//  ambient.set(9, ("35.679" + String(random(0, 100)) + "70").c_str()); //緯度(lat)
//  ambient.set(10, ("139.737" + String(random(0, 100)) + "80").c_str()); //経度(lng)

  int ret = ambient.send();

  if (ret == 0) {
    Serial.println("*** ERROR! RESET Wifi! ***\n");
    setupWiFi();
  }else{
    Serial.println("*** Send comleted! ***\n");
    delay(300000);  // 5分間隔
  }

}

boolean getVersion() {
  // Get version?
  getReply("AT+GMR", 100, true);
  while (true) {
    espreadline(50);  // this is the 'echo' from the data
    Serial.print("<--- "); Serial.println(replybuffer);
    if (strstr(replybuffer, "OK"))
    break;
  }
  return true;
}

boolean espReset() {
  getReply("AT+RST", 1000, true);
  if (! strstr(replybuffer, "OK")) return false;
  delay(500);

  // turn off echo
  getReply("ATE0", 250, true);

  return true;
}

boolean ESPconnectAP(char *s, char *p) {

  getReply("AT+CWMODE=1", 500, true);
  if (! (strstr(replybuffer, "OK") || strstr(replybuffer, "no change")) )
    return false;

  String connectStr = "AT+CWJAP=\"";
  connectStr += SSID;
  connectStr += "\",\"";
  connectStr += PASS;
  connectStr += "\"";
  connectStr.toCharArray(replybuffer, REPLYBUFFSIZ);
  getReply(replybuffer, 200, true);

  while (true) {
    espreadline(200);  // this is the 'echo' from the data
    if((String)replybuffer == ""){
      Serial.print(".");
    }else{
      Serial.println("");
      Serial.print("<--- "); Serial.println(replybuffer);
    }
    if (strstr(replybuffer, "OK"))
    break;
  }

  return true;
}


byte setupWiFi() {
  // reset WiFi module
  Serial.println(F("Soft resetting..."));
  if (!espReset())
    return WIFI_ERROR_RST;

  delay(500);

  Serial.println(F("Checking for ESP AT response"));
  if (!sendCheckReply("AT", "OK"))
    return WIFI_ERROR_AT;

  getVersion();

  Serial.print(F("Connecting to ")); Serial.println(SSID);
  if (!ESPconnectAP(SSID, PASS))
    return WIFI_ERROR_SSIDPWD;

  Serial.println(F("Single Client Mode"));

  if (!sendCheckReply("AT+CIPMUX=0", "OK"))
        return WIFI_ERROR_SERVER;

  return WIFI_ERROR_NONE;
}

boolean getIP() {
  getReply("AT+CIFSR", 100, true);
  while (true) {
    espreadline(50);  // this is the 'echo' from the data
    Serial.print("<--- "); Serial.println(replybuffer);
    if (strstr(replybuffer, "OK"))
    break;
  }

  delay(100);

  return true;
}




/************************/
uint8_t espreadline(uint16_t timeout, boolean multiline) {
  uint16_t replyidx = 0;

  while (timeout--) {
    if (replyidx > REPLYBUFFSIZ-1) break;

    while(Serial2.available()) {
      char c =  Serial2.read();
      if (c == '\r') continue;
      if (c == 0xA) {
        if (replyidx == 0)   // the first 0x0A is ignored
          continue;

        if (!multiline) {
          timeout = 0;         // the second 0x0A is the end of the line
          break;
        }
      }
      replybuffer[replyidx] = c;
      // Serial.print(c, HEX); Serial.print("#"); Serial.println(c);
      replyidx++;
    }

    if (timeout == 0) break;
    delay(1);
  }
  replybuffer[replyidx] = 0;  // null term
  return replyidx;
}

uint8_t getReply(char *send, uint16_t timeout, boolean echo) {
  // flush input
  while(Serial2.available()) {
     Serial2.read();
     delay(1);
  }

  if (echo) {
    Serial.print("---> "); Serial.println(send);
  }
  Serial2.println(send);

  // eat first reply sentence (echo)
  uint8_t readlen = espreadline(timeout);

  //Serial.print("echo? "); Serial.print(readlen); Serial.print(" vs "); Serial.println(strlen(send));

  if (strncmp(send, replybuffer, readlen) == 0) {
    // its an echo, read another line!
    readlen = espreadline();
  }

  if (echo) {
    Serial.print ("<--- "); Serial.println(replybuffer);
  }
  return readlen;
}

boolean sendCheckReply(char *send, char *reply, uint16_t timeout) {

  getReply(send, timeout, true);

 /*
  for (uint8_t i=0; i<strlen(replybuffer); i++) {
    Serial.print(replybuffer[i], HEX); Serial.print(" ");
  }
  Serial.println();
  for (uint8_t i=0; i<strlen(reply); i++) {
    Serial.print(reply[i], HEX); Serial.print(" ");
  }
  Serial.println();
*/
  return (strcmp(replybuffer, reply) == 0);
}

void debugLoop() {
  Serial.println("========================");
  //serial loop mode for diag
  while(1) {
    if (Serial.available()) {
      Serial2.write(Serial.read());
//      delay(1);
    }
    if (Serial2.available()) {
      Serial.write(Serial2.read());
//      delay(1);
    }
  }
}
