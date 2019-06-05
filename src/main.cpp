#include <Arduino.h>
#include <avdweb_Switch.h>
#include <NeoPixelBus.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// millis
unsigned long startLongPressMillis;
unsigned long currentLongPressMillis;
const unsigned long longPressDelay = 500;

unsigned long startMQTTmillis;
unsigned long currentMQTTmillis;
const unsigned long MQTTdelay = 5000;

// bulb characteristics

bool On;
float Hue;
float Brightness;
float Saturation;

// multi response button
const byte multiresponseButtonpin = D2;
int longPressActivePixels = 0;
bool pushed;

// payload { On: false, Brightness: 0, Hue: 0, Saturation: 0 }

// LIGHT JSON OBJECT
const size_t capacity = JSON_OBJECT_SIZE(4);

// neopixel
const uint16_t PixelCount = 10; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 20;  // make sure to set this to the correct pin, ignored for Esp8266

#define colorSaturation 128

NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount, PixelPin);

HsbColor hsbBlack(0, 0, 0);

// MQTT
WiFiClient espClient;
PubSubClient MQTTclient(espClient);

#define mqtt_server "homebridge"

void activateStrip() {

  Serial.println();
  Serial.println("Nuvola set new values");
  Serial.print("On ");
  Serial.println(On);
  Serial.print("Brightness ");
  Serial.println(Brightness);
  Serial.print("Hue ");
  Serial.println(Hue);
  Serial.print("Saturation ");
  Serial.println(Saturation);

  HsbColor hsbColor(Hue, Saturation, Brightness);

  for (int i = 0; i <= PixelCount; i++) {
    const HsbColor color = (i < longPressActivePixels && On) ? hsbColor : hsbBlack;
    strip.SetPixelColor(i, color);
  }

  strip.Show();
}

void sendNuvolaStatusOverMQTT() {
    // publish nuvola Light Payload

    DynamicJsonDocument MQTTPubPayload(capacity);

    MQTTPubPayload["On"] = On;
    MQTTPubPayload["Hue"] = Hue * 360;
    MQTTPubPayload["Brightness"] = Brightness * 100;
    MQTTPubPayload["Saturation"] = Saturation * 100;

    char buffer[512];
    size_t n = serializeJson(MQTTPubPayload, buffer);
    MQTTclient.publish("lights/nuvola2/getState", buffer, n);
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {

  DynamicJsonDocument MQTTSubPayload(capacity);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  deserializeJson(MQTTSubPayload, payload, length);

  if (MQTTSubPayload.containsKey("On")) {
    On = MQTTSubPayload["On"];
    if (On && !Brightness) {
      Brightness = 1;
    }
  }
  if (MQTTSubPayload.containsKey("Hue")) {
    Hue = MQTTSubPayload["Hue"];
    Hue /= 360;
  }
  if (MQTTSubPayload.containsKey("Brightness")) {
    Brightness = MQTTSubPayload["Brightness"];
    Brightness /= 100;
  }
  if (MQTTSubPayload.containsKey("Saturation")) {
    Saturation = MQTTSubPayload["Saturation"];
    Saturation /= 100;
  }

  activateStrip();
}

boolean reconnect() {

  if (MQTTclient.connect("Nuvola")) {

    Serial.println("MQTT is connected...");
    MQTTclient.subscribe("lights/nuvola2/setState");

    sendNuvolaStatusOverMQTT();

  }

  return MQTTclient.connected();
}

Switch multiresponseButton = Switch(multiresponseButtonpin);

void setup() {

    Serial.begin(115200);
    while (!Serial); // wait for serial attach

    // init strip obj
    On = false;
    Brightness = 0;
    Hue = 0;
    Saturation = 0;

    Serial.flush();

    Serial.println("Initializing... Nuvola");
    Serial.println();

    WiFiManager wifiManager;

    //reset saved settings
    //wifiManager.resetSettings();

    wifiManager.autoConnect("Nuvola");

    Serial.println("Nuvola connected to WiFi :)");

    // this sets the MQTT client and its callback
    Serial.println("Setting MQTT client...");
    MQTTclient.setServer(mqtt_server, 1883);
    MQTTclient.setCallback(MQTTcallback);
    startMQTTmillis = 0;

    // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();

    Serial.println();
    Serial.println("Running...");

    pushed = false;
}

void loop() {

  if (!MQTTclient.connected()) {

    currentMQTTmillis = millis();

    if (currentMQTTmillis - startMQTTmillis > MQTTdelay) {

      Serial.println("MQTT not connected...");
      Serial.println("reconnecting...");
      startMQTTmillis = currentMQTTmillis;

      // Attempt to reconnect
      if (reconnect()) {
        startMQTTmillis = 0;
      }
    }
  } else {

    // Client connected
    MQTTclient.loop();
  }

  multiresponseButton.poll();

  if (multiresponseButton.pushed()) {

    Serial.println("Pushed");
    startLongPressMillis = millis();
    pushed = 1;

  }

  if (multiresponseButton.released()) {

    Serial.println("Released");
    pushed = 0;

  }

  // get the current "time" (actually the number of milliseconds since the program started)
  currentLongPressMillis = millis();

  //test whether the longPressDelay has elapsed
  if (currentLongPressMillis - startLongPressMillis >= longPressDelay && pushed) {


    if (longPressActivePixels <= PixelCount) {

      longPressActivePixels += 2;

      On = true;
      Brightness = 1;
      Hue = 0;
      Saturation = 0;

    } else {

      longPressActivePixels = 0;

      On = false;
      Brightness = 0;
      Hue = 0;
      Saturation = 0;

    }

    Serial.print("Activated ");
    Serial.println(longPressActivePixels);

    activateStrip();
    sendNuvolaStatusOverMQTT();

    startLongPressMillis = currentLongPressMillis;
  }

  if ( multiresponseButton.singleClick() ) {

    Serial.println("Click");

    if (On) {

      On = false;
      Brightness = 0;
      Hue = 0;
      Saturation = 0;

      activateStrip();

    } else {

      On = true;
      Brightness = 1;
      Hue = 0;
      Saturation = 0;

      activateStrip();
    }

    sendNuvolaStatusOverMQTT();

  }
}

