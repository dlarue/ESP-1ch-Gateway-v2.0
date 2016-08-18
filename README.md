#Single Channel LoRaWAN Gateway

Version 2.0, August 13, 2016
Author: M Westenberg (mw12554@hotmail.com)
Copyright: M Westenberg (mw12554@hotmail.com)

This repository contains a proof-of-concept implementation of a single
channel LoRaWAN gateway. It has been tested on the Wemos D1 Mini, using a 
HopeRF RFM95W transceiver.  The nodes tested are:
- TeensyLC with HopeRF RFM95 radio
- Arduino Pro-Mini (default Armega328 model, 8MHz 3.3V)

###Note

There seems to be a lot of variation in Arduino Pro-Mini devices. I have at least 4 different types. 
Some work, and some do not work with the 1ch Gateway mostly due to timing issues so it seems.
The standard 8MHz type with the large chrystal on board seems to be working best.

The code is for testing and development purposes only, and is not meant 
for production usage yet. 

Version 1 was originally based on code base of Single Channel gateway for RaspberryPI
which was developed by Thomas Telkamp. Code was ported and extended to run on ESP 8266 
mcu and provide RTC, Webserver and DNS services.
Version 2.0 adds several enhancements and part have been completely redesigned.
Changes include two-way traffic. The code is also slit over multiple source files
which makes editing easier.

Maintained by Maarten Westenberg (mw12554@hotmail.com)

##Features

- Supports ABP nodes (TeensyLC and Arduino Pro-mini)
- Supports OTAA functions on TeensyLC and Arduino Pro-Mini (not all of them).
- Supports SF7, SF8. SF7 is tested for downstream communication
- Listens on configurable frequency and spreading factor
- Send status updates to server (keepalive)
- PULL_DATA messages to server
- It can forward messages to two servers at the same time (and read from them as well)
- DNS support for server lookup
- NTP Support for time sync with internet time servers
- Webserver support (default port 8080)
- .h header file for configuration

Not (yet) supported:

- SF7BW250 modulation
- FSK modulation
- SF9-SF12 rates not tested yet

##Dependencies

The software is dependent on several pieces of software, the Ardiuino IDE for ESP8266 
being the most important.
Several other libraries are also used by this program:

- gBase64 library, The gBase library is actually a base64 library made 
	by Adam Rudd (url=https://github.com/adamvr/arduino-base64). I changed the name because I had
	another base64 library installed on my system and they did not coexist well.
- Time library (http://playground.arduino.cc/code/time)
- Arduino JSON; Needed to decode downstream messages
- SimpleTimer; ot yet used, but reserved for interrupt and timing

##Connections

See http://things4u.github.io in the hardware section for building
and connection instructions

##Configuration

All user configurable settings are put in the ESP-sc-gway.h file as much as possible.
The most important things to configure to your own environment are:

- static char *wpa[WPASIZE][2] contains the array of known WiFi access points the Gateway will connect to.
Make sure that the dimensions of the array are correctly defined in the WPASIZE settings. 
- Only the sx1276 (and HopeRF 95) radio modules are supported at this time. The sx1272 code should be 
working without much works, but as I do not have one of these modules available I cannot test this.
- This software allows to connect to 2 servers at the same time (as most gateways do BTW). 
Make sure that you set:

 \#define _THINGPORT 1701							// Your UDP server should listen to this port  
 \#define _THINGSERVER "your_server.com"			// Server URL of the LoRa udp.js server program  

- Set the identity parameters for your gateway:  
// Gateway Ident definitions, please set location, email and description.  

\#define _DESCRIPTION "ESP Gateway"  
\#define _EMAIL "your.email@provider.com"  
\#define _PLATFORM "ESP8266"  
\#define _LAT 52.00  
\#define _LON 5.00  
\#define _ALT 0  


###Lora Radio Defaults:  

- LoRa:   SF7 at 868.1 Mhz
- Server:  
  \#define _TTNSERVER "router.eu.thethings.network"  
  \#define _TTNPORT 1700  
  These two settings are mandatory and should point to the standard servers of TTN
  40.114.249.243, port 1700 
  
Edit .h file (ESP-sc-gway.h) to change configuration (look for: "Configure these values!").

##Notes

The Gateay timestamps are according to the LoRa specification: 
- Receive_Delay1 1s
- Receive Delay2	2s (starting after Receive_Delay1)
- Join_Accept_Delay1 5s
- Join_Accept_Delay2 6s


##License

The source files in this repository are made available under the Eclipse
Public License v1.0, except for the base64 implementation, that has been
copied from the Semtech Packet Forwader.
