#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <PubSubClient.h>

SSD1306Wire display(0x3c, 4, 5);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BUTTONPIN 12

#define ssid "***REMOVED***"
#define password "***REMOVED***"
#define devicePass "***REMOVED***"

String location = "";

WiFiClient wifiClient;

#define mqtt_server "***REMOVED***"
#define mqtt_user "***REMOVED***"
#define mqtt_password "***REMOVED***"

String sensor = "sensor/";

PubSubClient mqttClient(wifiClient);
long lastMsg = 60000;

float humidity, temperature, heatIndex = 0.0;
int raw, level = 0;

void setupWifi();
void setupOTA();
void setupDisplay();
void measureBattery();
void measureEnvironment();
void reconnectMqtt();
void displayInterrupt();

void setup()
{
    Serial.begin(115200);
    Serial.println("Serial up and running");
    pinMode(BUTTONPIN, INPUT_PULLUP);
    attachInterrupt(BUTTONPIN, displayInterrupt, RISING);
    setupDisplay();
    setupWifi();
    setupOTA();
    // mqttClient.setServer(mqtt_server, 1883);
    Serial.println("Setup completed, turning display off");
    display.clear();
    // display.displayOff();
}

void loop()
{
    ArduinoOTA.handle();
    long now = millis();
    display.clear();
    if (now - lastMsg > 1000 * 600)
    {
        Serial.println("Running measurements");
        lastMsg = now;
        // reconnectMqtt();
        measureBattery();
        measureEnvironment();
    }
}
void setupDisplay()
{
    display.init();
    display.flipScreenVertically();
    display.setContrast(255);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.clear();
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
        display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 10, "OTA Update");
        display.display();
    });
    ArduinoOTA.onEnd([]() {
        display.clear();
        display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Restart");
        display.display();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {

        display.drawProgressBar(4, 32, 120, 8, progress / (total / 100));
        display.display();
    });
    ArduinoOTA.onError([](ota_error_t error) {
        display.clear();
        display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 10, "OTA Error :");
        if (error == OTA_AUTH_ERROR)
            display.drawString(display.getWidth() / 2, 32, "Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            display.drawString(display.getWidth() / 2, 32, "Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            display.drawString(display.getWidth() / 2, 32, "Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            display.drawString(display.getWidth() / 2, 32, "Receive Failed");
        else if (error == OTA_END_ERROR)
            display.drawString(display.getWidth() / 2, 32, "End Failed");
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
        sensor += "bedroom";
        // switch_number += '0';
    }
    else if (mac = "***REMOVED***")
    {
        location = "Living Room";
        sensor += "livingroom";
        // switch_number += '1'
    }
    WiFi.persistent(false);
    WiFi.forceSleepWake();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        display.drawString(display.getWidth() / 2, 10, "Connection Failed!");
        display.display();
        delay(3000);
        ESP.restart();
    }

    display.clear();
    display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 10, "IP:" + WiFi.localIP().toString());
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Monitor for:" + location);
    display.display();
    delay(3000);
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    // display.displayOff();
}
void reconnectMqtt()
{

    while (!mqttClient.connected())
    {
        Serial.print("Connecting to MQTT broker ...");
        if (mqttClient.connect("ESP8266Client", mqtt_user, mqtt_password))
        {
            Serial.println("OK");
        }
        else
        {
            Serial.print("KO, error : ");
            Serial.print(mqttClient.state());
            Serial.println(" Wait 5 secondes before to retry");
            delay(5000);
        }
    }
}
void measureBattery()
{
    raw = analogRead(A0);
    level = map(raw, 511, 718, 0, 100);
    Serial.println("Level:" + String(level) + '%');
    Serial.println("raw:" + String(raw));
    // mqttClient.publish((sensor + "/battery_level").c_str(), String(level).c_str(), true);
    // mqttClient.publish((sensor + "/battery_raw").c_str(), String(raw).c_str(), true);
}
void measureEnvironment()
{
    dht.begin();
    delay(500);
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    if (isnan(humidity) || isnan(temperature))
    {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }
    heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    Serial.print("Humidity: " + String(humidity) + "%\t");
    Serial.print("Temperature: " + String(temperature) + "˚C\t");
    Serial.print("Heat Index:" + String(heatIndex) + "˚C\n");

    // mqttClient.publish((sensor + "/temperature").c_str(), String(temperature).c_str(), true);
    // mqttClient.publish((sensor + "/humidity").c_str(), String(humidity).c_str(), true);
    // mqttClient.publish((sensor + "/heatindex").c_str(), String(heatIndex).c_str(), true);
}
void displayInterrupt()
{
    Serial.println("Interrupted, displaying current info");
    // display.displayOn();
    display.clear();
    display.drawString(0, 10, "Raw:" + String(raw));
    display.drawString(0, 20, "Level:" + String(level) + '%');
    display.drawString(0, 30, "Humidity:" + String(humidity) + '%');
    display.drawString(0, 40, "Temperature:" + String(temperature) + "˚C");
    display.drawString(0, 50, "Heat Index:" + String(heatIndex) + "˚C");
    display.display();
    // display.displayOff();
}