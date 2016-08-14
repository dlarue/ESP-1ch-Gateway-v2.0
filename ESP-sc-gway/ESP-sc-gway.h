//
// Author: Maarten Westenberg
// Version: 1.0.1
// Date: 2016-07-25
//
// This file contains a number of compile-time settings that can be set on (=1) or off (=0)
// The disadvantage of compile time is minor compared to the memory gain of not having
// too much code compiled and loaded on your ESP8266.
//
// ----------------------------------------------------------------------------------------


#define STATISTICS 1			// Gather statistics on sensor and Wifi status
#define DEBUG 1					// Initial value of debug var. Can be hanged using the admin webserver
								// For operational use, set initial DEBUG vaulue 0

#define CFG_sx1276_radio		// Define the correct radio type that you are using
//#define CFG_sx1272_radio
					
// Wifi definitions
// Array with SSID and password records. Set WPA size to number of entries in array
#define WPASIZE 6
static char *wpa[WPASIZE][2] = {
	{ "wifi1", "wifi1"},
	{ "wifi2", "wifi2"},
	{ "wifi3", "wifi3"},
	{ "wifi4", "wifi4"},
	{ "wifi5", "wifi5"},
	{ "wifi6", "wifi6"}
};

// Set the local Gateway settings
#define _SPREADING SF7							// We receive and sent on this Spreading Factor (only)
#define _LOCUDPPORT 1700						// Often 1701 is used for upstream comms

#define _PULL_INTERVAL 31						// PULL_DATA messages to server to get downstream
#define _STAT_INTERVAL 61						// Send a 'stat' message to server
#define _NTP_INTERVAL 3600						// How often doe we want time NTP synchronization

// MQTT definitions
#define _TTNPORT 1700
//#define _TTNSERVER "router.eu.staging.thethings.network"
#define _TTNSERVER "router.eu.thethings.network"

// Port is UDP port in this program
//#define _THINGPORT 1701							// Your own UDP server
//#define _THINGSERVER "yourserver.com"			// Server URL of the server

// Gateway Ident definitions
#define _DESCRIPTION "ESP 1ch Gateway"
#define _EMAIL "mw12554@hotmail.com"
#define _PLATFORM "ESP8266"
#define _LAT 0.00
#define _LON 0.00
#define _ALT 0

								
// Definitions for the admin webserver
#define A_SERVER 1				// Define local WebServer only if this define is set
#define SERVERPORT 8080			// local webserver port

#define A_MAXBUFSIZE 192		// Must be larger than 128, but small enough to work
#define _BAUDRATE 115200		// Works for debug messages to serial momitor (if attached).

// ntp
#define NTP_TIMESERVER "nl.pool.ntp.org"	// Country and region specific
#define NTP_TIMEZONES	2		// How far is our Timezone from UTC (excl daylight saving/summer time)
#define SECS_PER_HOUR	3600

#if !defined(CFG_noassert)
#define ASSERT(cond) if(!(cond)) gway_failed(__FILE__, __LINE__)
#else
#define ASSERT(cond) /**/
#endif