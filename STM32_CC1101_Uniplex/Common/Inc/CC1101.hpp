/**
 * @file CC1101.hpp
 * @brief Minimal CC1101 driver interface for TX‑only operation.
 *
 * This header exposes a compact API for:
 *   - Initializing the CC1101 transceiver
 *   - Issuing strobe commands (SRES, STX, SIDLE, SFTX, etc.)
 *   - Reading/writing single registers
 *   - Performing burst FIFO transfers
 *
 * The implementation (CC1101.cpp) configures the radio for:
 *   - 433.92 MHz center frequency
 *   - 38.4 kbps 2‑FSK
 *   - Whitening + CRC
 *   - Variable‑length packets (max payload = 61 bytes)
 *
 * Only TX functionality is implemented. RX support can be added later.
 */

#pragma once

#include <cstdint>
#include <cstddef>

constexpr uint8_t TXFIFO_UNDERFLOW = 0x80;
constexpr uint8_t RXFIFO_OVERFLOW = 0x80;
constexpr uint8_t CRC_OK = 0x80;

enum class CC1101_ConfigurationRegister : uint8_t
{
	IOCFG2 = 0x00,	 // GDO2 output pin configuration
	IOCFG1 = 0x01,	 // GDO1 output pin configuration
	IOCFG0 = 0x02,	 // GDO0 output pin configuration
	FIFOTHR = 0x03,	 // RX FIFO and TX FIFO thresholds
	SYNC1 = 0x04,	 // Sync word, high byte
	SYNC0 = 0x05,	 // Sync word, low byte
	PKTLEN = 0x06,	 // Packet length
	PKTCTRL1 = 0x07, // Packet automation control
	PKTCTRL0 = 0x08, // Packet automation control
	ADDR = 0x09,	 // Device address
	CHANNR = 0x0A,	 // Channel number
	FSCTRL1 = 0x0B,	 // Frequency synthesizer control
	FSCTRL0 = 0x0C,	 // Frequency synthesizer control
	FREQ2 = 0x0D,	 // Frequency control word, high byte
	FREQ1 = 0x0E,	 // Frequency control word, middle byte
	FREQ0 = 0x0F,	 // Frequency control word, low byte
	MDMCFG4 = 0x10,	 // Modem configuration
	MDMCFG3 = 0x11,	 // Modem configuration
	MDMCFG2 = 0x12,	 // Modem configuration
	MDMCFG1 = 0x13,	 // Modem configuration
	MDMCFG0 = 0x14,	 // Modem configuration
	DEVIATN = 0x15,	 // Modem deviation setting
	MCSM2 = 0x16,	 // Main Radio Control State Machine configuration
	MCSM1 = 0x17,	 // Main Radio Control State Machine configuration
	MCSM0 = 0x18,	 // Main Radio Control State Machine configuration
	FOCCFG = 0x19,	 // Frequency Offset Compensation configuration
	BSCFG = 0x1A,	 // Bit Synchronization configuration
	AGCTRL2 = 0x1B,	 // AGC control
	AGCTRL1 = 0x1C,	 // AGC control
	AGCTRL0 = 0x1D,	 // AGC control
	WOREVT1 = 0x1E,	 // High byte Event 0 timeout
	WOREVT0 = 0x1F,	 // Low byte Event 0 timeout
	WORCTRL = 0x20,	 // Wake On Radio control
	FREND1 = 0x21,	 // Front end RX configuration
	FREND0 = 0x22,	 // Front end TX configuration
	FSCAL3 = 0x23,	 // Frequency synthesizer calibration
	FSCAL2 = 0x24,	 // Frequency synthesizer calibration
	FSCAL1 = 0x25,	 // Frequency synthesizer calibration
	FSCAL0 = 0x26,	 // Frequency synthesizer calibration
	RCCTRL1 = 0x27,	 // RC oscillator configuration
	RCCTRL0 = 0x28,	 // RC oscillator configuration
	FSTEST = 0x29,	 // Frequency synthesizer calibration control
	PTEST = 0x2A,	 // Production test
	AGCTEST = 0x2B,	 // AGC test
	TEST2 = 0x2C,	 // Various test settings
	TEST1 = 0x2D,	 // Various test settings
	TEST0 = 0x2E,	 // Various test settings
};

enum class CC1101_StrobeCommand : uint8_t
{
	SRES = 0x30,	// Reset chip
	SFSTXON = 0x31, // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1)
					// If in RX (with CCA):
					// Go to a wait state where only the synthesizer is running (for quick RX / TX turnaround)
	SXOFF = 0x32,   // Turn off crystal oscillator
	SCAL = 0x33,    // Calibrate frequency synthesizer and turn it off
				    // SCAL can be strobed from IDLE mode
				    // without setting manual calibration mode (MCSM0.FS_AUTOCAL=0)
	SRX = 0x34,     // Enable RX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1
	STX = 0x35,     // In IDLE state: Enable TX. Perform calibration first if MCSM0.FS_AUTOCAL=1
				    // If in RX state and CCA is enabled: Only go to TX if channel is clear
	SIDLE = 0x36,	// Exit RX / TX, turn off frequency synthesizer and exit Wake-On-Radio mode if applicable
	SWOR = 0x38,	// Start automatic RX polling sequence (Wake-on-Radio) if WORCTRL.RC_PD=0
	SPWD = 0x39,	// Enter power down mode when CSn goes high
	SFRX = 0x3A,	// Flush the RX FIFO buffer. Only issue SFRX in IDLE or RXFIFO_OVERFLOW states
	SFTX = 0x3B,	// Flush the TX FIFO buffer. Only issue SFTX in IDLE or TXFIFO_UNDERFLOW states
	SWORRST = 0x3C, // Reset real time clock to Event1 value
	SNOP = 0x3D,	// No operation. May be used to get access to the chip status byte
};

enum class CC1101_StatusRegister : uint8_t
{
	PARTNUM = 0x30,		   // Part number for CC1101
	VERSION = 0x31,		   // Current version number
	FREQEST = 0x32,		   // Frequency Offset Estimate
	LQI = 0x33,			   // Demodulator estimate for Link Quality
	RSSI = 0x34,		   // Received signal strength indication
	MARCSTATE = 0x35,	   // Control state machine state
	WORTIME1 = 0x36,	   // High byte of WOR timer
	WORTIME0 = 0x37,	   // Low byte of WOR timer
	PKTSTATUS = 0x38,	   // Current GDOx status and packet status 94
	VCO_VC_DAC = 0x39,	   // Current setting from PLL calibration module
	TXBYTES = 0x3A,		   // Underflow and number of bytes in the TX FIFO
	RXBYTES = 0x3B,		   // Overflow and number of bytes in the RX FIFO
	RCCTRL1_STATUS = 0x3C, // Last RC oscillator calibration result
	RCCTRL0_STATUS = 0x3D, // Last RC oscillator calibration result
};

enum class CC1101_BurstRegister : uint8_t
{
	PATABLE = 0x3E, // Power Amplifier table address
	TXFIFO = 0x3F,  // TX FIFO address
	RXFIFO = 0x3F,  // RX FIFO address (same as TXFIFO, but with read bit set)
};

enum class CC1101_MARCState : uint8_t
{
	SLEEP = 0x00,
	IDLE = 0x01,
	XOFF = 0x02,
	VCOON_MC = 0x03,
	REGON_MC = 0x04,
	MANCAL = 0x05,
	VCOON = 0x06,
	REGON = 0x07,
	STARTCAL = 0x08,
	BWBOOST = 0x09,
	FS_LOCK = 0x0A,
	IFADCON = 0x0B,
	ENDCAL = 0x0C,
	RX = 0x0D,
	RX_END = 0x0E,
	RX_RST = 0x0F,
	TXRX_SWITCH = 0x10,
	RXFIFO_OVERFLOW = 0x11,
	FSTXON = 0x12,
	TX = 0x13,
	TX_END = 0x14,
	RXTX_SWITCH = 0x15,
	TXFIFO_UNDERFLOW = 0x16,
};

typedef struct {
    CC1101_ConfigurationRegister addr;
    uint8_t value;
} CC1101_RegisterValueTuple;

/**
 * @brief Initialize the CC1101 transceiver.
 *
 * Performs:
 *   - TI‑specified reset sequence
 *   - Full register configuration (modem, synthesizer, AGC, packet engine)
 *   - FIFO flush
 *
 * Must be called once at startup before any RF operations.
 */
void CC1101_Init();

void CC1101_Configure(const CC1101_RegisterValueTuple *tuples, size_t num_tuples);

/**
 * @brief Send a CC1101 strobe command.
 *
 * @param strobe  Strobe command byte.
 * @return Status byte returned by the CC1101.
 */
uint8_t CC1101_Strobe(CC1101_StrobeCommand strobe);

/**
 * @brief Write a single CC1101 register.
 *
 * @param addr   Register address (0x00–0x2E).
 * @param value  Value to write.
 */
void CC1101_WriteConfiguration(CC1101_ConfigurationRegister, uint8_t value);

/**
 * @brief Read a single CC1101 register.
 *
 * @param addr   Register address (0x00–0x2E).
 * @return Register value.
 */
uint8_t CC1101_ReadConfiguration(CC1101_ConfigurationRegister addr);

/**
 * @brief Read a single CC1101 status register.
 *
 * @param addr   Register address (0x30-0x3D).
 * @return Register value.
 */
uint8_t CC1101_ReadStatus(CC1101_StatusRegister addr);

/**
 * @brief Write multiple bytes to a CC1101 FIFO or burst‑capable register.
 *
 * Automatically sets the burst‑write bit (0x40).
 *
 * @param addr   Base address (e.g., 0x3F for TX FIFO).
 * @param data   Pointer to data buffer.
 * @param len    Number of bytes to write (clamped to 63).
 */
void CC1101_WriteBurst(CC1101_BurstRegister addr, const uint8_t *data, uint8_t len);

/**
 * @brief Read multiple bytes from a CC1101 FIFO or burst‑capable register.
 *
 * Automatically sets the burst‑read bit (0xC0).
 *
 * @param addr   Base address (e.g., 0x3F for RX FIFO).
 * @param data   Output buffer.
 * @param len    Number of bytes to read.
 */
void CC1101_ReadBurst(CC1101_BurstRegister addr, uint8_t *data, uint8_t len);
