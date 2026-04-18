/**
 * @file CC1101.cpp
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

#include <cstring>
#include <cstdint>

#include "main.h"
#include "CC1101.hpp"
#include "stm32f4xx_hal.h"

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
static const CC1101_RegisterValueTuple cc1101_init_regs[] = {
	// --- GDO Configuration ---------------------------------------------------
	{CC1101_ConfigurationRegister::IOCFG2, 0x06}, // IOCFG2: Asserts when sync word has been sent / received, and de-asserts at the end of the packet
	{CC1101_ConfigurationRegister::IOCFG0, 0x07}, // IOCFG0: GDO0 assert Packet received with CRC OK, deassert with fifo read

	// --- Sync Configuration ---------------------------------------------------
	{CC1101_ConfigurationRegister::SYNC1, 0x3D}, // SYNC1
	{CC1101_ConfigurationRegister::SYNC0, 0x0C}, // SYNC0

    // --- Packet Engine -------------------------------------------------------
    {CC1101_ConfigurationRegister::PKTLEN, 0x3D}, // PKTLEN: Max payload = 61 bytes
    {CC1101_ConfigurationRegister::PKTCTRL1, 0x0C}, // PKTCTRL1: No address check
    {CC1101_ConfigurationRegister::PKTCTRL0, 0x05}, // PKTCTRL0: Variable length, CRC enabled, whitening

    // --- Frequency Synthesizer ----------------------------------------------
    {CC1101_ConfigurationRegister::FSCTRL1, 0x06}, // FSCTRL1: IF frequency offset
    {CC1101_ConfigurationRegister::FSCTRL0, 0x00}, // FSCTRL0: Frequency offset (LSB)

    // 433.92 MHz center frequency (per TI calculator)
    {CC1101_ConfigurationRegister::FREQ2, 0x10}, // FREQ2
    {CC1101_ConfigurationRegister::FREQ1, 0xA1}, // FREQ1
    {CC1101_ConfigurationRegister::FREQ0, 0x7A}, // FREQ0

    // --- Modem Configuration -------------------------------------------------
    {CC1101_ConfigurationRegister::MDMCFG4, 0xCA}, // MDMCFG4: 38.4 kbps, channel BW
    {CC1101_ConfigurationRegister::MDMCFG3, 0x83}, // MDMCFG3: Data rate (mantissa)
    {CC1101_ConfigurationRegister::MDMCFG2, 0x13}, // MDMCFG2: 30/32 sync, 2‑FSK, whitening enabled
    {CC1101_ConfigurationRegister::MDMCFG1, 0x72}, // MDMCFG1: FEC off, preamble length (24)
    {CC1101_ConfigurationRegister::MDMCFG0, 0xF8}, // MDMCFG0: Channel spacing

    {CC1101_ConfigurationRegister::DEVIATN, 0x34}, // DEVIATN: Frequency deviation

    // --- Main Radio Control --------------------------------------------------
    {CC1101_ConfigurationRegister::MCSM0, 0x18}, // MCSM0: Auto‑calibrate on IDLE→TX (important for RX!)

    // --- AGC / Frequency Compensation ----------------------------------------
    {CC1101_ConfigurationRegister::FOCCFG, 0x1D}, // FOCCFG: Frequency offset compensation
    {CC1101_ConfigurationRegister::BSCFG, 0x6C}, // BSCFG: Bit synchronization
    {CC1101_ConfigurationRegister::AGCTRL2, 0x43}, // AGCCTRL2: AGC target
    {CC1101_ConfigurationRegister::AGCTRL1, 0x40}, // AGCCTRL1: AGC hysteresis
    {CC1101_ConfigurationRegister::AGCTRL0, 0x91}, // AGCCTRL0: AGC filter settings

    // --- Front End -----------------------------------------------------------
    {CC1101_ConfigurationRegister::FREND1, 0x56}, // FREND1: Front‑end RX configuration
    {CC1101_ConfigurationRegister::FREND0, 0x10}, // FREND0

    // --- Frequency Synthesizer Calibration -----------------------------------
    {CC1101_ConfigurationRegister::FSCAL3, 0xE9}, // FSCAL3
    {CC1101_ConfigurationRegister::FSCAL2, 0x2A}, // FSCAL2
    {CC1101_ConfigurationRegister::FSCAL1, 0x00}, // FSCAL1
    {CC1101_ConfigurationRegister::FSCAL0, 0x1F}, // FSCAL0

    // --- Test Registers (TI Recommended) -------------------------------------
    {CC1101_ConfigurationRegister::FSTEST, 0x59}, // FSTEST: Synthesizer test
    {CC1101_ConfigurationRegister::TEST2, 0x81}, // TEST2: Modem test
    {CC1101_ConfigurationRegister::TEST1, 0x35}, // TEST1: Modem test
    {CC1101_ConfigurationRegister::TEST0, 0x09}, // TEST0: Modem test
};

static constexpr uint8_t CC1101_READ = 0x80;
static constexpr uint8_t CC1101_WRITE = 0x00;
static constexpr uint8_t CC1101_BURST = 0x40;
static constexpr uint8_t CC1101_REGISTER = 0x00;


// -----------------------------------------------------------------------------
// SPI Chip‑Select Helpers
// -----------------------------------------------------------------------------
static void CC1101_Select(void)
{
    HAL_GPIO_WritePin(SPI1_Port, SPI1_CS_Pin, GPIO_PIN_RESET);

    // Wait for MISO to go low → CC1101 ready for SPI access
    uint32_t timeout = 1000;
    while (HAL_GPIO_ReadPin(SPI1_Port, SPI1_MISO_Pin) == GPIO_PIN_SET &&
           timeout-- > 0);
}

static void CC1101_Deselect(void)
{
    HAL_GPIO_WritePin(SPI1_Port, SPI1_CS_Pin, GPIO_PIN_SET);
}

// -----------------------------------------------------------------------------
// Single Byte Read/Write Helpers
// -----------------------------------------------------------------------------

void CC1101_WriteRegister(uint8_t addr, uint8_t value)
{
    uint8_t tx[2]{ addr, value };
    
    CC1101_Select();
    HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
    CC1101_Deselect();
}

uint8_t CC1101_ReadRegister(uint8_t addr)
{
    uint8_t tx[2]{ static_cast<uint8_t>(addr | CC1101_READ), 0x00 };
    uint8_t rx[2]{};

    CC1101_Select();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    CC1101_Deselect();

    return rx[1];
}


// -----------------------------------------------------------------------------
// Configuration Register Access
// -----------------------------------------------------------------------------
void CC1101_WriteConfiguration(CC1101_ConfigurationRegister addr, uint8_t value)
{
    CC1101_WriteRegister(static_cast<uint8_t>(addr), value);
}

uint8_t CC1101_ReadConfiguration(CC1101_ConfigurationRegister addr)
{
    return CC1101_ReadRegister(static_cast<uint8_t>(addr));
}

uint8_t CC1101_ReadStatus(CC1101_StatusRegister addr) {
    // Status registers (0x30-0x3D) require bits 0x80 (Read) AND 0x40 (Burst)
    return CC1101_ReadRegister(static_cast<uint8_t>(addr) | CC1101_BURST);
}

// -----------------------------------------------------------------------------
// Strobe Commands
// -----------------------------------------------------------------------------
uint8_t CC1101_Strobe(CC1101_StrobeCommand strobe)
{
    uint8_t cmd = static_cast<uint8_t>(strobe);
    uint8_t status = 0;

    CC1101_Select();
    HAL_SPI_TransmitReceive(&hspi1, &cmd, &status, 1, HAL_MAX_DELAY);
    CC1101_Deselect();

    return status;
}

// -----------------------------------------------------------------------------
// Burst FIFO Access
// -----------------------------------------------------------------------------
void CC1101_WriteBurst(CC1101_BurstRegister addr, const uint8_t *data, uint8_t len)
{
    if (len > 64) len = 64;

    uint8_t buffer[64+1];
    constexpr uint8_t BURST_READ = CC1101_BURST | CC1101_WRITE;
    buffer[0] = static_cast<uint8_t>(addr) | BURST_READ;
    memcpy(&buffer[1], data, len);

    CC1101_Select();
    HAL_SPI_Transmit(&hspi1, buffer, len + 1, HAL_MAX_DELAY);
    CC1101_Deselect();
}

void CC1101_ReadBurst(CC1101_BurstRegister addr, uint8_t *data, uint8_t len)
{
    constexpr uint8_t BURST_READ = CC1101_BURST | CC1101_READ;
	uint8_t header = static_cast<uint8_t>(addr) | BURST_READ;


    CC1101_Select();
    HAL_SPI_Transmit(&hspi1, &header, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, data, len, HAL_MAX_DELAY);
    CC1101_Deselect();
}

// -----------------------------------------------------------------------------
// CC1101 Reset (TI‑specified sequence)
// -----------------------------------------------------------------------------
void CC1101_Reset()
{
    CC1101_Deselect();
    HAL_Delay(1);

    CC1101_Select();
    HAL_Delay(1);

    CC1101_Deselect();
    HAL_Delay(1);

    CC1101_Select();

    uint8_t cmd = 0x30;
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);

    while (HAL_GPIO_ReadPin(SPI1_Port, SPI1_MISO_Pin) == GPIO_PIN_SET);

    CC1101_Deselect();
}

// -----------------------------------------------------------------------------
// CC1101 Initialization
// -----------------------------------------------------------------------------
void CC1101_Init()
{
    CC1101_Reset();
    CC1101_Configure(cc1101_init_regs, sizeof(cc1101_init_regs) / sizeof(CC1101_RegisterValueTuple));
    
    // Flush FIFOs
    CC1101_Strobe(CC1101_StrobeCommand::SFRX);
    CC1101_Strobe(CC1101_StrobeCommand::SFTX);

    // Enter SLEEP — RX state machine will explicitly issue SRX
    CC1101_Strobe(CC1101_StrobeCommand::SIDLE);
    CC1101_Strobe(CC1101_StrobeCommand::SPWD);
}

// -----------------------------------------------------------------------------
// CC1101 Configure with a pairs of CC1101_ConfigurationRegister, uint8_t
// -----------------------------------------------------------------------------

void CC1101_Configure(const CC1101_RegisterValueTuple *tuples, size_t num_tuples)
{
    for (size_t i = 0; i < num_tuples; i++)
        CC1101_WriteConfiguration(tuples[i].addr, tuples[i].value);
}   
