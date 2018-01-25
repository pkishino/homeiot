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
#include </Users/Patrick/.platformio/lib/Ticker_ID1586/Ticker.h>
#include <ThingSpeak.h>

SSD1306 display(0x3c, 4, 5);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const char *ssid = "***REMOVED***";
const char *password = "***REMOVED***";
// const char *ssid = "***REMOVED***";
// const char *password = "***REMOVED***";

const char *writeAPIKey = "***REMOVED***";
unsigned long channelId = 404454;
WiFiClient client;

String location = "";

unsigned long livingAddress = 52941634;
unsigned long bedroomAddress = 22070078;
NewRemoteTransmitter living(livingAddress, 14, 257);
NewRemoteTransmitter bedroom(bedroomAddress, 3, 258);

void measureBattery();
void measureEnvironment();
void rfSend();

Ticker enviroTicker(measureEnvironment, 300000);
Ticker batteryTicker(measureBattery, 600000);
Ticker rfTicker(rfSend, 10000);

void setup()
{
    Serial.begin(115200);
    Serial.println("Serial up and running");

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

    display.init();
    display.flipScreenVertically();
    display.setContrast(255);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        display.drawString(DISPLAY_WIDTH / 2, 10, "Connection Failed! Rebooting...");
        display.display();
        delay(5000);
        ESP.restart();
    }
    const char *hostname = location.c_str();
    ArduinoOTA.setHostname(hostname);

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
    dht.begin();
    ThingSpeak.begin(client);

    display.clear();
    display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, "IP:" + WiFi.localIP().toString());
    display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Monitor for:" + location);
    display.display();
    delay(3000);
    display.clear();
    enviroTicker.start();
    batteryTicker.start();
    rfTicker.start();
    measureBattery();
    measureEnvironment();
}

void loop()
{
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    ArduinoOTA.handle();
    enviroTicker.update();
    batteryTicker.update();
    rfTicker.update();
}
void measureBattery()
{
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    int raw = analogRead(A0);
    int level = map(raw, 537, 718, 0, 100);
    display.drawString(0, 10, "Raw:" + String(raw));
    display.drawString(0, 20, "Level:" + String(level) + '%');
    display.display();
    Serial.println("Level:" + String(level) + '%');
    Serial.println("raw:" + String(raw));

    if (location == "Living Room")
    {
        ThingSpeak.setField(5, level);
    }
    else if (location == "Bedroom")
    {
        ThingSpeak.setField(6, level);
    }
    int status = ThingSpeak.writeFields(channelId, writeAPIKey);
    // Serial.println("Battery thing:" + status);
    // display.drawString(0, DISPLAY_HEIGHT - 10, "Battery thing:" + status);
    // display.display();
}
void measureEnvironment()
{
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    float humidity = dht.readHumidity();
    float temp = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temp))
    {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }
    float index = dht.computeHeatIndex(temp, humidity, false);
    Serial.print("Humidity: " + String(humidity) + "%\t");
    Serial.print("Temperature: " + String(temp) + "˚C\t");
    Serial.print("Heat Index:" + String(temp) + "˚C\n");
    display.drawString(0, DISPLAY_HEIGHT / 2 - 10, "Humidity:" + String(humidity) + '%');
    display.drawString(0, DISPLAY_HEIGHT / 2, "Temperature:" + String(temp) + "˚C");
    display.drawString(0, DISPLAY_HEIGHT / 2 + 10, "Heat Index:" + String(index) + "˚C");
    display.display();

    if (location == "Living Room")
    {
        ThingSpeak.setField(1, humidity);
        ThingSpeak.setField(3, temp);
    }
    else if (location == "Bedroom")
    {
        ThingSpeak.setField(2, humidity);
        ThingSpeak.setField(4, temp);
    }
    int status = ThingSpeak.writeFields(channelId, writeAPIKey);
    // Serial.println("Temp thing:" + status);
    // display.drawString(0, DISPLAY_HEIGHT - 10, "Temp thing:" + status);
    // display.display();
}

void rfSend()
{
    living.sendUnit(6, true);
    delay(5000);
    // bedroom.sendUnit(15, false);
    living.sendUnit(6, false);
    delay(5000);
}