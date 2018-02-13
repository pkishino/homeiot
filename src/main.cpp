#include <Arduino.h>
#include <NewRemoteTransmitter.h>
#include <Wire.h>
#include <SSD1306.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

SSD1306 display(0x3c, 4, 5);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const char *ssid = "***REMOVED***";
const char *password = "***REMOVED***";
const char *devicePass = "***REMOVED***";

const char *host = "api.thingspeak.com";
const int httpPort = 80;
String writeAPIKey = "***REMOVED***";
WiFiClient client;
String thingspeak = "";

String location = "";

unsigned long nexa = 22070078;
byte livingUnit = 4;
byte bedroomUnit = 3;
NewRemoteTransmitter nexaTransmitter(nexa, 15, 258);

void setupWifi();
void setupOTA();
void setupDisplay();
void measureBattery();
void measureEnvironment();
void rfSend(byte, bool);
void uploadResults();

#define CHECK_INTERVAL 30

void setup()
{
    Serial.begin(115200);
    Serial.println("Serial up and running");
    setupDisplay();
    setupWifi();
    measureBattery();
    measureEnvironment();
    uploadResults();
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.forceSleepBegin();
    delay(100);
    display.displayOff();
    Serial.println("Sleeping...");
    Serial.print("Info:");
    Serial.print(ESP.getSdkVersion()); //Info:2.1.0(deb1901)0000000031
    Serial.print(ESP.getCoreVersion());
    Serial.print(ESP.getBootVersion());
    ESP.deepSleep(CHECK_INTERVAL * 1000000 * 60, WAKE_RF_DEFAULT);
}

void loop()
{
}
void setupDisplay()
{
    display.init();
    display.flipScreenVertically();
    display.setContrast(255);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
}
void setupOTA()
{
    const char *hostname = location.c_str();
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPassword(devicePass);

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        display.clear();
        display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, "OTA Update");
        display.display();
    });
    ArduinoOTA.onEnd([]() {
        display.clear();
        display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Restart");
        display.display();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {

        display.drawProgressBar(4, 32, 120, 8, progress / (total / 100));
        display.display();
    });
    ArduinoOTA.onError([](ota_error_t error) {
        display.clear();
        display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, "OTA Error :");
        if (error == OTA_AUTH_ERROR)
            display.drawString(DISPLAY_WIDTH / 2, 32, "Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            display.drawString(DISPLAY_WIDTH / 2, 32, "Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            display.drawString(DISPLAY_WIDTH / 2, 32, "Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            display.drawString(DISPLAY_WIDTH / 2, 32, "Receive Failed");
        else if (error == OTA_END_ERROR)
            display.drawString(DISPLAY_WIDTH / 2, 32, "End Failed");
        display.display();
        delay(5000);
    });
    ArduinoOTA.begin();
}
void setupWifi()
{
    String mac = WiFi.macAddress();
    Serial.println("MAC:" + mac);
    if (mac == "***REMOVED***")
    {
        location = "Bedroom";
    }
    else if (mac = "***REMOVED***")
    {
        location = "Living Room";
    }
    WiFi.persistent(false);
    WiFi.forceSleepWake();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        display.drawString(DISPLAY_WIDTH / 2, 10, "Connection Failed!");
        display.display();
        delay(3000);
        ESP.restart();
    }

    display.clear();
    display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, "IP:" + WiFi.localIP().toString());
    display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Monitor for:" + location);
    display.display();
    delay(3000);
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
}
void measureBattery()
{
    int raw = analogRead(A0);
    int level = map(raw, 511, 718, 0, 100);
    display.drawString(0, 10, "Raw:" + String(raw));
    display.drawString(0, 20, "Level:" + String(level) + '%');
    display.display();
    Serial.println("Level:" + String(level) + '%');
    Serial.println("raw:" + String(raw));

    if (location == "Living Room")
    {
        thingspeak += "&field5=" + String(level);
    }
    else if (location == "Bedroom")
    {
        thingspeak += "&field6=" + String(level);
    }
}
void measureEnvironment()
{
    dht.begin();
    delay(500);
    float humidity = dht.readHumidity();
    float temp = dht.readTemperature();

    if (isnan(humidity) || isnan(temp))
    {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }
    if (humidity <= 42)
    {
        if (location == "Living Room")
        {
            rfSend(livingUnit, true);
            thingspeak += "&field7=1";
        }
        else if (location == "Bedroom")
        {
            rfSend(bedroomUnit, true);
            thingspeak += "&field8=1";
        }
    }
    else if (humidity >= 47)
    {
        if (location == "Living Room")
        {
            rfSend(livingUnit, false);
            thingspeak += "&field7=0";
        }
        else if (location == "Bedroom")
        {
            rfSend(bedroomUnit, false);
            thingspeak += "&field8=0";
        }
    }
    float index = dht.computeHeatIndex(temp, humidity, false);
    Serial.print("Humidity: " + String(humidity) + "%\t");
    Serial.print("Temperature: " + String(temp) + "˚C\t");
    Serial.print("Heat Index:" + String(temp) + "˚C\n");
    display.drawString(0, 30, "Humidity:" + String(humidity) + '%');
    display.drawString(0, 40, "Temperature:" + String(temp) + "˚C");
    display.drawString(0, 50, "Heat Index:" + String(index) + "˚C");
    display.display();

    if (location == "Living Room")
    {
        thingspeak += "&field1=" + String(humidity);
        thingspeak += "&field3=" + String(temp);
    }
    else if (location == "Bedroom")
    {
        thingspeak += "&field2=" + String(humidity);
        thingspeak += "&field4=" + String(temp);
    }
}

void rfSend(byte unit, bool status)
{
    nexaTransmitter.sendUnit(unit, status);
    Serial.println("Turned living room " + String(status));
    delay(1000);
}

void uploadResults()
{
    if (!client.connect(host, httpPort))
    {
        return;
    }
    String postStr = writeAPIKey;
    postStr += thingspeak;
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + writeAPIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    delay(1000);
    Serial.println("Thingspeak result: " + String(client.read()));
    thingspeak = "";
    Serial.println(postStr);
}