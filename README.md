# Nuvola

## A connected lamp for my daughter

Connected means that it sends and receive MQTT messages with the following infos:

- Temperature
- Humidity
- Noise level
- Light (an array of 10 neopixels) with Hue, Brightness and Saturation

A Node-RED server interacts with an homebridge instance to convert MQTT messages into Siri™ actions and vice-versa. This is handy with homekit scenes, for instance dim the light intensity when is time to sleep and turn it off late at night. 

This is a very personal project with no documentation but two pictures ¯\\_(ツ)_/¯. Feel free to ask for more info if you need. The PCB is the one from the Rabbit project as I only needed three GPIO.

💻 Esp8266
💡 Neopixels
💻 Node-RED, homebridge


![](/circuit.jpeg)

![](/nuvola.jpeg)

![](/home.jpeg)

