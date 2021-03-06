/*
  vMix Tally Lights

  http://vicksmediatech.com/2020/03/18/wifi-tally-lights-for-vmix-software/

  https://www.youtube.com/watch?v=cOqnw6gP_XU
  

*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include "FS.h"

#include <FastLED.h>
#define NUM_LEDS 8
CRGB leds[NUM_LEDS];
/////////////////////////////////////////

bool debug = false;

/////////////////////////////////////////

//------------------------------------------------------------Change to your desire settings-----------------------------------------------
//Everything you edit here is for Setting Pin for led control

#define LEDstatusPin 15 //(Pin D8) Status LED

#define LEDactivePin 12 //(Pin D6) Active Tally

#define LEDpreviewPin 14 //(Pin D5) Preview Tally LED

#define SettingsResetPin 16 //(Pin D0) Connect this Pin to GND while restarting to reset the Wifi/vMix Settings and enter the config mode

#define LEDsDataPin 13 //(Pin D7)

//----------------Web Menu Setting-----------------------------------
//Everything you edit here is for web menu style, name, color

const char *tallyName = "Tally Light";

const char *backColor = "grey"; //Background Tally Menu

const char *tallyCompany = "vMix";

const char *tallyDescriptions = "NW";

//--------------------------Tally AP Settings--------------------------
//Everything you edit here is for your Tally Setting IP Address, SSID & Password

IPAddress tallyIp(192, 168, 1, 1); // change to your desire IP Address for web menu settings,

const char *ssidTally = "Tally_Lights_Config_%d"; //This is Tally light config-AP SSID !!! don't change/delete the "_%d".

const char *passwordTally = "tallylight"; //This is Tally light config-AP PASSWORD

/////////////////////////////////////////////////////////////////////////

ESP8266WebServer httpServer(80);
char deviceName[32];
int status = WL_IDLE_STATUS;
bool apEnabled = false;
char apPass[64];

int port = 8099; //port vmix tally

char currentState = -1;
const char statusOffTally = 0;
const char ActiveTally = 1;
const char previewTally = 2;

unsigned long previousMillis = 0;
unsigned long longinterval = 800;
unsigned long shortinterval = 250;
int StatusLEDMode = 0;        /////////////////// 0: connecting , 1: connected , 2: config needed
bool StatusLEDStatus = false; // on / off
bool LastStatusLEDStatus = false;

const int sizeOfSsid = 32;
const int sizeOfPass = 32;
const int sizeOfHostName = 24;
const int TallyNumberMaxValue = 12;

// Settings object

struct Settings
{
  char ssid[sizeOfSsid];
  char pass[sizeOfPass];
  char hostName[sizeOfHostName];
  int tallyNumber;
};
Settings settings;
Settings defaultSettings = {
    "SSID",        //SSID
    "Pass",        //Password
    "192.168.1.1", //IP Adrress
    1              //Tally Number
};

// The WiFi client
WiFiClient client;
int timeout = 10;
int delayTime = 5000;

// Time measure
unsigned long lastCheck = 0;

////////////////////////////////////////////////////////////////////////////////////////////

void handleStatusLED(int status);
void loadSettings();
void saveSettings();
void printSettings();
void ledSetOff();
void ledTallyActive();
void ledSetPreview();
void ledSetConnecting();
void ledSetSettings();
void ledTallyOff();
void SetledTallyActive();
void SetledTallyPreview();
void tallyConnectAtur();
void handleData(String data);
void apStart();
void rootPageHandler();
void handleSave();
void connectToWifi();
//void battStatus();
void vMixConnect();
void restart();
void start();

//////////////////////////////////////////////////////////////////////////////////////////

// Load settings from EEPROM
void loadSettings()
{
  Serial.println("------------");
  Serial.println("Loading settings");

  long ptr = 0;

  for (int i = 0; i < sizeOfSsid; i++)
  {
    settings.ssid[i] = EEPROM.read(ptr);
    ptr++;
  }

  for (int i = 0; i < sizeOfPass; i++)
  {
    settings.pass[i] = EEPROM.read(ptr);
    ptr++;
  }

  for (int i = 0; i < sizeOfHostName; i++)
  {
    settings.hostName[i] = EEPROM.read(ptr);
    ptr++;
  }

  settings.tallyNumber = EEPROM.read(ptr);

  if (strlen(settings.ssid) == 0 || strlen(settings.pass) == 0 || strlen(settings.hostName) == 0 || settings.tallyNumber == 0)
  {
    Serial.println("No settings found");
    Serial.println("Loading default settings");
    settings = defaultSettings;
    saveSettings();
    restart();
  }
  else
  {
    Serial.println("Settings loaded");
    printSettings();
    Serial.println("------------");
  }
}

// Save settings to EEPROM
void saveSettings()
{
  Serial.println("------------");
  Serial.println("Saving settings");

  long ptr = 0;

  for (int i = 0; i < 512; i++)
  {
    EEPROM.write(i, 0);
  }

  for (int i = 0; i < sizeOfSsid; i++)
  {
    EEPROM.write(ptr, settings.ssid[i]);
    ptr++;
  }

  for (int i = 0; i < sizeOfPass; i++)
  {
    EEPROM.write(ptr, settings.pass[i]);
    ptr++;
  }

  for (int i = 0; i < sizeOfHostName; i++)
  {
    EEPROM.write(ptr, settings.hostName[i]);
    ptr++;
  }

  EEPROM.write(ptr, settings.tallyNumber);

  EEPROM.commit();

  Serial.println("Settings saved");
  printSettings();
  Serial.println("------------");
}

// Print settings
void printSettings()
{
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(settings.ssid);
  //Serial.print("SSID Password: ");
  //Serial.println(settings.pass);
  Serial.print("IP vMix: ");
  Serial.println(settings.hostName);
  Serial.print("Tally WiFi Num: ");
  Serial.println(settings.tallyNumber);
}

void handleStatusLED(int status)
{
  //Serial.println("Stat");

  LastStatusLEDStatus = StatusLEDStatus;

  if (status == 0) // connecting
  {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= longinterval)
    {
      previousMillis = currentMillis;
      StatusLEDStatus = !StatusLEDStatus;
    }
  }
  else if (status == 1) // connected
  {
    StatusLEDStatus = true;
  }
  else if (status == 2) // config needed
  {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= shortinterval)
    {
      previousMillis = currentMillis;
      StatusLEDStatus = !StatusLEDStatus;
    }
  }

  switch (StatusLEDStatus)
  {
  case 0:
    leds[0] = CRGB(0, 0, 0);
    break;

  case 1:
    leds[0] = CRGB(0, 0, 70);
    break;
  }

  if (LastStatusLEDStatus == !StatusLEDStatus)
  {
    FastLED.show();
  }
}

// Set LED's off
void ledSetOff()
{
  digitalWrite(LEDstatusPin, LOW);
  digitalWrite(LEDactivePin, LOW);
  digitalWrite(LEDpreviewPin, LOW);
  digitalWrite(LEDstatusPin, HIGH);
  for (int i = 2; i <= 7; i++)
  {
    leds[i] = CRGB::Black;
  }

  leds[1] = CRGB::Black;
  //leds[0] = CRGB(0, 0, 70);
  FastLED.show();
}

// Set active
void ledTallyActive()
{
  digitalWrite(LEDstatusPin, LOW);
  digitalWrite(LEDactivePin, LOW);
  digitalWrite(LEDpreviewPin, LOW);
  digitalWrite(LEDactivePin, HIGH);
  digitalWrite(LEDstatusPin, HIGH);

  for (int i = 2; i <= 7; i++)
  {
    leds[i] = CRGB(0, 255, 0); // = Red
  }
  leds[1] = CRGB::Red;
  //leds[0] = CRGB(0, 0, 70);
  FastLED.show();
}

// Set Preview
void ledSetPreview()
{
  digitalWrite(LEDstatusPin, LOW);
  digitalWrite(LEDactivePin, LOW);
  digitalWrite(LEDpreviewPin, LOW);
  digitalWrite(LEDpreviewPin, HIGH);
  digitalWrite(LEDstatusPin, HIGH);

  for (int i = 2; i <= 7; i++)
  {
    leds[i] = CRGB::Black;
  }

  leds[1] = CRGB::Green;
  //leds[0] = CRGB(0, 0, 70);
  FastLED.show();
}

// Draw C(onnecting) with LED's
void ledSetConnecting()
{
  digitalWrite(LEDstatusPin, LOW);
  digitalWrite(LEDpreviewPin, LOW);
  digitalWrite(LEDactivePin, LOW);
  digitalWrite(LEDstatusPin, HIGH);

  for (int i = 2; i <= 7; i++)
  {
    leds[i] = CRGB::Black;
  }

  leds[1] = CRGB::Black;
  //leds[0] = CRGB(0, 0, 70);
  FastLED.show();
}

// Draw S(ettings) with LED's
void ledSetSettings()
{
  digitalWrite(LEDstatusPin, HIGH);
  digitalWrite(LEDpreviewPin, LOW);
  digitalWrite(LEDactivePin, LOW);

  for (int i = 2; i <= 7; i++)
  {
    leds[i] = CRGB::Black;
  }

  leds[1] = CRGB::Black;
  //leds[0] = CRGB(0, 0, 70);
  FastLED.show();
}

// Set tally to off
void ledTallyOff()
{
  Serial.println("Tally off");

  ledSetOff();
}

// Set tally to program
void SetledTallyActive()
{
  Serial.println("Tally active");

  ledSetOff();
  ledTallyActive();
}

// Set tally to preview
void SetledTallyPreview()
{
  Serial.println("Tally preview");

  ledSetOff();
  ledSetPreview();
}

// Set tally to connecting
void tallyConnectAtur()
{
  ledSetOff();
  ledSetConnecting();
}

// Handle incoming data
void handleData(String data)
{
  // Check if server data is tally data
  if (data.indexOf("TALLY") == 0)
  {
    char newState = data.charAt(settings.tallyNumber + 8);

    // Check if tally state has changed
    if (currentState != newState)
    {
      currentState = newState;

      switch (currentState)
      {
      case '0':
        ledTallyOff();
        break;
      case '1':
        SetledTallyActive();
        break;
      case '2':
        SetledTallyPreview();
        break;
      default:
        ledTallyOff();
      }
    }
  }
  else
  {
    Serial.print("Response from vMix: ");
    Serial.println(data);
  }
}

// Start access point
void apStart()
{
  ledSetSettings();
  Serial.println("AP Start");
  Serial.print("AP SSID: ");
  Serial.println(deviceName);
  Serial.print("AP password: ");
  Serial.println(apPass);

  IPAddress Ip(tallyIp);
  IPAddress NMask(255, 255, 255, 0);

  WiFi.softAPConfig(Ip, Ip, NMask);
  WiFi.mode(WIFI_AP);
  WiFi.hostname(deviceName);
  WiFi.softAP(deviceName, apPass);
  delay(100);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("IP address: ");
  Serial.println(myIP);

  apEnabled = true;
}

// Hanle http server root request
void rootPageHandler()
{

  String web_menu_desc = "<!DOCTYPE html>";
  web_menu_desc += "<center>";
  web_menu_desc += "<html lang='en'>";
  web_menu_desc += "<head>";
  web_menu_desc += "<title>Tally Light " + String(settings.tallyNumber) + "</title>";
  web_menu_desc += "<meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'>";
  web_menu_desc += "<meta charset='utf-8'>";
  web_menu_desc += "<link rel='icon' type='image/x-icon' href='favicon.ico'>";
  web_menu_desc += "<link rel='stylesheet' href='styles.css'>";
  web_menu_desc += "<style>body {width: 100%;height: 100%;padding: 25px; color: white;} .title1 {display: flex; flex-direction: column; background: white; color: #009eff;} .wtitle1 {display: flex; flex-direction: column; background: white; color: black;} .title2 {display: flex; flex-direction: column; background: black; color: yellow;} .title3 {color: white;} .title4 {color: black;} .title5 {background: white; color: blue;}</style>";
  web_menu_desc += "</head>";

  // web_menu_desc += "<h1 class='title1'>" + String(tallyCompany) + "</h1>";
  // web_menu_desc += "<h9 class='wtitle1'>" + String(tallyName) + "</h9>";

  web_menu_desc += "<body class='bg-light'>";
  web_menu_desc += "<style>body {background-color: " + String(backColor) + "}</style>";

  web_menu_desc += "<h1 class='title2'>Tally Light " + String(settings.tallyNumber) + "</h1>";
  web_menu_desc += "<div data-role='content' class='row'>";

  web_menu_desc += "<div class='col-md-6'>";
  web_menu_desc += "<h2 class='title3'>SETTINGS</h2>";
  web_menu_desc += "<form action='/save' method='post' enctype='multipart/form-data' data-ajax='false'>";

  web_menu_desc += "<div class='form-group row'>";
  web_menu_desc += "<label for='ssid' class='col-sm-4 col-form-label'>SSID/Network Name</label>";
  web_menu_desc += "<div class='col-sm-8'>";
  web_menu_desc += "<input id='ssid' class='form-control' type='text' size='20' maxlength='20' name='ssid' value='" + String(settings.ssid) + "'>";
  web_menu_desc += "</div></div>";

  web_menu_desc += "<div class='form-group row'>";
  web_menu_desc += "<label for='ssidpass' class='col-sm-4 col-form-label'>Password</label>";
  web_menu_desc += "<div class='col-sm-8'>";
  web_menu_desc += "<input id='ssidpass' class='form-control' type='text' size='20' maxlength='20' name='ssidpass' value='" + String(settings.pass) + "'>";
  web_menu_desc += "</div></div>";

  web_menu_desc += "<div class='form-group row'>";
  web_menu_desc += "<label for='hostname' class='col-sm-4 col-form-label'>vMix IP (without Port)</label>";
  web_menu_desc += "<div class='col-sm-8'>";
  web_menu_desc += "<input id='hostname' class='form-control' type='text' size='20' maxlength='20' name='hostname' value='" + String(settings.hostName) + "'>";
  web_menu_desc += "</div></div>";

  web_menu_desc += "<div class='form-group row'>";
  web_menu_desc += "<label for='inputnumber' class='col-sm-4 col-form-label'>Tally number (1-200)</label>";
  web_menu_desc += "<div class='col-sm-8'>";
  web_menu_desc += "<input id='inputnumber' class='form-control' type='number' size='64' min='0' max='1000' name='inputnumber' value='" + String(settings.tallyNumber) + "'>";
  web_menu_desc += "</div></div>";

  web_menu_desc += "<input type='submit' value='SAVE' class='btn btn-primary'></form>";
  web_menu_desc += "</div>";
  //int batt = analogRead(A0);
  //int level = map(batt, 195, 248, 0, 100);
  web_menu_desc += "<div class='col-md-8'>";
  web_menu_desc += "<h2 class='title3'>Info</h2>";
  web_menu_desc += "<table class='table'><tbody>";
  //web_menu_desc += "<h4 class='title3'>Battery Status</h4>";
  //web_menu_desc += "<h3 class='title2'>" + String(level) +"%</h3>";
  char ip[13];
  sprintf(ip, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  web_menu_desc += "<tr><th>IP</th><h4 class='title4'><td><td>   " + String(ip) + "</td></h4></tr>";

  web_menu_desc += "<tr><th>MAC</th><h4 class='title4'><td><td>   " + String(WiFi.macAddress()) + "</td></h4></tr>";
  web_menu_desc += "<tr><th>Signal</th><h4 class='title4'><td><td>   " + String(WiFi.RSSI()) + " dBm</td></h4></tr>";
  web_menu_desc += "<tr><th>Tally Num</th><h4 class='title4'><td><td>   " + String(settings.tallyNumber) + "</td></h4></tr>";

  if (WiFi.status() == WL_CONNECTED)
  {
    web_menu_desc += "<tr><th>Status</th><h4 class='title4'><td><td>   connected</td></h4></tr>";
  }
  else
  {
    web_menu_desc += "<tr><th>Status</th><h4 class='title4'><td><td>   Not connected</td></h4></tr>";
  }

  web_menu_desc += "</tbody></table>";
  web_menu_desc += "</div>";
  web_menu_desc += "</div>";
  web_menu_desc += "<h4 class='title5'> " + String(tallyDescriptions) + "</h4>";
  web_menu_desc += "</body>";
  web_menu_desc += "</html>";
  web_menu_desc += "</center>";

  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/html", String(web_menu_desc));
}

// Settings POST handler
void handleSave()
{
  bool doRestart = false;

  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send(302, "text/plain", "Redirected to: /");

  if (httpServer.hasArg("ssid"))
  {
    if (httpServer.arg("ssid").length() <= sizeOfSsid)
    {
      httpServer.arg("ssid").toCharArray(settings.ssid, sizeOfSsid);
      doRestart = true;
    }
  }

  if (httpServer.hasArg("ssidpass"))
  {
    if (httpServer.arg("ssidpass").length() <= sizeOfPass)
    {
      httpServer.arg("ssidpass").toCharArray(settings.pass, sizeOfPass);
      doRestart = true;
    }
  }

  if (httpServer.hasArg("hostname"))
  {
    if (httpServer.arg("hostname").length() <= sizeOfHostName)
    {
      httpServer.arg("hostname").toCharArray(settings.hostName, sizeOfHostName);
      doRestart = true;
    }
  }

  if (httpServer.hasArg("inputnumber"))
  {
    if (httpServer.arg("inputnumber").toInt() > 0 and httpServer.arg("inputnumber").toInt() <= TallyNumberMaxValue)
    {
      settings.tallyNumber = httpServer.arg("inputnumber").toInt();
      doRestart = true;
    }
  }

  if (doRestart == true)
  {
    restart();
  }
}

// Connect to WiFi
void connectToWifi()
{
  Serial.println();
  Serial.println("------------");
  Serial.println("Connecting to WiFi");
  Serial.print("SSID: ");
  Serial.println(settings.ssid);
  //Serial.print("Passphrase: ");
  //Serial.println(settings.pass);

  int timeout = 43;

  //ledSetConnecting();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  //WiFi.mode(WIFI_AP_STA);
  WiFi.hostname(deviceName);
  WiFi.begin(settings.ssid, settings.pass);

  //-------Disable this if you on Station mode only
  IPAddress Ip(tallyIp);
  IPAddress NMask(255, 255, 255, 0);

  WiFi.softAPConfig(Ip, Ip, NMask);
  WiFi.softAP(deviceName, apPass);
  //-------Disable until here if you on Station mode only

  Serial.print("Waiting for connection.");
  while (WiFi.status() != WL_CONNECTED and timeout > 0)
  {
    delay(700);
    timeout--;
    Serial.print(".");
    StatusLEDMode = 0;
    handleStatusLED(StatusLEDMode);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Success!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Device name: ");
    Serial.println(deviceName);
    Serial.println("------------");
    StatusLEDMode = 1;
  }
  else
  {
    if (WiFi.status() == WL_IDLE_STATUS)
      Serial.println("Idle");
    else if (WiFi.status() == WL_NO_SSID_AVAIL)
      Serial.println("No SSID Available");
    else if (WiFi.status() == WL_SCAN_COMPLETED)
      Serial.println("Scan Completed");
    else if (WiFi.status() == WL_CONNECT_FAILED)
      Serial.println("Connection Failed");
    else if (WiFi.status() == WL_CONNECTION_LOST)
      Serial.println("Connection Lost");
    else if (WiFi.status() == WL_DISCONNECTED)
      Serial.println("Disconnected");
    else
      Serial.println("Unknown Failure");

    Serial.println("------------");
    StatusLEDMode = 2;
    handleStatusLED(StatusLEDMode);
    apStart();
  }
}

// void battStatus()
// {
//   int batt = analogRead(A0);
//   int level = map(batt, 195, 248, 0, 100);

//   if (level < 80)
//   {
//     digitalWrite(16, LOW);
//   }
//   else if (level < 60)
//   {
//     digitalWrite(5, LOW);
//   }
//   else if (level < 40)
//   {
//     digitalWrite(4, LOW);
//   }
//   else if (level < 20)
//   {
//     digitalWrite(14, LOW);
//   }
//   else
//   {
//     digitalWrite(16, HIGH);
//     digitalWrite(5, HIGH);
//     digitalWrite(4, HIGH);
//     digitalWrite(14, HIGH);
//   }
// }

// Connect to vMix instance
void vMixConnect()
{
  Serial.print("Connecting to vMix on ");
  Serial.print(settings.hostName);
  Serial.print("...");
  StatusLEDMode = 0;
  handleStatusLED(StatusLEDMode);
  leds[0] = CRGB(0, 0, 0);
  FastLED.show();

  if (client.connect(settings.hostName, port))
  {
    Serial.println(" Connected!");
    Serial.println("------------");
    StatusLEDMode = 1;
    leds[0] = CRGB(0, 0, 70);
    FastLED.show();
    ledTallyOff();

    // Subscribe to the tally events
    client.println("SUBSCRIBE TALLY");
  }
  else
  {
    Serial.println(" Not found!");
    StatusLEDMode = 2;
  }
}

void restart()
{
  saveSettings();

  Serial.println();
  Serial.println();
  Serial.println("------------");
  Serial.println("------------");
  Serial.println("RESTART");
  Serial.println("------------");
  Serial.println("------------");
  Serial.println();
  Serial.println();

  start();
}

void start()
{
  StatusLEDMode = 0;
  tallyConnectAtur();

  loadSettings();
  sprintf(deviceName, ssidTally, settings.tallyNumber);
  //sprintf(apPass, "%s%s", deviceName, "_access");
  sprintf(apPass, passwordTally, deviceName, "pro");

  connectToWifi();

  if (WiFi.status() == WL_CONNECTED)
  {
    vMixConnect();
  }
}

void setup()
{

  if (debug)
  {
    Serial.begin(115200);
  }

  EEPROM.begin(512);
  //SPIFFS.begin();

  ////////////////////////////////////////////////////

  pinMode(SettingsResetPin, INPUT_PULLUP);

  if (digitalRead(SettingsResetPin) == LOW)
  {
    apStart();
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  pinMode(LEDstatusPin, OUTPUT);
  pinMode(LEDactivePin, OUTPUT);
  pinMode(LEDpreviewPin, OUTPUT);

  digitalWrite(LEDstatusPin, LOW);
  digitalWrite(LEDactivePin, LOW);
  digitalWrite(LEDpreviewPin, LOW);

  ////////////////////////////////////////////////////

  FastLED.addLeds<WS2812B, LEDsDataPin, RGB>(leds, NUM_LEDS);

  for (int i = 0; i <= 7; i++)
  {
    leds[i] = CRGB::Black;
  }
  FastLED.show();

  httpServer.on("/", HTTP_GET, rootPageHandler);
  httpServer.on("/save", HTTP_POST, handleSave);
  //httpServer.serveStatic("/", SPIFFS, "/", "max-age=315360000");
  httpServer.begin();

  if (!apEnabled)
  {
    start();
  }
}

void loop()
{
  // Serial.println(StatusLEDMode);
  // Serial.println(millis());
  // delay(10);
  handleStatusLED(StatusLEDMode);

  httpServer.handleClient();

  while (client.available())
  {
    String data = client.readStringUntil('\r\n');
    handleData(data);
  }

  if (!client.connected() && !apEnabled && millis() > lastCheck + 5000)
  {
    tallyConnectAtur();
    handleStatusLED(StatusLEDMode);
    client.stop();
    vMixConnect();
    handleStatusLED(StatusLEDMode);
    lastCheck = millis();
  }
}
