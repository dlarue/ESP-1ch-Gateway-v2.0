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



// Frequencies
// Set center frequency. If in doubt, choose the first one, comment all others
// Each "real" gateway should support the first 3 frequencies according to LoRa spec.
uint32_t  freq = 868100000; 					// Channel 0, 868.1 MHz
//uint32_t  freq = 868300000; 					// Channel 1, 868.3 MHz
//uint32_t  freq = 868500000; 					// in Mhz! (868.5)
//uint32_t  freq = 867100000; 					// in Mhz! (867.1)
//uint32_t  freq = 867300000; 					// in Mhz! (867.3)
//uint32_t  freq = 867500000; 					// in Mhz! (867.5)
//uint32_t  freq = 867700000; 					// in Mhz! (867.7)
//uint32_t  freq = 867900000; 					// in Mhz! (867.9)
//uint32_t  freq = 868800000; 					// in Mhz! (868.8)
//uint32_t  freq = 869525000; 					// in Mhz! (869.525)
// TTN defines an additional channel at 869.525Mhz using SF9 for class B. Not used


// ============================================================================
// Set all definitions for Gateway
// ============================================================================	

#define REG_FIFO                    0x00
#define REG_OPMODE                  0x01
#define REG_PAC                     0x09
#define REG_PARAMP                  0x0A
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_AD         0x0E
#define REG_FIFO_RX_BASE_AD         0x0F

#define REG_FIFO_RX_CURRENT_ADDR    0x10
#define REG_IRQ_FLAGS_MASK          0x11
#define REG_IRQ_FLAGS               0x12
#define REG_RX_NB_BYTES             0x13
#define REG_PKT_SNR_VALUE			0x19
#define REG_MODEM_CONFIG1           0x1D
#define REG_MODEM_CONFIG2           0x1E
#define REG_SYMB_TIMEOUT_LSB  		0x1F

#define REG_PAYLOAD_LENGTH          0x22
#define REG_MAX_PAYLOAD_LENGTH 		0x23
#define REG_HOP_PERIOD              0x24
#define REG_MODEM_CONFIG3           0x26

#define REG_INVERTIQ				0x33
#define REG_SYNC_WORD				0x39

#define REG_DIO_MAPPING_1           0x40
#define REG_DIO_MAPPING_2           0x41
#define REG_VERSION	  				0x42

#define REG_PADAC					0x5A

// ----------------------------------------
// opModes
#define SX72_MODE_SLEEP             0x80
#define SX72_MODE_STANDBY           0x81
#define SX72_MODE_FSTX              0x82
#define SX72_MODE_TX                0x83	// 0x80 | 0x03
#define SX72_MODE_RX_CONTINUOS      0x85

// ----------------------------------------
// LMIC Constants for radio registers
#define OPMODE_LORA      			0x80
#define OPMODE_MASK      			0x07
#define OPMODE_SLEEP     			0x00
#define OPMODE_STANDBY   			0x01
#define OPMODE_FSTX      			0x02
#define OPMODE_TX        			0x03
#define OPMODE_FSRX      			0x04
#define OPMODE_RX        			0x05
#define OPMODE_RX_SINGLE 			0x06
#define OPMODE_CAD       			0x07

#define PAYLOAD_LENGTH              0x40

// ----------------------------------------
// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23
#define LNA_OFF_GAIN                0x00
#define LNA_LOW_GAIN		    	0x20

// CONF REG
#define REG1                        0x0A
#define REG2                        0x84

// ----------------------------------------
// MC2 definitions
#define SX72_MC2_FSK                0x00
#define SX72_MC2_SF7                0x70		// SF7 == 0x07, so (SF7<<4) == SX7_MC2_SF7
#define SX72_MC2_SF8                0x80
#define SX72_MC2_SF9                0x90
#define SX72_MC2_SF10               0xA0
#define SX72_MC2_SF11               0xB0
#define SX72_MC2_SF12               0xC0

// ----------------------------------------
// MC1 sx1276 RegModemConfig1
#define SX1276_MC1_BW_125                   0x70
#define SX1276_MC1_BW_250                   0x80
#define SX1276_MC1_BW_500                   0x90
#define SX1276_MC1_CR_4_5                   0x02
#define SX1276_MC1_CR_4_6                   0x04
#define SX1276_MC1_CR_4_7                   0x06
#define SX1276_MC1_CR_4_8                   0x08
#define SX1276_MC1_IMPLICIT_HEADER_MODE_ON  0x01

#define SX72_MC1_LOW_DATA_RATE_OPTIMIZE     0x01 	// mandated for SF11 and SF12

// ----------------------------------------
// mc3
#define SX1276_MC3_LOW_DATA_RATE_OPTIMIZE  0x08
#define SX1276_MC3_AGCAUTO                 0x04

// ----------------------------------------
// FRF
#define REG_FRF_MSB					0x06
#define REG_FRF_MID					0x07
#define REG_FRF_LSB					0x08

#define FRF_MSB						0xD9		// 868.1 Mhz
#define FRF_MID						0x06
#define FRF_LSB						0x66

// ----------------------------------------
// DIO function mappings                D0D1D2D3
#define MAP_DIO0_LORA_RXDONE   0x00  // 00------
#define MAP_DIO0_LORA_TXDONE   0x40  // 01------
#define MAP_DIO1_LORA_RXTOUT   0x00  // --00----
#define MAP_DIO1_LORA_NOP      0x30  // --11----
#define MAP_DIO2_LORA_NOP      0xC0  // ----11--

#define MAP_DIO0_FSK_READY     0x00  // 00------ (packet sent / payload ready)
#define MAP_DIO1_FSK_NOP       0x30  // --11----
#define MAP_DIO2_FSK_TXNOP     0x04  // ----01--
#define MAP_DIO2_FSK_TIMEOUT   0x08  // ----10--

// ----------------------------------------
// Bits masking the corresponding IRQs from the radio
#define IRQ_LORA_RXTOUT_MASK 0x80
#define IRQ_LORA_RXDONE_MASK 0x40
#define IRQ_LORA_CRCERR_MASK 0x20
#define IRQ_LORA_HEADER_MASK 0x10
#define IRQ_LORA_TXDONE_MASK 0x08
#define IRQ_LORA_CDDONE_MASK 0x04
#define IRQ_LORA_FHSSCH_MASK 0x02
#define IRQ_LORA_CDDETD_MASK 0x01


#define PROTOCOL_VERSION  1
#define PKT_PUSH_DATA 0
#define PKT_PUSH_ACK  1
#define PKT_PULL_DATA 2
#define PKT_PULL_RESP 3
#define PKT_PULL_ACK  4