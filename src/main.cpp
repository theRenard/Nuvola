#include <Arduino.h>
#include <avdweb_Switch.h>
#include <NeoPixelBus.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Fsm.h>


// millis
unsigned long startLongPressMillis;
unsigned long currentLongPressMillis;
const unsigned long longPressDelay = 500;

unsigned long startMQTTmillis;
unsigned long currentMQTTmillis;
const unsigned long MQTTdelay = 5000;

unsigned long startTEMPmillis;
unsigned long currentTEMPmillis;
const unsigned long TEMPdelay = 60000;

// bulb characteristics

bool On;
float Hue;
float Brightness;
float Saturation;

// multi response button
const byte multiresponseButtonpin = D2;
int longPressActivePixels = 10;
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

// DHT

#define DHTPIN D1 // Pin which is connected to the DHT sensor.
#define DHTTYPE DHT11 // DHT 11

DHT_Unified dht(DHTPIN, DHTTYPE);

// MQTT
WiFiClient espClient;
PubSubClient MQTTclient(espClient);

#define mqtt_server "homebridge"

void activateStrip() {

  Serial.println();
  Serial.println("Nuvola set new values");
  Serial.print("Active Pixels ");
  Serial.println(longPressActivePixels);
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
    MQTTclient.publish("lights/nuvola/getState", buffer, n);
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
    MQTTclient.subscribe("lights/nuvola/setState");

    sendNuvolaStatusOverMQTT();

  }

  return MQTTclient.connected();
}

Switch multiresponseButton = Switch(multiresponseButtonpin);

// sound detection FSM

const byte soundDetectorSensor = D5;
int soundDetectedVal;
int SOUND = 1;

void sendNuvolaMotionDetectionStatusOverMQTT(bool state) {
    // publish nuvola Motion Detection Payload

    DynamicJsonDocument MQTTPubPayload(capacity);

    MQTTPubPayload["MotionDetected"] = state;

    char buffer[32];
    size_t n = serializeJson(MQTTPubPayload, buffer);
    MQTTclient.publish("sensors/nuvola/soundSensor/getState", buffer, n);
}

void initialStateEnter() {
  Serial.println("initialStateEnter");
}

void waitStateEnter() {
  Serial.println("waitStateEnter");
}

void soundDetectedStateEnter() {
  Serial.println("soundDetectedStateEnter");
}

void motionDetectedStateEnter() {
  sendNuvolaMotionDetectionStatusOverMQTT(true);
  Serial.println("motionDetectedStateEnter");
}

void finalStateEnter() {
  sendNuvolaMotionDetectionStatusOverMQTT(false);
  Serial.println("finalStateEnter");
}

void onStateExit() {
}

State initialState(&initialStateEnter, &onStateExit, NULL);
State waitState(&waitStateEnter, &onStateExit, NULL);
State soundDetectedState(&soundDetectedStateEnter, &onStateExit, NULL);
State motionDetectedState(&motionDetectedStateEnter, &onStateExit, NULL);
State finalState(&finalStateEnter, &onStateExit, NULL);

Fsm fsm(&initialState);



void setup() {

    Serial.begin(115200);
    while (!Serial); // wait for serial attach

    dht.begin();
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println("Initializing DHT");

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

    // add fsm transitions
    fsm.add_transition(&initialState, &waitState, SOUND, NULL);
    fsm.add_timed_transition(&waitState, &soundDetectedState, 10000, NULL);
    fsm.add_transition(&soundDetectedState, &motionDetectedState, SOUND, NULL);
    fsm.add_timed_transition(&soundDetectedState, &initialState, 5000, NULL);
    fsm.add_transition(&motionDetectedState, &motionDetectedState, SOUND, NULL);
    fsm.add_timed_transition(&motionDetectedState, &finalState, 5000, NULL);
    fsm.add_transition(&finalState, &motionDetectedState, SOUND, NULL);
    fsm.add_timed_transition(&finalState, &initialState, 5000, NULL);

    Serial.println();
    Serial.println("Running...");

    pushed = false;
}

void loop() {

    currentTEMPmillis = millis();

    if (currentTEMPmillis - startTEMPmillis > TEMPdelay) {

      DynamicJsonDocument MQTTPubSensorPayload(capacity);

      // Get temperature event and print its value.
      sensors_event_t event;
      dht.temperature().getEvent(&event);
      if (isnan(event.temperature)) {
        Serial.println("Error reading temperature!");
      }
      else {
        Serial.print("Temperature: ");
        Serial.print(event.temperature);
        Serial.println(" *C");
        MQTTPubSensorPayload["Temperature"] = event.temperature;
      }
      // Get humidity event and print its value.
      dht.humidity().getEvent(&event);
      if (isnan(event.relative_humidity)) {
        Serial.println("Error reading humidity!");
      }
      else {
        Serial.print("Humidity: ");
        Serial.print(event.relative_humidity);
        Serial.println("%");
        MQTTPubSensorPayload["Humidity"] = event.relative_humidity;
      }

      char buffer[512];
      size_t n = serializeJson(MQTTPubSensorPayload, buffer);
      MQTTclient.publish("sensors/nuvola/getState", buffer, n);

      startTEMPmillis = currentTEMPmillis;

    }

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


    if (longPressActivePixels <= PixelCount - 2) {

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

      if (longPressActivePixels == 0) longPressActivePixels = 10;

      On = true;
      Brightness = 1;
      Hue = 0;
      Saturation = 0;

      activateStrip();
    }

    sendNuvolaStatusOverMQTT();

  }

  soundDetectedVal = digitalRead(soundDetectorSensor);

  if (soundDetectedVal == 0) fsm.trigger(SOUND);

  fsm.run_machine();

}

