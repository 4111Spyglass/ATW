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

enum class CC1101_StrobeCmd : uint8_t
{
    SRES  = 0x30, // Reset
    SRX   = 0x34, // Enter RX
    STX   = 0x35, // Enter TX
    SIDLE = 0x36, // Enter IDLE
    SPWD  = 0x39, // Enter Power Down
    SFRX  = 0x3A, // Flush RX FIFO
    SFTX  = 0x3B  // Flush TX FIFO
};

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

/**
 * @brief Send a CC1101 strobe command.
 *
 * @param strobe  Strobe command byte.
 * @return Status byte returned by the CC1101.
 */
uint8_t CC1101_Strobe(CC1101_StrobeCmd strobe);

/**
 * @brief Write a single CC1101 register.
 *
 * @param addr   Register address (0x00–0x2E).
 * @param value  Value to write.
 */
void CC1101_WriteReg(uint8_t addr, uint8_t value);

/**
 * @brief Read a single CC1101 register.
 *
 * @param addr   Register address (0x00–0x2E).
 * @return Register value.
 */
uint8_t CC1101_ReadReg(uint8_t addr);

/**
 * @brief Write multiple bytes to a CC1101 FIFO or burst‑capable register.
 *
 * Automatically sets the burst‑write bit (0x40).
 *
 * @param addr   Base address (e.g., 0x3F for TX FIFO).
 * @param data   Pointer to data buffer.
 * @param len    Number of bytes to write (clamped to 63).
 */
void CC1101_WriteBurst(uint8_t addr, const uint8_t *data, uint8_t len);

/**
 * @brief Read multiple bytes from a CC1101 FIFO or burst‑capable register.
 *
 * Automatically sets the burst‑read bit (0xC0).
 *
 * @param addr   Base address (e.g., 0x3F for RX FIFO).
 * @param data   Output buffer.
 * @param len    Number of bytes to read.
 */
void CC1101_ReadBurst(uint8_t addr, uint8_t *data, uint8_t len);
