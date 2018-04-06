#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <passwords.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const char *ssid = ACCESS_POINT;
const char *password = ACCESS_PASS;
const char *devicePass = ESP_PASS;

String location = "";

WiFiClient wifiClient;
const char *mqtt_server = MQTT_IP;
const char *mqtt_user = MQTT_USER;
const char *mqtt_password = MQTT_PASS;

String sensor = "sensor/";

PubSubClient mqttClient(wifiClient);
long lastMsg = 0;
long now = 600001;

float humidity, temperature, heatIndex = 0.0;
int raw, level = 0;

void setupWifi();
void setupOTA();
void measureBattery();
void measureEnvironment();
void reconnectMqtt();
void setup()
{
    Serial.begin(115200);
    Serial.println("Serial up and running");
    setupWifi();
    setupOTA();
    mqttClient.setServer(mqtt_server, 1883);
}

void loop()
{
    ArduinoOTA.handle();
    if (now - lastMsg > 1000 * 600)
    {
        Serial.println("Running measurements");
        lastMsg = now;
        reconnectMqtt();
        measureBattery();
        measureEnvironment();
    }
    now = millis();
}
void setupOTA()
{
    const char *hostname = location.c_str();
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPassword(devicePass);
    ArduinoOTA.begin();
}
void setupWifi()
{
    String mac = WiFi.macAddress();
    Serial.println("MAC:" + mac);
    if (mac == MAC_BED)
    {
        location = "Bedroom";
        sensor += "bedroom";
    }
    else if (mac = MAC_LIVING)
    {
        location = "Living Room";
        sensor += "livingroom";
    }
    WiFi.persistent(false);
    WiFi.forceSleepWake();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        delay(3000);
        ESP.restart();
    }
    Serial.println("Connected:" + WiFi.localIP().toString());
    Serial.println("Location:" + location);
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
    mqttClient.publish((sensor + "/battery_level").c_str(), String(level).c_str(), true);
    mqttClient.publish((sensor + "/battery_raw").c_str(), String(raw).c_str(), true);
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

    mqttClient.publish((sensor + "/temperature").c_str(), String(temperature).c_str(), true);
    mqttClient.publish((sensor + "/humidity").c_str(), String(humidity).c_str(), true);
    mqttClient.publish((sensor + "/heatindex").c_str(), String(heatIndex).c_str(), true);
}