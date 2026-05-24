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
 */

#pragma once

#include <cstdint>
#include <cstddef>

#include "main.h"
#include "stm32f4xx_hal.h"

template <typename GPIO>
class CC1101
{
public:
	// -------------------------------------------------------------------------
	// Nested Types and Enums
	// -------------------------------------------------------------------------

	enum class ConfigurationRegister : uint8_t
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

		PATABLE = 0x3E, // TX Power
	};

	enum class StrobeCommand : uint8_t
	{
		SRES = 0x30,	// Reset chip
		SFSTXON = 0x31, // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1)
						// If in RX (with CCA):
						// Go to a wait state where only the synthesizer is running (for quick RX / TX turnaround)
		SXOFF = 0x32,	// Turn off crystal oscillator
		SCAL = 0x33,	// Calibrate frequency synthesizer and turn it off
						// SCAL can be strobed from IDLE mode
						// without setting manual calibration mode (MCSM0.FS_AUTOCAL=0)
		SRX = 0x34,		// Enable RX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1
		STX = 0x35,		// In IDLE state: Enable TX. Perform calibration first if MCSM0.FS_AUTOCAL=1
						// If in RX state and CCA is enabled: Only go to TX if channel is clear
		SIDLE = 0x36,	// Exit RX / TX, turn off frequency synthesizer and exit Wake-On-Radio mode if applicable
		SWOR = 0x38,	// Start automatic RX polling sequence (Wake-on-Radio) if WORCTRL.RC_PD=0
		SPWD = 0x39,	// Enter power down mode when CSn goes high
		SFRX = 0x3A,	// Flush the RX FIFO buffer. Only issue SFRX in IDLE or RXFIFO_OVERFLOW states
		SFTX = 0x3B,	// Flush the TX FIFO buffer. Only issue SFTX in IDLE or TXFIFO_UNDERFLOW states
		SWORRST = 0x3C, // Reset real time clock to Event1 value
		SNOP = 0x3D,	// No operation. May be used to get access to the chip status byte
	};

	enum class StatusRegister : uint8_t
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

	enum class BurstRegister : uint8_t
	{
		PATABLE = 0x3E, // Power Amplifier table address
		TXFIFO = 0x3F,	// TX FIFO address
		RXFIFO = 0x3F,	// RX FIFO address (same as TXFIFO, but with read bit set)
	};

	enum class MARCState : uint8_t
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

	enum class TXPower : uint8_t
	{
		LOW = 0x03,	   // -30dBm
		BENCH = 0x2D,  //  -6dBm
		NORMAL = 0x50, //   0dBm
		HIGH = 0x0C,   // +10dBm
	};

	struct RegisterValueTuple
	{
		ConfigurationRegister addr;
		uint8_t value;
	};

	static constexpr uint8_t TXFIFO_UNDERFLOW = 0x80;
	static constexpr uint8_t RXFIFO_OVERFLOW = 0x80;
	static constexpr uint8_t CRC_OK = 0x80;

	static constexpr uint8_t READ = 0x80;
	static constexpr uint8_t WRITE = 0x00;
	static constexpr uint8_t BURST = 0x40;
	static constexpr uint8_t REGISTER = 0x00;

	// -------------------------------------------------------------------------
	// Public Methods
	// -------------------------------------------------------------------------

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
	void Init();

	void Configure(const RegisterValueTuple *tuples, size_t num_tuples);

	/**
	 * @brief Send a CC1101 strobe command.
	 *
	 * @param strobe  Strobe command byte.
	 * @return Status byte returned by the CC1101.
	 */
	uint8_t Strobe(StrobeCommand strobe);

	/**
	 * @brief Write a single CC1101 register.
	 *
	 * @param addr   Register address (0x00–0x2E).
	 * @param value  Value to write.
	 */
	void WriteConfiguration(ConfigurationRegister addr, uint8_t value);

	/**
	 * @brief Read a single CC1101 register.
	 *
	 * @param addr   Register address (0x00–0x2E).
	 * @return Register value.
	 */
	uint8_t ReadConfiguration(ConfigurationRegister addr);

	/**
	 * @brief Read a single CC1101 status register.
	 *
	 * @param addr   Register address (0x30-0x3D).
	 * @return Register value.
	 */
	uint8_t ReadStatus(StatusRegister addr);

	/**
	 * @brief Write multiple bytes to a CC1101 FIFO or burst‑capable register.
	 *
	 * Automatically sets the burst‑write bit (0x40).
	 *
	 * @param addr   Base address (e.g., 0x3F for TX FIFO).
	 * @param data   Pointer to data buffer.
	 * @param len    Number of bytes to write (clamped to 63).
	 */
	void WriteBurst(BurstRegister addr, const uint8_t *data, uint8_t len);

	/**
	 * @brief Read multiple bytes from a CC1101 FIFO or burst‑capable register.
	 *
	 * Automatically sets the burst‑read bit (0xC0).
	 *
	 * @param addr   Base address (e.g., 0x3F for RX FIFO).
	 * @param data   Output buffer.
	 * @param len    Number of bytes to read.
	 */
	void ReadBurst(BurstRegister addr, uint8_t *data, uint8_t len);

	/**
	 * @brief Set TX power.
	 *
	 * @param dBm TX power level.
	 */
	void SetPower(TXPower dBm);

private:
	void Select();
	void Deselect();
	void WriteRegister(uint8_t addr, uint8_t value);
	uint8_t ReadRegister(uint8_t addr);
	void Reset();
};

// -----------------------------------------------------------------------------
// Template Implementations
// -----------------------------------------------------------------------------

/**
 * @brief Minimal CC1101 driver for RX operation.
 *
 * This module provides:
 *   - Correct CC1101 reset sequence (TI SWRS061)
 *   - Register initialization for 433 MHz, 38.4 kbps 2‑FSK
 *   - Whitening + CRC
 *   - Variable‑length packets (max payload = 61 bytes)
 *   - FIFO burst read/write helpers
 *   - Strobe command helper
 *
 * The CC1101 is used here as a simple packet radio transport:
 *      [ length | payload… ]
 *
 * RX functionality is enabled by placing the radio into SLEEP after init
 * and letting the RX state machine explicitly strobe SRX.
 */

// -----------------------------------------------------------------------------
// CC1101 Register Initialization Table
//
// These values configure the radio for:
//   - 433.92 MHz operation
//   - 38.4 kbps 2‑FSK
//   - Whitening + CRC
//   - Variable‑length packets (PKTLEN = 61 max payload)
//   - Auto‑calibration on IDLE→RX (MCSM0 = 0x14)
//
// Each entry is { register_address, register_value }.
// Addresses follow TI datasheet SWRS061.
// -----------------------------------------------------------------------------

template <typename GPIO>
static constexpr typename CC1101<GPIO>::RegisterValueTuple cc1101_init_regs[] = {
	// --- GDO Configuration ---------------------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::IOCFG2, 0x06}, // IOCFG2: Asserts when sync word has been sent / received, and de-asserts at the end of the packet
	{CC1101<GPIO>::ConfigurationRegister::IOCFG0, 0x07}, // IOCFG0: GDO0 assert Packet received with CRC OK, deassert with fifo read

	// --- Sync Configuration ---------------------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::SYNC1, 0x3D}, // SYNC1
	{CC1101<GPIO>::ConfigurationRegister::SYNC0, 0x0C}, // SYNC0

	// --- Packet Engine -------------------------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::PKTLEN, 0x3D},   // PKTLEN: Max payload = 61 bytes
	{CC1101<GPIO>::ConfigurationRegister::PKTCTRL1, 0x0C}, // PKTCTRL1: No address check
	{CC1101<GPIO>::ConfigurationRegister::PKTCTRL0, 0x05}, // PKTCTRL0: Variable length, CRC enabled, whitening

	// --- Frequency Synthesizer ----------------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::FSCTRL1, 0x06}, // FSCTRL1: IF frequency offset
	{CC1101<GPIO>::ConfigurationRegister::FSCTRL0, 0x00}, // FSCTRL0: Frequency offset (LSB)

	// 433.92 MHz center frequency (per TI calculator)
	{CC1101<GPIO>::ConfigurationRegister::FREQ2, 0x10}, // FREQ2
	{CC1101<GPIO>::ConfigurationRegister::FREQ1, 0xA1}, // FREQ1
	{CC1101<GPIO>::ConfigurationRegister::FREQ0, 0x7A}, // FREQ0

	// --- Modem Configuration -------------------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::MDMCFG4, 0xCA}, // MDMCFG4: 38.4 kbps, channel BW
	{CC1101<GPIO>::ConfigurationRegister::MDMCFG3, 0x83}, // MDMCFG3: Data rate (mantissa)
	{CC1101<GPIO>::ConfigurationRegister::MDMCFG2, 0x13}, // MDMCFG2: 30/32 sync, 2‑FSK, whitening enabled
	{CC1101<GPIO>::ConfigurationRegister::MDMCFG1, 0x72}, // MDMCFG1: FEC off, preamble length (24)
	{CC1101<GPIO>::ConfigurationRegister::MDMCFG0, 0xF8}, // MDMCFG0: Channel spacing

	{CC1101<GPIO>::ConfigurationRegister::DEVIATN, 0x34}, // DEVIATN: Frequency deviation

	// --- Main Radio Control --------------------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::MCSM0, 0x18}, // MCSM0: Auto‑calibrate on IDLE→TX (important for RX!)

	// --- AGC / Frequency Compensation ----------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::FOCCFG, 0x1D},  // FOCCFG: Frequency offset compensation
	{CC1101<GPIO>::ConfigurationRegister::BSCFG, 0x6C},	  // BSCFG: Bit synchronization
	{CC1101<GPIO>::ConfigurationRegister::AGCTRL2, 0x43}, // AGCCTRL2: AGC target
	{CC1101<GPIO>::ConfigurationRegister::AGCTRL1, 0x40}, // AGCCTRL1: AGC hysteresis
	{CC1101<GPIO>::ConfigurationRegister::AGCTRL0, 0x91}, // AGCCTRL0: AGC filter settings

	// --- Front End -----------------------------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::FREND1, 0x56}, // FREND1: Front‑end RX configuration
	{CC1101<GPIO>::ConfigurationRegister::FREND0, 0x10}, // FREND0

	// --- Frequency Synthesizer Calibration -----------------------------------
	{CC1101<GPIO>::ConfigurationRegister::FSCAL3, 0xE9}, // FSCAL3
	{CC1101<GPIO>::ConfigurationRegister::FSCAL2, 0x2A}, // FSCAL2
	{CC1101<GPIO>::ConfigurationRegister::FSCAL1, 0x00}, // FSCAL1
	{CC1101<GPIO>::ConfigurationRegister::FSCAL0, 0x1F}, // FSCAL0

	// --- Test Registers (TI Recommended) -------------------------------------
	{CC1101<GPIO>::ConfigurationRegister::FSTEST, 0x59}, // FSTEST: Synthesizer test
	{CC1101<GPIO>::ConfigurationRegister::TEST2, 0x81},	 // TEST2: Modem test
	{CC1101<GPIO>::ConfigurationRegister::TEST1, 0x35},	 // TEST1: Modem test
	{CC1101<GPIO>::ConfigurationRegister::TEST0, 0x09},	 // TEST0: Modem test
};

// -----------------------------------------------------------------------------
// SPI Chip‑Select Helpers
// -----------------------------------------------------------------------------
template <typename GPIO>
void CC1101<GPIO>::Select()
{
	GPIO::low();

	// Wait for MISO to go low → CC1101 ready for SPI access
	uint32_t timeout = 1000;
	while (GPIO::read() == GPIO_PIN_SET && timeout-- > 0)
		;
}

template <typename GPIO>
void CC1101<GPIO>::Deselect()
{
	GPIO::high();
}

// -----------------------------------------------------------------------------
// Single Byte Read/Write Helpers
// -----------------------------------------------------------------------------

template <typename GPIO>
void CC1101<GPIO>::WriteRegister(uint8_t addr, uint8_t value)
{
	uint8_t tx[2]{addr, value};

	Select();
	HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
	Deselect();
}

template <typename GPIO>
uint8_t CC1101<GPIO>::ReadRegister(uint8_t addr)
{
	uint8_t tx[2]{static_cast<uint8_t>(addr | READ), 0x00};
	uint8_t rx[2]{};

	Select();
	HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
	Deselect();

	return rx[1];
}

// -----------------------------------------------------------------------------
// Configuration Register Access
// -----------------------------------------------------------------------------
template <typename GPIO>
void CC1101<GPIO>::WriteConfiguration(typename CC1101<GPIO>::ConfigurationRegister addr, uint8_t value)
{
	WriteRegister(static_cast<uint8_t>(addr), value);
}

template <typename GPIO>
uint8_t CC1101<GPIO>::ReadConfiguration(typename CC1101<GPIO>::ConfigurationRegister addr)
{
	return ReadRegister(static_cast<uint8_t>(addr));
}

template <typename GPIO>
uint8_t CC1101<GPIO>::ReadStatus(typename CC1101<GPIO>::StatusRegister addr)
{
	// Status registers (0x30-0x3D) require bits 0x80 (Read) AND 0x40 (Burst)
	return ReadRegister(static_cast<uint8_t>(addr) | BURST);
}

// -----------------------------------------------------------------------------
// Strobe Commands
// -----------------------------------------------------------------------------
template <typename GPIO>
uint8_t CC1101<GPIO>::Strobe(typename CC1101<GPIO>::StrobeCommand strobe)
{
	uint8_t cmd = static_cast<uint8_t>(strobe);
	uint8_t status = 0;

	Select();
	HAL_SPI_TransmitReceive(&hspi1, &cmd, &status, 1, HAL_MAX_DELAY);
	Deselect();

	return status;
}

// -----------------------------------------------------------------------------
// Burst FIFO Access
// -----------------------------------------------------------------------------
template <typename GPIO>
void CC1101<GPIO>::WriteBurst(typename CC1101<GPIO>::BurstRegister addr, const uint8_t *data, uint8_t len)
{
	if (len > 64)
		len = 64;

	uint8_t buffer[64 + 1];
	constexpr uint8_t BURST_READ = BURST | WRITE;
	buffer[0] = static_cast<uint8_t>(addr) | BURST_READ;
	memcpy(&buffer[1], data, len);

	Select();
	HAL_SPI_Transmit(&hspi1, buffer, len + 1, HAL_MAX_DELAY);
	Deselect();
}

template <typename GPIO>
void CC1101<GPIO>::ReadBurst(typename CC1101<GPIO>::BurstRegister addr, uint8_t *data, uint8_t len)
{
	constexpr uint8_t BURST_READ = BURST | READ;
	uint8_t header = static_cast<uint8_t>(addr) | BURST_READ;

	Select();
	HAL_SPI_Transmit(&hspi1, &header, 1, HAL_MAX_DELAY);
	HAL_SPI_Receive(&hspi1, data, len, HAL_MAX_DELAY);
	Deselect();
}

// -----------------------------------------------------------------------------
// CC1101 Reset (TI‑specified sequence)
// -----------------------------------------------------------------------------
template <typename GPIO>
void CC1101<GPIO>::Reset()
{
	Deselect();
	HAL_Delay(1);

	Select();
	HAL_Delay(1);

	Deselect();
	HAL_Delay(1);

	Select();

	uint8_t cmd = 0x30;
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);

	while (HAL_GPIO_ReadPin(SPI1_Port, SPI1_MISO_Pin) == GPIO_PIN_SET)
		;

	Deselect();
}

// -----------------------------------------------------------------------------
// CC1101 Initialization
// -----------------------------------------------------------------------------
template <typename GPIO>
void CC1101<GPIO>::Init()
{
	Reset();

	constexpr auto &table = cc1101_init_regs<GPIO>;
	constexpr size_t num = sizeof(table) / sizeof(table[0]);

	Configure(table, num);

	// Flush FIFOs
	Strobe(CC1101<GPIO>::StrobeCommand::SFRX);
	Strobe(CC1101<GPIO>::StrobeCommand::SFTX);

	// Enter SLEEP — RX state machine will explicitly issue SRX
	Strobe(CC1101<GPIO>::StrobeCommand::SIDLE);
	Strobe(CC1101<GPIO>::StrobeCommand::SPWD);
}

// -----------------------------------------------------------------------------
// CC1101 Configure with a pairs of CC1101<GPIO>::ConfigurationRegister, uint8_t
// -----------------------------------------------------------------------------
template <typename GPIO>
void CC1101<GPIO>::Configure(const typename CC1101<GPIO>::RegisterValueTuple *tuples, size_t num_tuples)
{
	for (size_t i = 0; i < num_tuples; ++i)
	{
		WriteConfiguration(static_cast<ConfigurationRegister>(tuples[i].addr), tuples[i].value);
	}
}

// -----------------------------------------------------------------------------
// CC1101 TX Power
// -----------------------------------------------------------------------------
template <typename GPIO>
void CC1101<GPIO>::SetPower(typename CC1101<GPIO>::TXPower dBm)
{
	// We write a single byte to PATABLE to set the output power.
	WriteConfiguration(CC1101<GPIO>::ConfigurationRegister::PATABLE, static_cast<uint8_t>(dBm));
}
