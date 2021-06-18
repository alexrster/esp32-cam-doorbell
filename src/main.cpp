#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "version.h"
#include "camera.cpp"

#define WIFI_SSID                     "qx.zone"
#define WIFI_PASSPHRASE               "1234Qwer-"
#define WIFI_RECONNECT_MILLIS         1000
#define WIFI_WATCHDOG_MILLIS          5*60000

#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME                 "doorbell-cam-01"
#endif

#define MQTT_SERVER_NAME              "ns2.in.qx.zone"
#define MQTT_SERVER_PORT              1883
#define MQTT_USERNAME                 NULL
#define MQTT_PASSWORD                 NULL
#define MQTT_RECONNECT_MILLIS         5000

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID                WIFI_HOSTNAME
#endif

#define MQTT_STATUS_TOPIC             MQTT_CLIENT_ID "/status"
#define MQTT_VERSION_TOPIC            MQTT_CLIENT_ID "/version"
#define MQTT_RESTART_CONTROL_TOPIC    MQTT_CLIENT_ID "/restart"

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
static const String PubSubRestartControlTopic = String(MQTT_RESTART_CONTROL_TOPIC);

unsigned long 
  now = 0,
  lastWifiOnline = 0,
  lastPubSubReconnectAttempt = 0;

void onMqttMessage(char* topic, byte* payload, unsigned int length);

void otaStarted()
{ }

void otaEnd()
{ 
  ESP.restart();
}

void otaError(ota_error_t)
{ 
  ESP.restart();
}

void wifi_setup()
{
  WiFi.enableAP(false);
  WiFi.enableSTA(true);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);

  ArduinoOTA.onStart(otaStarted);
  ArduinoOTA.onEnd(otaEnd);
  ArduinoOTA.onError(otaError);
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.begin();

  pubSubClient.setCallback(onMqttMessage);
  pubSubClient.setServer(MQTT_SERVER_NAME, MQTT_SERVER_PORT);

  now = millis();
  lastWifiOnline = now;
}

bool reconnectPubSub() 
{
  if (now - lastPubSubReconnectAttempt > MQTT_RECONNECT_MILLIS) {
    lastPubSubReconnectAttempt = now;

    if (pubSubClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_STATUS_TOPIC, MQTTQOS0, true, "0", false)) {
      pubSubClient.publish(MQTT_STATUS_TOPIC, "1", true);
      pubSubClient.publish(MQTT_VERSION_TOPIC, VERSION, true);
      
      pubSubClient.subscribe(MQTT_RESTART_CONTROL_TOPIC, MQTTQOS0);
    }
    
    return pubSubClient.connected();
  }

  return false;
}

void pubSubClientLoop() 
{
  if (!pubSubClient.connected() && !reconnectPubSub()) return;

  pubSubClient.loop();
}

void setup()
{
  wifi_setup();
  camera_setup();
}

void loop()
{
  now = millis();
  camera_loop(now);
}

void onMqttMessage(char* topic, byte* payload, unsigned int length)
{
  if (length == 0) return;

  // if (PubSubSwitchControlTopic == (String)topic) {
  //   switch (length) {
  //     case 1: 
  //       switch (payload[0]) {
  //         case '1': setSwitch(On); return;
  //         case '0': setSwitch(Off); return;
  //         default: return;
  //       }
  //       break;
  //     case 2:
  //     case 3:
  //       switch (payload[1]) {
  //         case 'n': setSwitch(On); return;
  //         case 'f': setSwitch(Off); return;
  //       }
  //       break;
  //     case 4: setSwitch(On); return;
  //     case 5: setSwitch(Off); return;
  //     default: return;
  //   }
  // }
  // else
  if (PubSubRestartControlTopic == (String)topic) {
    if (payload[0] == '1') ESP.restart();
  }
}
