/*******************************************************************************
 * Copyright (c) 2016 Maarten Westenberg version for ESP8266
 * Verison 1.1
 * Date: 2016-08-10
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
//#include <Esp.h>
//#include "loraModem.h"
//#include "ESP-sc-gway.h"
// NOTE: Header files contained in the main sketch don't need to be
//		included a second time.



// ============================================================================
// LORA GATEWAY/MODEM FUNCTIONS
//
// The LoRa supporting functions are in the section below

// ----------------------------------------------------------------------------
// The SS (Chip select) pin is used to make sure the RFM95 is selected
// ----------------------------------------------------------------------------
void selectreceiver()
{
    digitalWrite(ssPin, LOW);
}

// ----------------------------------------------------------------------------
// ... or unselected
// ----------------------------------------------------------------------------
void unselectreceiver()
{
    digitalWrite(ssPin, HIGH);
}


// ----------------------------------------------------------------------------
// Read one byte value, par addr is address
// Returns the value of register(addr)
// ----------------------------------------------------------------------------
byte readRegister(byte addr)
{
    selectreceiver();
	SPI.beginTransaction(SPISettings(50000, MSBFIRST, SPI_MODE0));
	SPI.transfer(addr & 0x7F);
	uint8_t res = SPI.transfer(0x00);
	SPI.endTransaction();
    unselectreceiver();
    return res;
}


// ----------------------------------------------------------------------------
// Write value to a register with address addr. 
// Function writes one byte at a time.
// ----------------------------------------------------------------------------
void writeRegister(byte addr, byte value)
{
    unsigned char spibuf[2];

    spibuf[0] = addr | 0x80;
    spibuf[1] = value;
    selectreceiver();
	SPI.beginTransaction(SPISettings(50000, MSBFIRST, SPI_MODE0));
	SPI.transfer(spibuf[0]);
	SPI.transfer(spibuf[1]);
	SPI.endTransaction();
    unselectreceiver();
}


// ----------------------------------------------------------------------------
//  setRate is setting rate etc. for transmission
//		Modem Config 1 (MC1) ==
//		Modem Config 2 (MC2) == (CRC_ON) | (sf<<4)
//		Modem Config 3 (MC3) == 0x04 | (optional SF11/12 LOW DATA OPTIMIZE 0x08)
//		sf == SF7 default 0x07, (SF7<<4) == SX72_MC2_SF7
//		bw == 125 == 0x70
//		cr == CR4/5 == 0x02
//		CRC_ON == 0x04
// ----------------------------------------------------------------------------
void setRate(uint8_t sf, uint8_t crc) {

	uint8_t mc1=0, mc2=0, mc3=0;
	// Set rate based on Spreading Factor etc
    if (sx1272) {
		mc1= 0x0A;				// SX1276_MC1_BW_250 0x80 | SX1276_MC1_CR_4_5 0x02
		mc2= (sf<<4) | crc;
		// SX1276_MC1_BW_250 0x80 | SX1276_MC1_CR_4_5 0x02 | SX1276_MC1_IMPLICIT_HEADER_MODE_ON 0x01
        if (sf == SF11 || sf == SF12) { mc1= 0x0B; }			        
    } 
	else {
	    mc1= 0x72;				// SX1276_MC1_BW_125==0x70 | SX1276_MC1_CR_4_5==0x02
		mc2= (sf<<4) | crc;		// crc is 0x00 or 0x04==SX1276_MC2_RX_PAYLOAD_CRCON
		mc3= 0x04;				// 0x04; SX1276_MC3_AGCAUTO
        if (sf == SF11 || sf == SF12) { mc3|= 0x08; }		// 0x08 | 0x04
    }
	
	// Implicit Header (IH), for class b beacons
	//if (getIh(LMIC.rps)) {
    //   mc1 |= SX1276_MC1_IMPLICIT_HEADER_MODE_ON;
    //    writeRegister(REG_PAYLOAD_LENGTH, getIh(LMIC.rps)); // required length
    //}
		
	writeRegister(REG_MODEM_CONFIG1, mc1);
	writeRegister(REG_MODEM_CONFIG2, mc2);
	writeRegister(REG_MODEM_CONFIG3, mc3);
	
	// Symbol timeout settings
    if (sf == SF10 || sf == SF11 || sf == SF12) {
        writeRegister(REG_SYMB_TIMEOUT_LSB,0x05);
    } else {
        writeRegister(REG_SYMB_TIMEOUT_LSB,0x08);
    }
	
	return;
}


// ----------------------------------------------------------------------------
// Set the frequency for our gateway
// The function has no parameter other than the freq setting used in init.
// Since we are usin a 1ch gateway this value is set fixed.
// ----------------------------------------------------------------------------
void setFreq()
{
	if (debug >= 2) {
		Serial.print(F("setFreq using: "));
		Serial.println(freq);
	}
    // set frequency
    uint64_t frf = ((uint64_t)freq << 19) / 32000000;
    writeRegister(REG_FRF_MSB, (uint8_t)(frf>>16) );
    writeRegister(REG_FRF_MID, (uint8_t)(frf>> 8) );
    writeRegister(REG_FRF_LSB, (uint8_t)(frf>> 0) );
	
	return;
}


// ----------------------------------------------------------------------------
//	Set Power for our gateway
// ----------------------------------------------------------------------------
void setPow(uint8_t powe) {

	if (powe >= 16) powe = 15;
	else if (powe < 2) powe =2;
	
	uint8_t pac = 0x80 | (powe & 0xF);
	writeRegister(REG_PAC,pac);
	writeRegister(REG_PADAC, readRegister(REG_PADAC)|0x4);
	
	// XXX Power settings for CFG_sx1272 are different
	
	return;
}


// ----------------------------------------------------------------------------
// Used to set the radio to LoRa mode (transmitter)
// ----------------------------------------------------------------------------
static void opmodeLora() {
    uint8_t u = OPMODE_LORA;
#ifdef CFG_sx1276_radio
    u |= 0x8;   // TBD: sx1276 high freq
#endif
    writeRegister(REG_OPMODE, u);
}


// ----------------------------------------------------------------------------
// Set the opmode to a value as defined on top
// Values are 0x00 to 0x07
// ----------------------------------------------------------------------------
static void opmode (uint8_t mode) {
    writeRegister(REG_OPMODE, (readRegister(REG_OPMODE) & ~OPMODE_MASK) | mode);
	writeRegister(REG_OPMODE, (readRegister(REG_OPMODE) & ~OPMODE_MASK) | mode);
}


// ----------------------------------------------------------------------------
// This DOWN function sends a payload to the LoRa node over the air
// Radio must go back in standby mode as soon as the transmission is finished
// ----------------------------------------------------------------------------
bool sendPkt(uint8_t *payLoad, uint8_t payLength, uint32_t tmst)
{
	writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_TX_BASE_AD));	// 0x0D, 0x0E
	writeRegister(REG_PAYLOAD_LENGTH, payLength);				// 0x22
	for(int i = 0; i < payLength; i++)
    {
        writeRegister(REG_FIFO, payLoad[i]);					// 0x00
    }
	return true;
}


// ----------------------------------------------------------------------------
// Setup the LoRa receiver on the connected transceiver.
// - Determine the correct transceiver type (sx1272/RFM92 or sx1276/RFM95)
// - Set the frequency to listen to (1-channel remember)
// - Set Spreading Factor (standard SF7)
// The reset RST pin might not be necessary for at least the RGM95 transceiver
//
// 1. Put the radio in LoRa mode
// 2. Put modem in sleep or in standby
// 3. Set Frequency
// ----------------------------------------------------------------------------
void rxLoraModem()
{
	// 1. Put system in LoRa mode
	opmodeLora();
	
	// Put the radio in sleep mode
    opmode(OPMODE_SLEEP);
	
	// 3. Set frequency based on value in freq
	setFreq();

    writeRegister(REG_SYNC_WORD, 0x34); // LoRaWAN public sync word

	// Set spreading Factor
    if (sx1272) {
        if (sf == SF11 || sf == SF12) {
            writeRegister(REG_MODEM_CONFIG1,0x0B);
        } else {
            writeRegister(REG_MODEM_CONFIG1,0x0A);
        }
        writeRegister(REG_MODEM_CONFIG2,(sf<<4) | 0x04);
    } else {
        if (sf == SF11 || sf == SF12) {
            writeRegister(REG_MODEM_CONFIG3,0x0C);				// 0x08 | 0x04
        } else {
            writeRegister(REG_MODEM_CONFIG3,0x04);				// 0x04; SX1276_MC3_LOW_DATA_RATE_OPTIMIZE
        }
        writeRegister(REG_MODEM_CONFIG1,0x72);
        writeRegister(REG_MODEM_CONFIG2,(sf<<4) | 0x04);		// Set mc2 to (SF<<4) | CRC==0x04
    }

    if (sf == SF10 || sf == SF11 || sf == SF12) {
        writeRegister(REG_SYMB_TIMEOUT_LSB,0x05);
    } else {
        writeRegister(REG_SYMB_TIMEOUT_LSB,0x08);
    }
	
	// prevent node to node communication
	writeRegister(REG_INVERTIQ,0x27);							// 0x33, 0x27; to reset from TX
	
	// Max Payload length is dependent on 256byte buffer. At startup TX starts at
	// 0x80 and RX at 0x00. RX therefore maximized at 128 Bytes
    writeRegister(REG_MAX_PAYLOAD_LENGTH,0x80);					// 0x23, 0x80
    writeRegister(REG_PAYLOAD_LENGTH,PAYLOAD_LENGTH);			// 0x22, 0x40; Payload is 64Byte long
    writeRegister(REG_HOP_PERIOD,0x00);							// 0x24, 0x00 was 0xFF
    writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_BASE_AD));	// 0x0D, 0x0F

    // Low Noise Amplifier used in receiver
    writeRegister(REG_LNA, LNA_MAX_GAIN);  						// 0x0C, 0x23
	
	writeRegister(REG_IRQ_FLAGS_MASK, ~IRQ_LORA_RXDONE_MASK);	// Accept no interrupts except RXDONE
	writeRegister(REG_DIO_MAPPING_1, MAP_DIO0_LORA_RXDONE);		// Set RXDONE interrupt to dio0
	
	// Set Continous Receive Mode
    opmode(OPMODE_RX);											// 0x80 | 0x05 (listen)
	
	return;
}

// ----------------------------------------------------------------------------
// loraWait()
// This function implements the wait protocol needed for downstream transmissions.
// Note: Timing of downstream and JoinAccept messages is VERY critical.
//
// As the ESP8266 watchdog will not like us to wait more than a few hundred
// milliseconds (or it will kick in) we have to implement a simple way to wait
// time in case we have to wait seconds before sending messages (e.g. for OTAA 5 or 6 seconds)
// Without it, the system is known to crash in half of the cases it has to wait for 
// JOIN-ACCEPT messages to send.
//
// This function uses a combination of delay() statements and delayMicroseconds().
// As we use delay() only when there is still enough time to wait and we use micros()
// to make sure that delay() did not take too much time this works.
// 
// Parameter: uint32-t tmst gives the micros() value when transmission should start.
// ----------------------------------------------------------------------------
void loraWait(uint32_t tmst) {

	uint32_t startTime = micros();							// Start of the loraWait function
	uint32_t nowTime;										// to prevent micros(0) to creep over 0xFFFFFF
	
	if ((nowTime = micros()) > tmst) {
		// If difference larger than 8 secs, no rollover. Waiting would take too long
		if ((0xFFFFFFFF - nowTime + tmst) > 8000000)
		{
			Serial.print(F("loraWait:: ERROR, micros="));
			Serial.print(nowTime);
			Serial.print(F(", tmst="));
			Serial.print(tmst);
			Serial.print(F(", wait="));
			Serial.println(0xFFFFFFFF - nowTime + tmst);
			delay(100);
			return;
		}
		if (debug >= 1) {
			Serial.print(F("Rollover, micros="));
			Serial.print(nowTime);
			Serial.print(F(", tmst="));
			Serial.print(tmst);
			Serial.print(F(", wait="));
			Serial.println(0xFFFFFFFF - nowTime + tmst);
		}
	}
	else if ((tmst - nowTime) > 8000000) {
		Serial.print(F("Wait ERROR, micros="));
		Serial.print(nowTime);
		Serial.print(F(", tmst="));
		Serial.print(tmst);
		Serial.print(F(", wait="));
		Serial.println(tmst - nowTime);
		return;
	}
	else if (debug >= 2) {
		Serial.print(F("Waiting, micros="));
		Serial.print(nowTime);
		Serial.print(F(", tmst="));
		Serial.print(tmst);
		Serial.print(F(", wait="));
		Serial.println(tmst - nowTime);
		return;
	}
	
	// First deal with the situation of overflow, tmst < micros();
	while ((nowTime = micros()) > tmst) {					// Buffer overflow situation, correct 2^32
		// While we can wait 16000 usecs
		if ((0xFFFFFFFF - nowTime + tmst) > 15000) delay(15);
		else delayMicroseconds(0xFFFFFFFF - nowTime + tmst);	// Last uSec wait if longer times have been waited out.
	}
	
	while ((nowTime = micros()) < tmst) {
		if ((tmst- nowTime) > 15000) delay(14);
		else delayMicroseconds(tmst - nowTime);				// Last uSec wait if longer times have been waited out.
	}
	delayMicroseconds(txDelay);								// extra correction (only positive numbers allowed)
}


// ----------------------------------------------------------------------------
// txLoraModem
// Init the transmitter and transmit the buffer
// After successful transmission (dio0==1) re-init the receiver
//
//	crc is set to 0x00 for TX
//	iiq is set to 0x27 (or 0x40 based on ipol value in txpkt)
//
//	1. opmodeLoRa
//	2. opmode StandBY
//	3. Configure Modem
//	4. Configure Channel
//	5. write PA Ramp
//	6. config Power
//	7. RegLoRaSyncWord LORA_MAC_PREAMBLE
//	8. write REG dio mapping (dio0)
//	9. write REG IRQ flags
// 10. write REG IRQ flags mask
// 11. write REG LoRa Fifo Base Address
// 12. write REG LoRa Fifo Addr Ptr
// 13. write REG LoRa Payload Length
// 14. Write buffer (byte by byte)
// 15. opmode TX
// ----------------------------------------------------------------------------

static void txLoraModem(uint8_t *payLoad, uint8_t payLength, uint32_t tmst,
						uint8_t powe, uint32_t freq, uint8_t crc, uint8_t iiq)
{
	if (debug>=1) {
		Serial.print(F("txLoraModem:: "));
		Serial.print(F("powe: ")); Serial.print(powe);
		Serial.print(F(", freq: ")); Serial.print(freq);
		Serial.print(F(", crc: ")); Serial.print(crc);
		Serial.print(F(", iiq: ")); Serial.print(iiq,HEX);
		Serial.println();
	}
	
	// 1. Select LoRa modem from sleep mode
	opmodeLora();
	
	// Assert the value of the current mode
	ASSERT((readRegister(REG_OPMODE) & OPMODE_LORA) != 0);
	
	// 2. enter standby mode (required for FIFO loading))
	opmode(OPMODE_STANDBY);
	
	// 3. Init spreading factor and other Modem setting
    sf_t sf = _SPREADING;
	setRate(sf, crc);
	
	//writeRegister(REG_HOP_PERIOD, 0x00);						// 0x24 only for receivers
	
	// 4. Init Frequency, config channel
	setFreq();

	// 5. Config PA Ramp up time
	writeRegister(REG_PARAMP, (readRegister(REG_PARAMP) & 0xF0) | 0x08); // set PA ramp-up time 50 uSec
	
	// 6. Set power level, REG_PAC
	setPow(powe);
	
	// prevent node to node communication
	writeRegister(REG_INVERTIQ,iiq);							// 0x33, (0x27 or 0x40)

	// 7. set sync word
    writeRegister(REG_SYNC_WORD, 0x34);							// LORA_MAC_PREAMBLE
	
	// 8. set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP (or lesss for 1ch gateway)
    writeRegister(REG_DIO_MAPPING_1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
	//writeRegister(REG_DIO_MAPPING_1, MAP_DIO0_LORA_TXDONE);
	
	// 9. clear all radio IRQ flags
    writeRegister(REG_IRQ_FLAGS, 0xFF);
	
	// 10. mask all IRQs but TxDone
    writeRegister(REG_IRQ_FLAGS_MASK, ~IRQ_LORA_TXDONE_MASK);
	
	// txLora
	//opmode(OPMODE_FSTX);	// 0x02
	
	// 11, 12, 13, 14. write the buffer to the FiFo
	sendPkt(payLoad, payLength, tmst);

	// wait extra delay out. The delayMicroseconds timer is accurate until 16383 uSec.
	//												// XXX We should not use yield() outside loop()
	uint32_t startTime = micros();
	loraWait(tmst);
	
	// 15. Initiate actual transmission of FiFo
	opmode(OPMODE_TX);
	
	yield();
	if (debug >=1) { 
		Serial.print(F("start: ")); 
		Serial.print(startTime);
		Serial.print(F(", end: "));
		Serial.print(tmst);
		Serial.print(F(", waited: "));
		Serial.print(tmst - startTime);
		Serial.print(F(", delay="));
		Serial.print(txDelay);
		Serial.println();
	}
	
	// XXX Intead of handling the interrupt of dio0, we wait it out, Not using delay(1);
	// for trasmitter this should not be a problem.
	while(digitalRead(dio0) == 0) {  }						// XXX tx done? handle by interrupt

	// ----- TX SUCCESS, SWITCH BACK TO RX CONTINUOUS --------
	// Successful TX cycle put's radio in standby mode.
	
	// Reset the IRQ register
	writeRegister(REG_IRQ_FLAGS, 0xFF);

	// Give control back to continuous receive setup
	// Put's the radio in sleep mode and then in stand-by
	rxLoraModem();								
}

// ----------------------------------------------------------------------------
// First time initialisation of the LoRa modem
// Subsequent changes to the modem state etc. done by txLoraModem or rxLoraModem
// After initialisation the modem is put in rxContinuous mode (listen)
// ----------------------------------------------------------------------------
static void initLoraModem()
{
	opmode(OPMODE_SLEEP);
	
    digitalWrite(RST, HIGH);
    delay(100);
    digitalWrite(RST, LOW);
    delay(100);
	
    byte version = readRegister(REG_VERSION);					// Read the LoRa chip version id
    if (version == 0x22) {
        // sx1272
        Serial.println(F("WARNING:: SX1272 detected"));
        sx1272 = true;
    } else {
        // sx1276?
        digitalWrite(RST, LOW);
        delay(100);
        digitalWrite(RST, HIGH);
        delay(100);
        version = readRegister(REG_VERSION);
        if (version == 0x12) {
            // sx1276
            if (debug >=1) Serial.println(F("SX1276 detected, starting."));
            sx1272 = false;
        } else {
            Serial.print(F("Unrecognized transceiver, version: "));
            Serial.println(version,HEX);
            die("");
        }
    }
	
	// Set the radio in Continuous listen mode
	rxLoraModem();
	if (debug >= 1) Serial.println(F("initLoraModem done"));
}


// ----------------------------------------------------------------------------
// This LoRa function reads a message from the LoRa transceiver
// returns true when message correctly received or fails on error 
// (CRC error for example).
// UP function
// ----------------------------------------------------------------------------
bool receivePkt(uint8_t *payload)
{
    // clear rxDone
    writeRegister(REG_IRQ_FLAGS, 0x40);						// 0x12; Clear RxDone
	
    int irqflags = readRegister(REG_IRQ_FLAGS);				// 0x12

    cp_nb_rx_rcv++;											// Receive statistics counter

    //  payload crc=0x20 set
    if((irqflags & 0x20) == 0x20)
    {
        Serial.println(F("CRC error"));
        writeRegister(REG_IRQ_FLAGS, 0x20);					// 0x12
        return false;
    } else {

        cp_nb_rx_ok++;										// Receive OK statistics counter

        byte currentAddr = readRegister(REG_FIFO_RX_CURRENT_ADDR);	// 0x10
        byte receivedCount = readRegister(REG_RX_NB_BYTES);	// 0x13; How many bytes were read
        receivedbytes = receivedCount;

        //writeRegister(REG_FIFO_ADDR_PTR, currentAddr);	// 0x0D XXX??? This sets the FiFo higher!!!

        for(int i = 0; i < receivedCount; i++)
        {
            payload[i] = readRegister(REG_FIFO);			// 0x00
        }
		//yield();
    }
    return true;
}



// ----------------------------------------------------------------------------
// Send DOWN a LoRa packet over the air to the node. This function does all the 
// decoding of the server message and prepares a Payload buffer.
// The payload is actually transmitted by the sendPkt() function.
// This function is used for regular downstream messages and for JOIN_ACCEPT
// messages.
// ----------------------------------------------------------------------------
int sendPacket(uint8_t *buff_down, uint8_t length) {

	// Received package with Meta Data:
	// codr	: "4/5"
	// data	: "Kuc5CSwJ7/a5JgPHrP29X9K6kf/Vs5kU6g=="	// for example
	// freq	: 868.1 									// 868100000
	// ipol	: true/false
	// modu : "LORA"
	// powe	: 14										// Set by default
	// rfch : 0											// Set by default
	// size	: 21
	// tmst : 1800642 									// for example
	// datr	: "SF7BW125"
	
	if (debug >= 2) Serial.println(F("sendPacket called"));
	
	// Trx Time received the message
    uint32_t trx = (uint32_t) micros();
	
	// 12-byte header;
	//		HDR (1 byte)
	//		
	//
	// Data Reply for JOIN_ACCEPT as sent by server:
	//		AppNonce (3 byte)
	//		NetID (3 byte)
	//		DevAddr (4 byte) [ 31..25]:NwkID , [24..0]:NwkAddr
 	//		DLSettings (1 byte)
	//		RxDelay (1byte)
	//		CFList (fill to 16 bytes)
	
	int i=0;
	StaticJsonBuffer<256> jsonBuffer;
	char * bufPtr = (char *) (buff_down);
	buff_down[length] = 0;
	
	if (debug >= 2) Serial.println((char *)buff_down);
	
	// Use JSON to decode the string after the first 4 bytes.
	// The data for the node is in the "data" field. This function destroys original buffer
	JsonObject& root = jsonBuffer.parseObject(bufPtr);
		
	if (!root.success()) {
		Serial.print (F("sendPacket:: ERROR Json Decode "));
		if (debug>=2) {
			Serial.print(": ");
			Serial.println(bufPtr);
		}
		return(-1);
	}
	delay(1);
	// Meta Data sent by server (example)
	// {"txpk":{"codr":"4/5","data":"YCkEAgIABQABGmIwYX/kSn4Y","freq":868.1,"ipol":true,"modu":"LORA","powe":14,"rfch":0,"size":18,"tmst":1890991792,"datr":"SF7BW125"}}

	// Used in the protocol:
	const char * data = root["txpk"]["data"];
	uint8_t psize = root["txpk"]["size"];
	bool ipol = root["txpk"]["ipol"];
	uint8_t powe = root["txpk"]["powe"];
	uint32_t tmst = (uint32_t) root["txpk"]["tmst"].as<unsigned long>();

	
	// Not used in the protocol:
	const char * datr = root["txpk"]["datr"];
	const double ff= root["txpk"]["freq"];
	const char * modu = root["txpk"]["modu"];
	//if (root["txpk"].containsKey("imme") ) {
	//	const bool imme = root["txpk"]["imme"];			// Immediate Transmit (tmst don't care)
	//}
	const uint32_t fff = (uint32_t)(ff*1000000);
		
	if (data != NULL) {
		if (debug>=2) { Serial.print(F("data: ")); Serial.println((char *) data); }
	}
	else {
		Serial.println(F("sendPacket:: data is NULL"));
		return(-1);
	}
	
	uint8_t iiq = (ipol? 0x40: 0x27);					// if ipol==true 0x40 else 0x27
	uint8_t crc = 0x00;									// switch CRC off for TX
	uint8_t payLength = base64_dec_len((char *) data, strlen(data));
	uint8_t payLoad[payLength];
	base64_decode((char *) payLoad, (char *) data, strlen(data));

	txLoraModem(payLoad, payLength, tmst, powe, freq, crc, iiq);
	
	if ((debug >= 2) && (fff != freq)) {
		Serial.print(F("sendPacket:: WARNING used freq="));
		Serial.print(freq);
		Serial.print(F(", freq req="));
		Serial.println(fff);
	}
	
	if (payLength != psize) {
		Serial.print(F("sendPacket:: WARNING payLength: "));
		Serial.print(payLength);
		Serial.print(F(", psize="));
		Serial.println(psize);
	}
	else if (debug >= 2 ) {
		for (i=0; i<payLength; i++) {Serial.print(payLoad[i],HEX); Serial.print(':'); }
		Serial.println();
	}

	cp_up_pkt_fwd++;
	
	return 1;
}


// ----------------------------------------------------------------------------
// Receive a LoRa package over the air
//
// Receive a LoRa message and fill the buff_up char buffer.
// returns values:
// - returns the length of string returned in buff_up
// - returns -1 when no message arrived.
// ----------------------------------------------------------------------------
int receivePacket(uint8_t * buff_up) {

    // long int SNR;
	long SNR;
    int rssicorr;
	char cfreq[12] = {0};										// Character array to hold freq in MHz

	// delay(1);
	
	// Regular message received, see SX1276 spec table 18
	// Next statement could also be a "while" to combine several messages received in one UDP message
	// The Semtech Gateway spec does allow this.
    if(digitalRead(dio0) == 1)									// READY?
    {
		// Take the timestamp as soon as possible, to have accurate recepion timestamp
		// TODO: tmst can jump if micros() overflow.
		uint32_t tmst = (uint32_t) micros();				// Only microseconds, rollover in 
		lastTmst = tmst;									// MMMM according to spec
		
		if (debug >= 2) Serial.println(F("receivePacket:: LoRa message ready"));
		
		// Handle the physical data read from FiFo 
        if(receivePkt(message)) {
			
            byte value = readRegister(REG_PKT_SNR_VALUE);		// 0x19; 
            if( value & 0x80 ) // The SNR sign bit is 1
            {
                // Invert and divide by 4
                value = ( ( ~value + 1 ) & 0xFF ) >> 2;
                SNR = -value;
            }
            else
            {
                // Divide by 4
                SNR = ( value & 0xFF ) >> 2;
            }
            
            if (sx1272) {
                rssicorr = 139;
            } else {											// Probably SX1276 or RFM95
                rssicorr = 157;
            }
			
			if (debug>=1) {
			    Serial.print(F("Packet RSSI: "));
				Serial.print(readRegister(0x1A)-rssicorr);
				Serial.print(F(" RSSI: "));
				Serial.print(readRegister(0x1B)-rssicorr);
				Serial.print(F(" SNR: "));
				Serial.print(SNR);
				Serial.print(F(" Length: "));
				Serial.print((int)receivedbytes);
				Serial.print(F(" -> "));
				int i;
				for (i=0; i< receivedbytes; i++) {
					Serial.print(message[i],HEX);
					Serial.print(' ');
				}
				Serial.println();
				yield();
			}
			
            int j;
			// XXX Base64 library is nopad. So we may have to add padding characters until
			// 	length is multiple of 4!
			int encodedLen = base64_enc_len(receivedbytes);		// max 341
			base64_encode(b64, (char *) message, receivedbytes);// max 341

            int buff_index=0;

            // pre-fill the data buffer with fixed fields
            buff_up[0] = PROTOCOL_VERSION;						// 0x01 still
            buff_up[3] = PKT_PUSH_DATA;							// 0x00

			// READ MAC ADDRESS OF ESP8266, and insert 0xFF 0xFF in the middle
            buff_up[4]  = MAC_array[0];
            buff_up[5]  = MAC_array[1];
            buff_up[6]  = MAC_array[2];
            buff_up[7]  = 0xFF;
            buff_up[8]  = 0xFF;
            buff_up[9]  = MAC_array[3];
            buff_up[10] = MAC_array[4];
            buff_up[11] = MAC_array[5];

            // start composing datagram with the header 
            uint8_t token_h = (uint8_t)rand(); 					// random token
            uint8_t token_l = (uint8_t)rand(); 					// random token
            buff_up[1] = token_h;
            buff_up[2] = token_l;
            buff_index = 12; 									// 12-byte header

            // start of JSON structure that will make payload
            memcpy((void *)(buff_up + buff_index), (void *)"{\"rxpk\":[", 9);
            buff_index += 9;
            buff_up[buff_index] = '{';
            ++buff_index;
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, "\"tmst\":%u", tmst);
            buff_index += j;
			ftoa((double)freq/1000000,cfreq,6);					// XXX This can be done better
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"chan\":%1u,\"rfch\":%1u,\"freq\":%s", 0, 0, cfreq);
            buff_index += j;
            memcpy((void *)(buff_up + buff_index), (void *)",\"stat\":1", 9);
            buff_index += 9;
            memcpy((void *)(buff_up + buff_index), (void *)",\"modu\":\"LORA\"", 14);
            buff_index += 14;
            /* Lora datarate & bandwidth, 16-19 useful chars */
            switch (sf) {
            case SF7:
                memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF7", 12);
                buff_index += 12;
                break;
            case SF8:
                memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF8", 12);
                buff_index += 12;
                break;
            case SF9:
                memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF9", 12);
                buff_index += 12;
                break;
            case SF10:
                memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF10", 13);
                buff_index += 13;
                break;
            case SF11:
                memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF11", 13);
                buff_index += 13;
                break;
            case SF12:
                memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF12", 13);
                buff_index += 13;
                break;
            default:
                memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF?", 12);
                buff_index += 12;
            }
            memcpy((void *)(buff_up + buff_index), (void *)"BW125\"", 6);
            buff_index += 6;
            memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"4/5\"", 13);
            buff_index += 13;
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"lsnr\":%li", SNR);
            buff_index += j;
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"rssi\":%d,\"size\":%u", readRegister(0x1A)-rssicorr, receivedbytes);
            buff_index += j;
            memcpy((void *)(buff_up + buff_index), (void *)",\"data\":\"", 9);
            buff_index += 9;

			// Use gBase64 library
			encodedLen = base64_enc_len(receivedbytes);		// max 341
			j = base64_encode((char *)(buff_up + buff_index), (char *) message, receivedbytes);

            buff_index += j;
            buff_up[buff_index] = '"';
            ++buff_index;

            // End of packet serialization
            buff_up[buff_index] = '}';
            ++buff_index;
            buff_up[buff_index] = ']';
            ++buff_index;
            // end of JSON datagram payload */
            buff_up[buff_index] = '}';
            ++buff_index;
            buff_up[buff_index] = 0; 						// add string terminator, for safety

			if (debug>=1) {
				Serial.print(F("RXPK:: "));
				Serial.println((char *)(buff_up + 12));		// DEBUG: display JSON payload
			}
            
			return(buff_index);
			
        } // received a message
    } // dio0=1
	// else not ready for receive
	
	return(-1);
}


