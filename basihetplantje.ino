#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define ANA A0
#define DIGI D5
#define POMPOUT D6
#define XTRALED D7
#define LED D4 // Use built-in LED which connected to D4 pin or GPIO 2

ESP8266WebServer server(80);

bool pumpOn = true; //the pump is running
unsigned long pumpOnTime = 1500;   //how long should the pump be running
unsigned long pumpOffTime = 60000; //how long should the pump be switched off to allow the ground to be saturated
unsigned long checkInterval = 500; //how often should we check.
unsigned long totalPumpTime;

double pumpOnValue = 500;   //when then analogValue is above this value the pump should be switched on
bool dryRun = true; //don't switch on pump, but use internal led

bool configLoaded = false;
bool wifiConnected = false;
bool manualOverride = false; //manual override to switch pump on.

unsigned long lastCheck; 
unsigned long switchPumpOffTime; //when was the pump switched off
unsigned long switchPumpOnTime;  //when was the pump switched on

enum ConfigType
{
  PlantConfig,
  WiFiConfig
};

String configFileName(ConfigType type)
{
  switch(type)
  {
    case PlantConfig:
      return "/config.json";
    case WiFiConfig:
      return "/wifi.json";
  }
}

bool loadConfigFile(ConfigType type)
{
  File configFile = LittleFS.open(configFileName(type), "r");
  Serial.println("opening configuration " + configFileName(type));
  if (!configFile) 
  {
    Serial.println("Failed to open config file: " + configFileName(type));
    return false;
  }
  String json = configFile.readString();
  configFile.close();
  switch(type)
  {
    case PlantConfig:
      loadPlantConfig(json);
      break;
    case WiFiConfig:
      loadWiFiConfig(json);
      break;
  }
}

unsigned long toMillis(double value)
{
  return value * 1000;
}

bool loadPlantConfig(String json)
{
  DynamicJsonDocument doc(1024);
  auto error = deserializeJson(doc, json);
  if (error)
  {
    Serial.println("Failed to parse config file");
    return false;
  }
  pumpOnTime = toMillis(doc["pumpOnTime"]);
  pumpOffTime = toMillis(doc["pumpOffTime"]);
  pumpOnValue = doc["pumpOnValue"];
  checkInterval = toMillis(doc["checkInterval"]);
  dryRun = doc["dryRun"];
  
  Serial.print("Pump on time: ");
  Serial.println(pumpOnTime);
  Serial.print("Pump off time: ");
  Serial.println(pumpOffTime);
  Serial.print("Pump on value: ");
  Serial.println(pumpOnValue);
  Serial.print("Check interval: ");
  Serial.println(checkInterval);
  Serial.print("Dry run: ");
  Serial.println(dryRun);
  return true;
}

bool loadWiFiConfig(String json)
{
  DynamicJsonDocument doc(1024);
  auto error = deserializeJson(doc, json);
  if (error)
  {
    Serial.println("Failed to parse config file");
    return false;
  }
  String serverName = doc["serverName"].as<String>();;
  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();;
  
  Serial.println("serverName: " + serverName);
  Serial.println("ssid: " + ssid);
  Serial.print("password: ");
  int i = 0;
  while (password[i] != 0)
  {
    Serial.print("*");
    i++;
  }
  Serial.println("");
  connectWifi(ssid, password, serverName);
  if(wifiConnected)
  {
    setupServer(serverName);
  }
  return true;
}

bool saveConfig(String config, ConfigType configType)
{
  
  File configFile = LittleFS.open(configFileName(configType), "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing: " + configFileName(configType));
    return false;
  }
  configFile.write(config.c_str());
  configFile.close();
  return true;
}

void getConfig()
{
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    server.send(500, "configuration not found");
  }
  String json = configFile.readString();
  configFile.close();
  server.send(200, "application/json", json);
}

void postConfig()
{
  if (server.hasArg("plain")== false)
  { //Check if body received
    server.send(400, "text/plain", "Body not received");
    return;
  }

  String json = server.arg("plain");
  Serial.println("received config: " + json);  
  if(loadPlantConfig(json))
  {
    saveConfig(json, PlantConfig);
    getConfig(); //to show that we really save what you asked for
  }
  else
  {
    server.send(422, "text/plain", "configuration is invalid");
  }
}


void setupServer(String serverName)
{
  server.on("/", handleRoot);
  server.on("/manual/on", manualOn);
  server.on("/manual/off", manualOff);
  
  server.on("/configuration", HTTP_POST, postConfig);
  server.on("/configuration", HTTP_GET, getConfig);
  server.onNotFound(handleNotFound);
  if(MDNS.begin(serverName)) 
  {
    Serial.println("MDNS responder started");
  }
  server.begin();  
}

void handleNotFound() 
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) 
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setupPorts()
{
  pinMode(ANA, INPUT);
  pinMode(DIGI, INPUT);
  pinMode(POMPOUT, OUTPUT);
  pinMode(LED, OUTPUT);     // Initialize the LED as an output
  pinMode(XTRALED, OUTPUT);
}

void setup() 
{
  setupPorts();
  // put your setup code here, to run once:
  Serial.begin(115200);
  if (!LittleFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }
  configLoaded = loadConfigFile(WiFiConfig);
  if(!configLoaded)
  {
      Serial.println("waiting for retrieving WiFi config from serial");
  }
  else if(!loadConfigFile(PlantConfig))
  {
      Serial.println("waiting for retrieving config from server");
      configLoaded = false;
  }
  switchPump(false);
}


void connectWifi(String ssid, String password, String serverName)
{
  wifiConnected = false;
  WiFi.disconnect();
  if(ssid != "")
  {
    WiFi.hostname(serverName);
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.begin(ssid, password);

    Serial.print("Connecting to ");
    Serial.print(ssid);
    int i = 0; 
    wifiConnected = WiFi.status() == WL_CONNECTED;
    bool ledOn = false;
    while(!wifiConnected && i < 60)
    {
      Serial.print(".");
      delay(500);
      i++;
      wifiConnected = WiFi.status() == WL_CONNECTED;
      digitalWrite(LED, ledOn);
      ledOn = !ledOn;
    }
    digitalWrite(LED, LOW);
    
    Serial.println("");
    if(wifiConnected)
    {
      Serial.print("connected to: ");
      Serial.print(WiFi.SSID());
      Serial.print(" local ip: ");
      Serial.println(WiFi.localIP());
    }
    else
    {
      Serial.print("failed to connect to WiFi: ");
      Serial.println(ssid);
    }
  }
}

void blinkInternalLed(int interval)
{
  static bool blinkLedOn;
  static unsigned long blinkInternalTime;

  if(timePassed(&blinkInternalTime, interval))
  {
    blinkLedOn = !blinkLedOn;
    digitalWrite(LED, blinkLedOn); 
  }
}

void switchPump(bool switchOn)
{
  unsigned long now = millis();
  if(switchOn == pumpOn)
  {
    return;
  }

  static unsigned long pumpOnTimer;
  if(switchOn)
  {
    if(now < switchPumpOffTime + pumpOffTime && !manualOverride)
    {
      return; //we wait to switch on until the ground is saturated
    }
    pumpOnTimer = switchPumpOnTime = now;
  }
  else
  {
    if(now > pumpOnTimer && pumpOnTimer != 0) //every 50 days a reset should not get crazy stats
    {
      totalPumpTime += now - pumpOnTimer;
    }
    switchPumpOffTime = now;
  }

  pumpOn = switchOn;
  digitalWrite(XTRALED, pumpOn);
  if(!dryRun)
  {
    Serial.print(now);
    if(pumpOn)
    {
      Serial.println("switching pump on");
    }
    else
    {
      Serial.println("switching pump off");
    }
    digitalWrite(POMPOUT, pumpOn);
  }
}

void checkWater()
{
  if(!configLoaded)
  {
     blinkInternalLed(200);
     return;
  }

  
  if(timePassed(&lastCheck, checkInterval))
  {
    double value = analogRead(ANA);
    if(value > pumpOnValue)
    {
      blinkInternalLed(1000); //alert
      switchPump(true);
    }
    else 
    {
      digitalWrite(LED, LOW); //all systems green
      if(pumpOn && !manualOverride)
      {
        switchPump(false);
      }
    }
  }
  
  if(pumpOn)
  {
    if(timePassed(&switchPumpOnTime, pumpOnTime))
    {
      switchPump(false);
      manualOverride = false; 
    }
  }
}

bool timePassed(unsigned long *lastCheck, unsigned long timeSpan)
{
  unsigned long now = millis();
  bool passed = *lastCheck + timeSpan <= now || now < *lastCheck;
  if(passed)
  {
    *lastCheck = now;
  }
  return passed;
}

void handleRoot() 
{
  Serial.print("handling root");
  StaticJsonDocument<200> doc;
  double value = analogRead(ANA);
  doc["saturation"] = analogRead(ANA);
  doc["saturated"] = !digitalRead(DIGI);
  doc["pump"] = pumpOn;
  doc["totalPumpTime"] = totalPumpTime;
  doc["dryRun"] = dryRun;
  String json; 
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void manualOn()
{
  manualOverride = true;
  switchPump(true);
  handleRoot();
}

void manualOff()
{
  manualOverride = false;
  switchPump(false);
  handleRoot();
}

unsigned long lastPing;
void pingOfLive()
{
  if(timePassed(&lastPing, 30000))
  {
    Serial.print("Wifi status ");
    Serial.print(WiFi.status());
    Serial.print(" ");
    Serial.print(WiFi.localIP());
    Serial.print(" saturation ");
    Serial.println(analogRead(ANA));
  }
}

void checkWiFiConfig()
{
  if(Serial.available()>0)
  {
    String json = Serial.readString();
    int length = json.length();
    if(length > 0)
    {
      Serial.print("receiving configuration: ");
      if(loadWiFiConfig(json))
      {
        configLoaded = true;
        saveConfig(json, WiFiConfig);
      }
    }
  }
}

void loop() 
{
  pingOfLive();
  checkWater();
  server.handleClient();
  MDNS.update();
  checkWiFiConfig();
}
