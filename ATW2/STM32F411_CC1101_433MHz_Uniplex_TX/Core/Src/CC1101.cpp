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
static const struct {
    uint8_t addr;
    uint8_t value;
} cc1101_init_regs[] = {

	// --- GDO Configuration ---------------------------------------------------
	{0x00, 0x2E}, // IOCFG2: GDO2 PA_PD: high during TX, low when TX done
	{0x02, 0x07}, // IOCFG0: GDO0 Packet received, CRC OK, deassert end of packet

	// --- Sync Configuration ---------------------------------------------------
	{0x04, 0x3D}, // SYNC1
	{0x05, 0x0C}, // SYNC0

    // --- Packet Engine -------------------------------------------------------
    {0x06, 0x3D}, // PKTLEN: Max payload = 61 bytes
    {0x07, 0x0C}, // PKTCTRL1: No address check
    {0x08, 0x05}, // PKTCTRL0: Variable length, CRC enabled, whitening

    // --- Frequency Synthesizer ----------------------------------------------
    {0x0B, 0x06}, // FSCTRL1: IF frequency offset
    {0x0C, 0x00}, // FSCTRL0: Frequency offset (LSB)

    // 433.92 MHz center frequency (per TI calculator)
    {0x0D, 0x10}, // FREQ2
    {0x0E, 0xA1}, // FREQ1
    {0x0F, 0x7A}, // FREQ0

    // --- Modem Configuration -------------------------------------------------
    {0x10, 0xCA}, // MDMCFG4: 38.4 kbps, channel BW
    {0x11, 0x83}, // MDMCFG3: Data rate (mantissa)
    {0x12, 0x13}, // MDMCFG2: 30/32 sync, 2‑FSK, whitening enabled
    {0x13, 0x22}, // MDMCFG1: FEC off, preamble length
    {0x14, 0xF8}, // MDMCFG0: Channel spacing

    {0x15, 0x34}, // DEVIATN: Frequency deviation

    // --- Main Radio Control --------------------------------------------------
    {0x18, 0x18}, // MCSM0: Auto‑calibrate on IDLE→TX (important for RX!)

    // --- AGC / Frequency Compensation ----------------------------------------
    {0x19, 0x16}, // FOCCFG: Frequency offset compensation
    {0x1A, 0x6C}, // BSCFG: Bit synchronization
    {0x1B, 0x03}, // AGCCTRL2: AGC target
    {0x1C, 0x40}, // AGCCTRL1: AGC hysteresis
    {0x1D, 0x91}, // AGCCTRL0: AGC filter settings

    // --- Front End -----------------------------------------------------------
    {0x21, 0x56}, // FREND1: Front‑end RX configuration
    {0x22, 0x10}, // FREND0

    // --- Frequency Synthesizer Calibration -----------------------------------
    {0x23, 0xE9}, // FSCAL3
    {0x24, 0x2A}, // FSCAL2
    {0x25, 0x00}, // FSCAL1
    {0x26, 0x1F}, // FSCAL0

    // --- Test Registers (TI Recommended) -------------------------------------
    {0x29, 0x59}, // FSTEST: Synthesizer test
    {0x2C, 0x81}, // TEST2: Modem test
    {0x2D, 0x35}, // TEST1: Modem test
    {0x2E, 0x09}, // TEST0: Modem test
};

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
// Register Access
// -----------------------------------------------------------------------------
void CC1101_WriteReg(uint8_t addr, uint8_t value)
{
    uint8_t tx[2] = { addr, value };
    CC1101_Select();
    HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
    CC1101_Deselect();
}

uint8_t CC1101_ReadReg(uint8_t addr)
{
    uint8_t tx[2] = { (uint8_t)(addr | 0x80), 0x00 };
    uint8_t rx[2] = {0};

    CC1101_Select();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    CC1101_Deselect();

    return rx[1];
}

// -----------------------------------------------------------------------------
// Strobe Commands
// -----------------------------------------------------------------------------
uint8_t CC1101_Strobe(CC1101_StrobeCmd strobe)
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
void CC1101_WriteBurst(uint8_t addr, const uint8_t *data, uint8_t len)
{
    if (len > 63) len = 63;

    uint8_t buffer[64];
    buffer[0] = addr | 0x40;
    memcpy(&buffer[1], data, len);

    CC1101_Select();
    HAL_SPI_Transmit(&hspi1, buffer, len + 1, HAL_MAX_DELAY);
    CC1101_Deselect();
}

void CC1101_ReadBurst(uint8_t addr, uint8_t *data, uint8_t len)
{
    uint8_t header = addr | 0xC0;

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

    for (unsigned i = 0; i < sizeof(cc1101_init_regs)/sizeof(cc1101_init_regs[0]); i++)
        CC1101_WriteReg(cc1101_init_regs[i].addr, cc1101_init_regs[i].value);

    // Flush FIFOs
    CC1101_Strobe(CC1101_StrobeCmd::SFRX);
    CC1101_Strobe(CC1101_StrobeCmd::SFTX);

    // Enter SLEEP — RX state machine will explicitly issue SRX
    CC1101_Strobe(CC1101_StrobeCmd::SIDLE);
    CC1101_Strobe(CC1101_StrobeCmd::SPWD);
}
