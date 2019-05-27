#include <Arduino.h>
#include "avdweb_Switch.h"
#include <NeoPixelBus.h>

// millis
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 1000;

// multi response button
const byte multiresponseButtonpin = D2;
int ledState = 0;
int pixels = 1;
int pushed = 0;

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

void lightUp() {
    for (int i = 0; i <= pixels; i++) {
        strip.SetPixelColor(pixels, hslRed);
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial); // wait for serial attach

    Serial.println();
    Serial.println("Initializing...");
    Serial.flush();

    // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();


    Serial.println();
    Serial.println("Running...");
}


void loop() {

  multiresponseButton.poll();

  if (multiresponseButton.pushed()) {

    Serial.println("multiresponseButton pushed");
    startMillis = millis();
    pushed = 1;

  }

  if (multiresponseButton.released()) {

    Serial.println("multiresponseButton released");
    pushed = 0;

  }

  // get the current "time" (actually the number of milliseconds since the program started)
  currentMillis = millis();

  //test whether the period has elapsed
  if (currentMillis - startMillis >= period && pushed) {
    Serial.print("now");
    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }

  if ( multiresponseButton.singleClick() ) {
    Serial.println("multiresponseButton click");
     if (ledState == 1) {
        // set the colors,
        strip.SetPixelColor(0, hslRed);
        strip.SetPixelColor(1, hslGreen);
        strip.SetPixelColor(2, hslBlue);
        strip.SetPixelColor(3, hslWhite);
        strip.Show();
        ledState = 0;
      } else {
        // turn off the pixels
        strip.SetPixelColor(0, hslBlack);
        strip.SetPixelColor(1, hslBlack);
        strip.SetPixelColor(2, hslBlack);
        strip.SetPixelColor(3, hslBlack);
        strip.Show();
        ledState = 1;
      }
  }

}
