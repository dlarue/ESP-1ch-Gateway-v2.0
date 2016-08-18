/*******************************************************************************
 * Copyright (c) 2016 Maarten Westenberg version for ESP8266
 *
 * 	based on work done by Thomas Telkamp for Raspberry PI 1ch gateway
 *	and many others.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * The protocols used in this 1ch gateway: 
 * 1. LoRA Specification version V1.0 and V1.1 for Gateway-Node communication
 *	
 * 2. Semtech Basic communication protocol between Lora gateway and server version 3.0.0
 *	https://github.com/Lora-net/packet_forwarder/blob/master/PROTOCOL.TXT
 *
 * Notes: 
 * - Once call gethostbyname() to get IP for services, after that only use IP
 *	 addresses (too many gethost name makes ESP unstable)
 * - Only call yield() in main stream (not for background NTP sync). 
 *
 *******************************************************************************/

//
#define VERSION " ! V. 2.0.0, 160813"

#include <Esp.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <sys/time.h>
#include <cstring>
#include <SPI.h>
#include <TimeLib.h>								// http://playground.arduino.cc/code/time
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
extern "C" {
#include "user_interface.h"
#include "lwip/err.h"
#include "lwip/dns.h"
}
#include <pins_arduino.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#include <gBase64.h>							// https://github.com/adamvr/arduino-base64 (I changed the name)

#include "loraModem.h"
#include "ESP-sc-gway.h"						// This file contains configuration of GWay

int debug=1;									// Debug level! 0 is no msgs, 1 normal, 2 is extensive

using namespace std;

byte currentMode = 0x81;
uint8_t message[256];
char b64[256];
bool sx1272 = true;								// Actually we use sx1276/RFM95
byte receivedbytes;

uint32_t cp_nb_rx_rcv;
uint32_t cp_nb_rx_ok;
uint32_t cp_nb_rx_bad;
uint32_t cp_nb_rx_nocrc;
uint32_t cp_up_pkt_fwd;

enum sf_t { SF7=7, SF8, SF9, SF10, SF11, SF12 };

uint8_t MAC_array[6];
char MAC_char[18];

/*******************************************************************************
 *
 * Configure these values only if necessary!
 *
 *******************************************************************************/

// SX1276 - ESP8266 connections
int ssPin = 15;									// GPIO15, D8
int dio0  = 5;									// GPIO5,  D1
int dio1  = 4;									// GPIO4,  D2
int dio2  = 3;									// GPIO3, !! NOT CONNECTED IN THIS VERSION
int RST   = 0;									// GPIO16, D0, not connected

// Set spreading factor (SF7 - SF12)
sf_t sf 			= _SPREADING ;

// Set location, description and other configuration parameters
// Defined in ESP-sc_gway.h
//
float lat			= _LAT;						// Configuration specific info...
float lon			= _LON;
int   alt			= _ALT;
char platform[24]	= _PLATFORM; 				// platform definition
char email[40]		= _EMAIL;    				// used for contact email
char description[64]= _DESCRIPTION;				// used for free form description 

// define servers

IPAddress ntpServer;							// IP address of NTP_TIMESERVER
IPAddress ttnServer;							// IP Address of thethingsnetwork server
IPAddress thingServer;

WiFiUDP Udp;
uint32_t stattime = 0;							// last time we sent a stat message to server
uint32_t pulltime = 0;							// last time we sent a pull_data request to server
uint32_t lastTmst = 0;

SimpleTimer timer; 								// Timer is needed for delayed sending

// You can switch webserver off if not necessary but probably better to leave it in.
#if A_SERVER==1
#include <Streaming.h>          				// http://arduiniana.org/libraries/streaming/
  String webPage;
  ESP8266WebServer server(SERVERPORT);
#endif




#define TX_BUFF_SIZE  2048						// Upstream buffer to send to MQTT
#define RX_BUFF_SIZE  1024						// Downstream received from MQTT
#define STATUS_SIZE	  512						// This should(!) be enough based on the static text part.. was 1024

uint8_t buff_up[TX_BUFF_SIZE]; 					// buffer to compose the upstream packet
uint8_t buff_down[RX_BUFF_SIZE];
uint16_t lastToken = 0x00;

// ----------------------------------------------------------------------------
// DIE is not use actively in the source code anymore.
// It is replaced by a Serial.print command so we know that we have a problem
// somewhere.
// There are at least 3 other ways to restart the ESP. Pick one if you want.
// ----------------------------------------------------------------------------
void die(const char *s)
{
    Serial.println(s);
	delay(50);
	// system_restart();						// SDK function
	// ESP.reset();				
	abort();									// Within a second
}

// ----------------------------------------------------------------------------
// gway_failed is a function called by ASSERT.
// ----------------------------------------------------------------------------
void gway_failed(const char *file, uint16_t line) {
	Serial.print(F("Program failed in file: "));
	Serial.print(file);
	Serial.print(F(", line: "));
	Serial.print(line);
}

// ----------------------------------------------------------------------------
// Print leading '0' digits for hours(0) and second(0) when
// printing values less than 10
// ----------------------------------------------------------------------------
void printDigits(int digits)
{
    // utility function for digital clock display: prints preceding colon and leading 0
    if(digits < 10)
        Serial.print(F("0"));
    Serial.print(digits);
}


// ----------------------------------------------------------------------------
// Print the current time
// ----------------------------------------------------------------------------
void printTime() {
	char *Days [] ={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
	Serial.print(Days[weekday()-1]);
	Serial.print(F(" "));
	printDigits(hour());
	Serial.print(F(":"));
	printDigits(minute());
	Serial.print(F(":"));
	printDigits(second());
	return;
}


// ----------------------------------------------------------------------------
// Convert a float to string for printing
// f is value to convert
// p is precision in decimal digits
// val is character array for results
// ----------------------------------------------------------------------------
void ftoa(float f, char *val, int p) {
	int j=1;
	int ival, fval;
	char b[6];
	
	for (int i=0; i< p; i++) { j= j*10; }

	ival = (int) f;								// Make integer part
	fval = (int) ((f- ival)*j);					// Make fraction. Has same sign as integer part
	if (fval<0) fval = -fval;					// So if it is negative make fraction positive again.
												// sprintf does NOT fit in memory
	strcat(val,itoa(ival,b,10));
	strcat(val,".");							// decimal point
	
	itoa(fval,b,10);
	for (int i=0; i<(p-strlen(b)); i++) strcat(val,"0");
	// Fraction can be anything from 0 to 10^p , so can have less digits
	strcat(val,b);
}

// =============================================================================
// NTP TIME functions

const int NTP_PACKET_SIZE = 48;					// Fixed size of NTP record
byte packetBuffer[NTP_PACKET_SIZE];

// ----------------------------------------------------------------------------
// Send the request packet to the NTP server.
//
// ----------------------------------------------------------------------------
void sendNTPpacket(IPAddress& timeServerIP) {
  // Zeroise the buffer.
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	packetBuffer[0] = 0b11100011;   			// LI, Version, Mode
	packetBuffer[1] = 0;						// Stratum, or type of clock
	packetBuffer[2] = 6;						// Polling Interval
	packetBuffer[3] = 0xEC;						// Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;	

	Udp.beginPacket(timeServerIP, (int) 123);	// NTP Server and Port

	if ((Udp.write((char *)packetBuffer, NTP_PACKET_SIZE)) != NTP_PACKET_SIZE) {
		die("sendNtpPacket:: Error write");
	}
	else {
		// Success
	}
	Udp.endPacket();
}


// ----------------------------------------------------------------------------
// Get the NTP time from one of the time servers
// Note: As this function is called from SyncINterval in the background
//	make sure we have no blocking calls in this function
// ----------------------------------------------------------------------------
time_t getNtpTime()
{
  WiFi.hostByName(NTP_TIMESERVER, ntpServer);
  for (int i = 0 ; i < 4 ; i++) { 				// 5 retries.
    sendNTPpacket(ntpServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 6000) 
	{
      if (Udp.parsePacket()) {
        Udp.read(packetBuffer, NTP_PACKET_SIZE);
        // Extract seconds portion.
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secSince1900 = highWord << 16 | lowWord;
        Udp.flush();
        return secSince1900 - 2208988800UL + NTP_TIMEZONES * SECS_PER_HOUR;				
		// UTC is 1 TimeZone correction when no daylight saving time
      }
      //delay(10);
    }
  }
  return 0; 									// return 0 if unable to get the time
}

// ----------------------------------------------------------------------------
// Set up regular synchronization of NTP server and the local time.
// ----------------------------------------------------------------------------
void setupTime() {
  setSyncProvider(getNtpTime);
  setSyncInterval(_NTP_INTERVAL);
}



// ============================================================================
// UDP AND WLAN FUNCTIONS

// ----------------------------------------------------------------------------
// GET THE DNS SERVER IP address
// ----------------------------------------------------------------------------
IPAddress getDnsIP() {
	ip_addr_t dns_ip = dns_getserver(0);
	IPAddress dns = IPAddress(dns_ip.addr);
	return((IPAddress) dns);
}




// ----------------------------------------------------------------------------
// Function to join the Wifi Network
// XXX Maybe we should make the reconnect shorter in order to avoid watchdog resets.
//	It is a matter of returning to the main loop() asap and make sure in next loop
//	the reconnect is done first thing.
// ----------------------------------------------------------------------------
int WlanConnect() {
	// We start by connecting to a WiFi network 
  
  unsigned char agains = 0;
  unsigned char wpa_index = 0;
  int ledStatus = LOW;
  while (WiFi.status() != WL_CONNECTED)
  {
    char *ssid = wpa[wpa_index][0];
	char *password = wpa[wpa_index][1];
	Serial.print(F("WiFi connect to: ")); Serial.println(ssid);
	WiFi.begin(ssid, password);
	
	while (WiFi.status() != WL_CONNECTED) {
		agains++;
		delay(agains*500);
		digitalWrite(BUILTIN_LED, ledStatus); 	// Write LED high/low
		ledStatus = (ledStatus == HIGH) ? LOW : HIGH;
		if (debug>=2) Serial.print(".");
		yield();
		
		// If after 10 times there is still no connection, we probably wait forever
		// So restart the WiFI.begin process!!
		if (agains == 10) {
			agains = 0;
			Serial.println();
			WiFi.disconnect();
			yield();
			delay(500);
			break;
		}
	}
	wpa_index++;
	if (wpa_index >= WPASIZE) { return(-1); }
  }
  Serial.print(F("WiFi connected. local IP address: ")); 
  Serial.println(WiFi.localIP());
  yield();
  return(0);
}


// ----------------------------------------------------------------------------
// Read DOWN a package from UDP socket, can come from any server
// Messages are received when server responds to gateway requests from LoRa nodes 
// (e.g. JOIN requests etc.) or when server has downstream data.
// We repond only to the server that sent us a message!
// ----------------------------------------------------------------------------
int readUdp(int packetSize, uint8_t * buff_down)
{
  uint8_t protocol;
  uint16_t token;
  uint8_t ident; 
  char LoraBuffer[64]; 						//buffer to hold packet to send to LoRa node
  
  if (packetSize > RX_BUFF_SIZE) {
	Serial.print(F("readUDP:: ERROR package of size: "));
	Serial.println(packetSize);
	Udp.flush();
	return(-1);
  }
  
  Udp.read(buff_down, packetSize);
  IPAddress remoteIpNo = Udp.remoteIP();
  unsigned int remotePortNo = Udp.remotePort();

  uint8_t * data = buff_down + 4;
  protocol = buff_down[0];
  token = buff_down[2]*256 + buff_down[1];
  ident = buff_down[3];
  
  // now parse the message type from the server (if any)
  switch (ident) {
	case PKT_PUSH_DATA: // 0x00 UP
		if (debug >=1) {
			Serial.print(F("PKT_PUSH_DATA:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);
			Serial.print(F(", data: "));
			for (int i=0; i<packetSize; i++) {
				Serial.print(buff_down[i],HEX);
				Serial.print(':');
			}
			Serial.println();
		}
	break;
	case PKT_PUSH_ACK:	// 0x01 DOWN
		if (debug >= 1) {
			Serial.print(F("PKT_PUSH_ACK:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);
			Serial.print(F(", token: "));
			Serial.println(token, HEX);
		}
	break;
	case PKT_PULL_DATA:	// 0x02 UP
		Serial.print(F(" Pull Data"));
		Serial.println();
	break;
	case PKT_PULL_RESP:	// 0x03 DOWN
	
		lastTmst = micros();					// Store the tmst this package was received
		// Send to the LoRa Node first (timing) and then do messaging
		if (sendPacket(data, packetSize-4) < 0) {
			return(-1);
		}
		
		// Now respond with an PKT_PULL_ACK; 0x04 UP
		buff_up[0]=buff_down[0];
		buff_up[1]=buff_down[1];
		buff_up[2]=buff_down[2];
		buff_up[3]=PKT_PULL_ACK;
		buff_up[4]=0;
		
		// Only send the PKT_PULL_ACK to the UDP socket that just sent the data!!!
		Udp.beginPacket(remoteIpNo, remotePortNo);
		if (Udp.write((char *)buff_up, 4) != 4) {
			Serial.println("PKT_PULL_ACK:: Error writing Ack");
		}
		else {
			if (debug>=1) {
				Serial.print(F("PKT_PULL_ACK:: tmst="));
				Serial.println(micros());
			}
		}
		//yield();
		Udp.endPacket();
		
		if (debug >=1) {
			Serial.print(F("PKT_PULL_RESP:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);	
			Serial.print(F(", data: "));
			data = buff_down + 4;
			data[packetSize] = 0;
			Serial.print((char *)data);
			Serial.println(F("..."));
		}
		
	break;
	case PKT_PULL_ACK:	// 0x04 DOWN; the server sends a PULL_ACK to confirm PULL_DATA receipt
		if (debug >= 2) {
			Serial.print(F("PKT_PULL_ACK:: size ")); Serial.print(packetSize);
			Serial.print(F(" From ")); Serial.print(remoteIpNo);
			Serial.print(F(", port ")); Serial.print(remotePortNo);	
			Serial.print(F(", data: "));
			for (int i=0; i<packetSize; i++) {
				Serial.print(buff_down[i],HEX);
				Serial.print(':');
			}
			Serial.println();
		}
	break;
	default:
		Serial.print(F(", ERROR ident not recognized: "));
		Serial.println(ident);
	break;
  }
  
  // For downstream messages, fill the buff_down buffer
  
  return packetSize;
}


// ----------------------------------------------------------------------------
// Send UP an UDP/DGRAM message to the MQTT server
// If we send to more than one host (not sure why) then we need to set sockaddr 
// before sending.
// ----------------------------------------------------------------------------
void sendUdp(uint8_t * msg, int length) {
	int l;
	lastToken = msg[2]*256+msg[1];
	
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println(F("sendUdp: ERROR not connected to WLAN"));
		Udp.flush();

		if (WlanConnect() < 0) {
			Serial.print(F("sendUdp: ERROR connecting to WiFi"));
			yield();
			return;
		}
		if (debug>=1) Serial.println(F("WiFi reconnected"));	
		delay(10);
	}

	//send the update
	
	Udp.beginPacket(ttnServer, (int) _TTNPORT);
	if ((l = Udp.write((char *)msg, length)) != length) {
		Serial.println("sendUdp:: Error write");
	}
	else {
		if (debug>=2) {
			Serial.print(F("sendUdp 1: sent "));
			Serial.print(l);
			Serial.println(F(" bytes"));
		}
	}
	yield();
	Udp.endPacket();
	
#ifdef _THINGSERVER
	delay(1);

	Udp.beginPacket(thingServer, (int) _THINGPORT);
		if ((l = Udp.write((char *)msg, length)) != length) {
		Serial.println("sendUdp:: Error write");
	}
	else {
		if (debug>=2) {
			Serial.print(F("sendUdp 2: sent "));
			Serial.print(l);
			Serial.println(F(" bytes"));
		}
	}
	yield();
	Udp.endPacket();
#endif

	return;
}


// ----------------------------------------------------------------------------
// connect to UDP – returns true if successful or false if not
// ----------------------------------------------------------------------------
bool UDPconnect() {

	bool ret = false;
	unsigned int localPort = _LOCUDPPORT;			// To listen to return messages from WiFi
	if (debug>=1) {
		Serial.print(F("Connecting to UDP port "));
		Serial.println(localPort);
	}
	
	if (Udp.begin(localPort) == 1) {
		if (debug>=1) Serial.println(F("Connection successful"));
		ret = true;
	}
	else{
		//Serial.println("Connection failed");
	}
	return(ret);
}




// ----------------------------------------------------------------------------
// Send UP periodic Pull_DATA message to server to keepalive the connection
// and to invite the server to send downstream messages when available
// *2, par. 5.2
//	- Protocol Version (1 byte)
//	- Random Token (2 bytes)
//	- PULL_DATA identifier (1 byte) = 0x02
//	- Gateway unique identifier (8 bytes) = MAC address
// ----------------------------------------------------------------------------
void pullData() {

    uint8_t pullDataReq[12]; 						// status report as a JSON object
    int pullIndex=0;
	int i;
	
    // pre-fill the data buffer with fixed fields
    pullDataReq[0]  = PROTOCOL_VERSION;						// 0x01
	uint8_t token_h = (uint8_t)rand(); 						// random token
    uint8_t token_l = (uint8_t)rand();						// random token
    pullDataReq[1]  = token_h;
    pullDataReq[2]  = token_l;
    pullDataReq[3]  = PKT_PULL_DATA;						// 0x02
	
	// READ MAC ADDRESS OF ESP8266
    pullDataReq[4]  = MAC_array[0];
    pullDataReq[5]  = MAC_array[1];
    pullDataReq[6]  = MAC_array[2];
    pullDataReq[7]  = 0xFF;
    pullDataReq[8]  = 0xFF;
    pullDataReq[9]  = MAC_array[3];
    pullDataReq[10] = MAC_array[4];
    pullDataReq[11] = MAC_array[5];

    pullIndex = 12;											// 12-byte header
	
    pullDataReq[pullIndex] = 0; 							// add string terminator, for safety

    if (debug>= 2) {
		Serial.print(F("PKT_PULL_DATA request: <"));
		Serial.print(pullIndex);
		Serial.print(F("> "));
		for (i=0; i<pullIndex; i++) {
			Serial.print(pullDataReq[i],HEX);				// DEBUG: display JSON stat
			Serial.print(':');
		}
		Serial.println();
	}
    //send the update
    sendUdp(pullDataReq, pullIndex);
}


// ----------------------------------------------------------------------------
// Send UP periodic status message to server even when we do not receive any
// data. 
// Parameter is socketr to TX to
// ----------------------------------------------------------------------------
void sendstat() {

    uint8_t status_report[STATUS_SIZE]; 					// status report as a JSON object
    char stat_timestamp[32];								// XXX was 24
    time_t t;
	char clat[8]={0};
	char clon[8]={0};

    int stat_index=0;
	
    // pre-fill the data buffer with fixed fields
    status_report[0]  = PROTOCOL_VERSION;					// 0x01
    status_report[3]  = PKT_PUSH_DATA;						// 0x00
	
	// READ MAC ADDRESS OF ESP8266
    status_report[4]  = MAC_array[0];
    status_report[5]  = MAC_array[1];
    status_report[6]  = MAC_array[2];
    status_report[7]  = 0xFF;
    status_report[8]  = 0xFF;
    status_report[9]  = MAC_array[3];
    status_report[10] = MAC_array[4];
    status_report[11] = MAC_array[5];

    uint8_t token_h   = (uint8_t)rand(); 					// random token
    uint8_t token_l   = (uint8_t)rand();					// random token
    status_report[1]  = token_h;
    status_report[2]  = token_l;
    stat_index = 12;										// 12-byte header
	
    t = now();												// get timestamp for statistics
		
	sprintf(stat_timestamp, "%d-%d-%2d %d:%d:%02d CET", year(),month(),day(),hour(),minute(),second());
	yield();
	
	ftoa(lat,clat,4);										// Convert lat to char array with 4 decimals
	ftoa(lon,clon,4);										// As Arduino CANNOT prints floats
	
	// Build the Status message in JSON format, XXX Split this one up...
	delay(1);
	
    int j = snprintf((char *)(status_report + stat_index), STATUS_SIZE-stat_index, 
		"{\"stat\":{\"time\":\"%s\",\"lati\":%s,\"long\":%s,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%u.0,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}", 
		stat_timestamp, clat, clon, (int)alt, cp_nb_rx_rcv, cp_nb_rx_ok, cp_up_pkt_fwd, 0, 0, 0,platform,email,description);
		
	yield();												// Give way to the internal housekeeping of the ESP8266
	if (debug >=1) { delay(1); }
    stat_index += j;
    status_report[stat_index] = 0; 							// add string terminator, for safety

    if (debug>=2) {
		Serial.print(F("stat update: <"));
		Serial.print(stat_index);
		Serial.print(F("> "));
		Serial.println((char *)(status_report+12));			// DEBUG: display JSON stat
	}
	
    //send the update
	// delay(1);
    sendUdp(status_report, stat_index);
	return;
}



// ============================================================================
// MAIN PROGRAM CODE (SETUP AND LOOP)


// ----------------------------------------------------------------------------
// Setup code (one time)
// ----------------------------------------------------------------------------
void setup () {
	Serial.begin(_BAUDRATE);						// As fast as possible for bus

	// The following section can normnally be left OFF. Only for those suffering
	// from exception problems due to am erro in the WiFi code of the ESP8266
#define WIFIBUG 0									// System no recovers from Wifi errors
#if WIFIBUG==1
	uint16_t s,res;
	os_printf("Erasing sectors\n");
	for (s=0x70;s<=0x7F; s++)
	{
		res = spi_flash_erase_sector(s);
		os_printf("Sector erased 0x%02X. Res %d\n",s,res);
	}
#endif

	delay(1500);
	yield();
		
	if (debug>=1) {
		Serial.print(F("! debug: ")); 
		Serial.println(debug);
		yield();
	}
	
	// Setup WiFi UDP connection. Give it some time ..
	while (WlanConnect() < 0) {
		Serial.print(F("Error Wifi network connect "));
		Serial.println();
		yield();
	}
	//else {
		Serial.print(F("Wlan Connected to "));
		Serial.print(WiFi.SSID());
		Serial.println();
		delay(200);
		// If we are here we are connected to WLAN
		// So now test the UDP function
		if (!UDPconnect()) {
			Serial.println(F("Error UDPconnect"));
		}
		delay(500);
	//}
	 
	WiFi.macAddress(MAC_array);
    for (int i = 0; i < sizeof(MAC_array); ++i){
      sprintf(MAC_char,"%s%02x:",MAC_char,MAC_array[i]);
    }
	Serial.print("MAC: ");
    Serial.println(MAC_char);
	
    pinMode(ssPin, OUTPUT);
    pinMode(dio0, INPUT);
    pinMode(RST, OUTPUT);
	
	SPI.begin();
	delay(1000);
    initLoraModem();
	delay(500);
	
	// We choose the Gateway ID to be the Ethernet Address of our Gateway card
    // display results of getting hardware address
	// 
    Serial.print("Gateway ID: ");
    Serial.print(MAC_array[0],HEX);
    Serial.print(MAC_array[1],HEX);
    Serial.print(MAC_array[2],HEX);
	Serial.print(0xFF, HEX);
	Serial.print(0xFF, HEX);
    Serial.print(MAC_array[3],HEX);
    Serial.print(MAC_array[4],HEX);
    Serial.print(MAC_array[5],HEX);

    Serial.print(", Listening at SF");
	Serial.print(sf);
	Serial.print(" on ");
	Serial.print((double)freq/1000000);
	Serial.println(" Mhz.");

	WiFi.hostByName(_TTNSERVER, ttnServer);					// Use DNS to get server IP once
	delay(500);
#if defined(_THINGSERVER)
	WiFi.hostByName(_THINGSERVER, thingServer);
	delay(500);
#endif
	
	setupTime();											// Set NTP time host and interval
	setTime((time_t)getNtpTime());
	Serial.print("Time: "); printTime();
	Serial.println();

#if A_SERVER==1	
	server.on("/", []() {
		webPage = WifiServer("","");
		server.send(200, "text/html", webPage);
	});
	server.on("/HELP", []() {
		webPage = WifiServer("HELP","");
		server.send(200, "text/html", webPage);
	});
	server.on("/RESET", []() {
		webPage = WifiServer("RESET","");
		server.send(200, "text/html", webPage);
	});
	server.on("/DEBUG=0", []() {
		webPage = WifiServer("DEBUG","0");
		server.send(200, "text/html", webPage);
	});
	server.on("/DEBUG=1", []() {
		webPage = WifiServer("DEBUG","1");
		server.send(200, "text/html", webPage);
	});
	server.on("/DEBUG=2", []() {
		webPage = WifiServer("DEBUG","2");
		server.send(200, "text/html", webPage);
	});
	server.on("/DELAY=1", []() {
		webPage = WifiServer("DELAY","1");
		server.send(200, "text/html", webPage);
	});
	server.on("/DELAY=-1", []() {
		webPage = WifiServer("DELAY","-1");
		server.send(200, "text/html", webPage);
	});
	server.begin();											// Start the webserver
	Serial.print(F("Admin Server started on port "));
	Serial.println(SERVERPORT);
#endif	

	Serial.println(F("--------------------------------------"));
	delay(1000);											// Wait after setup
}


// ----------------------------------------------------------------------------
// LOOP
// This is the main program that is executed time and time again.
// We need to give way to the backend WiFi processing that 
// takes place somewhere in the ESP8266 firmware and therefore
// we include yield() statements at important points.
//
// Note: If we spend too much time in user processing functions
//	and the backend system cannot do its housekeeping, the watchdog
// function will be executed which means effectively that the 
// program crashes.
//
// NOTE: For ESP make sure not to do lage array declarations in loop();
// ----------------------------------------------------------------------------
void loop ()
{
	uint32_t nowseconds;
	int buff_index;
	int packetSize;
	
	// Receive Lora messages if there are any
    if ((buff_index = receivePacket(buff_up)) >= 0) {	// read is successful
		yield();
		// rxpk PUSH_DATA received from node is rxpk (*2, par. 3.2)
		sendUdp(buff_up, buff_index);					// We can send to multiple sockets if necessary

	}
	else {
		// No LoRa message received
	}
	
	yield();
	
	// Receive UDP PUSH_ACK messages from server. (*2, par. 3.3)
	// This is important since the TTN broker will return confirmation
	// messages on UDP for every message sent by the gateway. So we have to consume them..
	// As we do not know when the server will respond, we test in every loop.
	//

	while( (packetSize = Udp.parsePacket()) > 0) {		// Length of UDP message waiting
		yield();
		if (readUdp(packetSize, buff_down) < 0) {
			Serial.println(F("readUDP error"));
		}
	}
	
	yield();

	// stat PUSH_DATA message (*2, par. 4)
	//	
	nowseconds = (uint32_t) millis() /1000;
    if (nowseconds - stattime >= _STAT_INTERVAL) {		// Wake up every xx seconds
        sendstat();										// Show the status message and send to server	
		stattime = nowseconds;
    }
	
	yield();
	
	// send PULL_DATA message (*2, par. 4)
	//
	nowseconds = (uint32_t) millis() /1000;
    if (nowseconds - pulltime >= _PULL_INTERVAL) {		// Wake up every xx seconds
        pullData();										// Send PULL_DATA message to server						
		pulltime = nowseconds;
    }

	// Handle the WiFi server part of this sketch. Mainly used for administration of the node
#if A_SERVER==1
	server.handleClient();
#endif	
	
	yield();
}
