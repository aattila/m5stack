# Internet Radio with M5Stack Node Audio (WM8978)

This project is made by using the:
- [M5Stack Node Audio](https://m5stack.com/collections/m5-base/products/node-module) development module
- [M5Stack Core](https://m5stack.com/collections/m5-core/products/basic-core-iot-development-kit) black 
- Micro SD card

![Modules](https://github.com/aattila/m5stack/raw/master/M5StackIR/img/modules.jpg)

It is a full featured internet radio where the stations can be stored on the SD card or you can set inline in the code. The radio is having two modes:
- Playing the station
- Paused and showing the station list to select a new station

## Playing the Station

In this mode tha station is streaming and the music is played. The elements on the screen are:
- Top Bar: 
  - Is wifi connected
  - Is SD card is inserted
  - The status of the stream buffer
  - The music's bitrate
- In the middle: Is listed a part of the stations and the current one is highlighted with red
- Bottom Bar: Shows the volume. If grey you are in headphone mode if orange in speaker mode

Photo playing a station in headphone mode:

![Playing](https://github.com/aattila/m5stack/raw/master/M5StackIR/img/headphonemode.jpg)

Photo playing a station in speaker mode:

![Playing](https://github.com/aattila/m5stack/raw/master/M5StackIR/img/speakermode.jpg)

In this mode the buttons are having the following functions:
- A (left): Decrease the volume
- B (middle) normal push: Pause the stream and set the station select mode
- B (middle) long push (> 2 sec): Set the speaker mode, pushing long again sets the headphone mode
- C (right): Increase the volume

## Select a Station

If you are pushing the middle button the stream is stopped and you have the chance to select another station.

![Stations](https://github.com/aattila/m5stack/raw/master/M5StackIR/img/stationsselect.jpg)

In this mode the buttons are having the following functions:
- A (left): Previous station
- B (middle): Station selected and going back to the playing mode
- C (right): Next station

## Stations and WiFi

You have two possibilities to set stations and the connections to the WiFi network:
- Use an SD card:
  - create a file named ```M5StackIR.cfg```
  - the first row must be the wifi credentials in the format: ```"username", "password"``` (the quotes are mandatory)
  - all the other rows must be in the format: ```"station name", "station url that points to a playeable stream"``` (the quotes are mandatory)
- Set inline the relevant fields:
  - for the stations set or expand the field named ```bkpStations```
  - for the WiFi set the ```WIFI_SSID``` and the ```WIFI_PASSWORD``` (those are set inside in the ```credentials.h`` based on the ide of [Andreas Spiess](https://www.sensorsiot.org) )
  
A sample SD card config file could be like the following example:
```
"your ssid", "your wifi password"
"Hirschmilch Chillout", "http://hirschmilch.de:7000/chillout.mp3"
"Antenne Bayern Chillout", "http://mp3channels.webradio.antenne.de/chillout?type=.mp3"
"Radio Gaia - Chill Out", "http://streamingV2.shoutcast.com/Chill-Out-Radio-Gaia?lang=en-GB%2cen-US%3bq%3d0.9%2cen%3bq%3d0.8"
"Psychedelik - Ambient", "https://stream.psychedelik.com:8002/"
"Psychedelik - PsyTrance", "https://stream.psychedelik.com:8000/"
```
I not checked how many entries could be used but the ESP32 is having a limited memory so take care.

## Known issues

- Not all the time is connecting at first to the wifi but after a reset (switch button pressed one time) will work.
- If a thation is having a broken url the device will restart (solution in the chapter: Internal State)

## Prerequisites

To succesfuly compile the code you have to install two libraries:
- [wm8978-esp32](https://github.com/CelliesProjects/wm8978-esp32) it will work from Arduino IDE with manage libraries
- [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) you have to install manually and it needs some attention:
  - After extracting the zip file copy the folder into: ```<arduino data folder>/packages/esp32/hardware/esp32/1.0.4/libraries``` 
  - Edit the file ```<arduino data folder>/packages/esp32/hardware/esp32/1.0.4/libraries//ESP-audioI2S-master/src/Audio.cpp``` and comment the line 134: ```clientsecure.setInsecure();```
  
### arduino-esp32 version 1.0.5-rcX

The ```Audio.cpp``` suggesting to you to install this release candidate version (this is why you need to comment out that line) but this version is having some problems:
- ```esptool.py``` is not working with MacOS Big Sur (you can workarount that by replacing the os binary ```esptool``` with the provided ```esptool.py```, just edit the ```platform.txt``` file)
- The compiled project will not work consistently because there is a wifi related bug and sometimes the button A (left) will receive the pushed event (many events)

## Internal State

There is some data that will be persisted into the SD card into a hidden file named ```.M5StackIR``` this will have the following entries:
```
stationIdx,isSpkMode,spkVolume,hpVolume[0],hpVolume[1]
``` 
More exactly: the current station, speaker or headphone mode, speaker volume, headphone left volume, headphone right volume

If you have a broken station url the only solution to not having continuous reatarts is to edit ths file and set another station index!

## Challanges

There is a lot of more fun here by using the embedded:
- infrared send and receive mechanism
- DHT temperatire senzor
- LED backplate

In the feature releases I'll do something in that way but until that feel free and fork ;)



  

