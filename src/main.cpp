#include <Arduino.h>
#include <avdweb_Switch.h>
#include <NeoPixelBus.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT

WiFiClient espClient;
PubSubClient MQTTclient(espClient);

#define mqtt_server "homebridge"

// buffer MQTT
long lastReconnectAttempt;

void MQTTcallback(char* topic, byte* payload, unsigned int length) {

  StaticJsonDocument<404> doc;
  deserializeJson(doc, payload, length);

  const char* value = doc["key"];
  Serial.println(value);
  Serial.print("payload");

}

boolean reconnect() {
  if (MQTTclient.connect("Nuvola")) {
    Serial.println("MQTT is connected...");
    // Once connected, publish an announcement...
    MQTTclient.publish("outTopic","hello world");
    // ... and resubscribe
    MQTTclient.subscribe("inTopic");
  }
  return MQTTclient.connected();
}


// millis
unsigned long startLongPressMillis;
unsigned long currentLongPressMillis;
const unsigned long longPressDelay = 500;

unsigned long startMQTTmillis;
unsigned long currentMQTTmillis;
const unsigned long MQTTdelay = 500;

// multi response button
const byte multiresponseButtonpin = D2;
int ledState = 0;
int activePixels = 0;
bool pushed;

Switch multiresponseButton = Switch(multiresponseButtonpin);

// neopixel
const uint16_t PixelCount = 10; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 20;  // make sure to set this to the correct pin, ignored for Esp8266

#define colorSaturation 128

NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount, PixelPin);

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

HslColor hslRed(red);
HslColor hslGreen(green);
HslColor hslBlue(blue);
HslColor hslWhite(white);
HslColor hslBlack(black);

void setup()
{
    Serial.begin(115200);
    while (!Serial); // wait for serial attach

    Serial.println();
    Serial.println("Initializing... Nuvola");
    Serial.flush();

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

    Serial.println("MQTT not connected...");
    currentMQTTmillis = millis();

    if (currentMQTTmillis - startMQTTmillis > MQTTdelay) {

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

    Serial.println("multiresponseButton pushed");
    startLongPressMillis = millis();
    pushed = 1;

  }

  if (multiresponseButton.released()) {

    Serial.println("multiresponseButton released");
    pushed = 0;

  }

  // get the current "time" (actually the number of milliseconds since the program started)
  currentLongPressMillis = millis();

  //test whether the longPressDelay has elapsed
  if (currentLongPressMillis - startLongPressMillis >= longPressDelay && pushed) {


    if (activePixels <= 10) {
      activePixels += 2;
    } else {
      activePixels = 0;
    }

    Serial.println(activePixels);
    Serial.print(" Activated");

    for (int i = 0; i <= PixelCount; i++) {
      const HslColor color = (i < activePixels) ? hslWhite : hslBlack;
        strip.SetPixelColor(i, color);
    }

    strip.Show();

    startLongPressMillis = currentLongPressMillis;  //IMPORTANT to save the start time of the current LED state.
  }

  if ( multiresponseButton.singleClick() ) {

    Serial.println("multiresponseButton click");

     if (!ledState) {

        // set the colors,
        strip.SetPixelColor(0, hslRed);
        strip.SetPixelColor(1, hslGreen);
        strip.SetPixelColor(2, hslBlue);
        strip.SetPixelColor(3, hslWhite);
        strip.Show();
        ledState = true;
        activePixels = 0;

      } else {

        // turn off the pixels
        strip.SetPixelColor(0, hslBlack);
        strip.SetPixelColor(1, hslBlack);
        strip.SetPixelColor(2, hslBlack);
        strip.SetPixelColor(3, hslBlack);
        strip.Show();
        ledState = false;
      }
  }
}

