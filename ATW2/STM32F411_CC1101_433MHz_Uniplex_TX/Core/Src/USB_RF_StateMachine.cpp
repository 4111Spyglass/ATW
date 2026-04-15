/**
 * @file RF_USB_StateMachine.cpp
 * @brief Deterministic USB → RF transmit state machine for CC1101‑based nodes.
 *
 * This module implements a non‑blocking, interrupt‑safe pipeline that:
 *
 *   1. Accumulates bytes arriving from USB CDC (via IRQ callback)
 *   2. Emits a single RF packet after an inter‑packet timeout
 *   3. Uses a snapshot buffer to avoid races with the USB IRQ
 *   4. Transmits via CC1101 in variable‑length packet mode
 *
 * Packet format:
 *      [ length (1 byte) | payload (N bytes) ]
 *
 * The CC1101 is configured for:
 *      - 433.92 MHz
 *      - 38.4 kbps 2‑FSK
 *      - Whitening + CRC
 *      - Variable‑length packets (max payload = 61 bytes)
 *
 * This file contains **only the TX state machine**.
 * A matching RX state machine can be added later.
 */

#include <cstring>
#include <cstdio>

#include "USB_RF_StateMachine.hpp"
#include "States.hpp"
#include "CC1101.hpp"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"

// -----------------------------------------------------------------------------
// Debug printing helper (non‑blocking enough for low‑rate logging)
// -----------------------------------------------------------------------------
static void dbg_print(const char* msg)
{
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
}

// -----------------------------------------------------------------------------
// Snapshot buffer used during TX.
//
// usb_rf_buf[] is filled by the USB IRQ.
// When a TX is triggered, we copy the accumulated bytes into tx_snapshot[].
// This prevents races between USB IRQ and RF TX timing windows.
// -----------------------------------------------------------------------------
static uint8_t tx_snapshot[USB_RF_BUF_SIZE + 1];
static uint8_t tx_snapshot_len = 0;

// -----------------------------------------------------------------------------
// Main USB → RF transmit state machine
// -----------------------------------------------------------------------------
void USB_RF_StateMachine(void)
{
    switch (u_state)
    {
    // -------------------------------------------------------------------------
    // IDLE: Waiting for USB data to accumulate.
    // -------------------------------------------------------------------------
    case U_STATE_IDLE:
        break;

    // -------------------------------------------------------------------------
    // ACCUM: USB IRQ is filling usb_rf_buf[].
    //
    // When the inter‑packet timeout expires, we snapshot the buffer and begin TX.
    // -------------------------------------------------------------------------
    case U_STATE_ACCUM:
        if (usb_rf_len > 0 && usb_timeout_elapsed)
        {
            usb_timeout_elapsed = 0;

            // Snapshot accumulated bytes (max 61 payload bytes)
            tx_snapshot_len = usb_rf_len;
            if (tx_snapshot_len > 61)
                tx_snapshot_len = 61;

            tx_snapshot[0] = tx_snapshot_len;
            memcpy(&tx_snapshot[1], usb_rf_buf, tx_snapshot_len);

            // Clear accumulation buffer for next message
            usb_rf_len = 0;

            // Log before entering timing‑critical RF section
            char buffer[64];
            sprintf(buffer, "TX_START len=%u\n", tx_snapshot_len);
            dbg_print(buffer);

            u_state = U_STATE_TX_PREP;
        }
        break;

    // -------------------------------------------------------------------------
    // TX_PREP: Prepare CC1101 for transmission.
    //
    // Steps:
    //   1. Force IDLE
    //   2. Wait for IDLE (bounded)
    //   3. Flush TX FIFO
    //   4. Write packet [len][payload…]
    //   5. Issue STX
    //
    // No logging occurs inside the timing‑critical window.
    // -------------------------------------------------------------------------
    case U_STATE_TX_PREP:
    {
        uint8_t len = tx_snapshot_len;

        if (len == 0)
        {
            dbg_print("TX_PREP: zero length, abort\n");
            u_state = U_STATE_IDLE;
            break;
        }

        // Pre‑TX MARCSTATE (safe to log)
        {
            uint8_t marc = CC1101_ReadReg(0x35 | 0xC0) & 0x1F;
            char buffer[64];
            sprintf(buffer, "PRE-TX MARCSTATE=%02X\n", marc);
            dbg_print(buffer);
        }

        // ---------------- TIMING‑CRITICAL SECTION ----------------

        // Force IDLE
        CC1101_Strobe(CC1101_StrobeCmd::SIDLE);
        HAL_Delay(1);

        // Wait for IDLE (bounded wait)
        uint32_t t0 = HAL_GetTick();
        while (1)
        {
            uint8_t marc = CC1101_ReadReg(0x35 | 0xC0) & 0x1F;
            if (marc == 0x01) break;          // IDLE
            if (HAL_GetTick() - t0 > 5) break; // Timeout
        }

        // Flush TX FIFO and load packet
        CC1101_Strobe(CC1101_StrobeCmd::SFTX);
        CC1101_WriteBurst(0x3F, tx_snapshot, len + 1);

        // Optional diagnostic: TX FIFO occupancy
        {
            uint8_t txbytes = CC1101_ReadReg(0x3A | 0xC0);
            char buffer[64];
            sprintf(buffer, "TXBYTES=%02X\n", txbytes);
            dbg_print(buffer);
        }

        // Start RF transmission
        CC1101_Strobe(CC1101_StrobeCmd::STX);

        // ----------------------------------------------------------

        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        u_state = U_STATE_TX_WAIT;
        break;
    }

    // -------------------------------------------------------------------------
    // TX_WAIT: Poll MARCSTATE until TX completes.
    //
    // Valid MARCSTATE values during TX:
    //   0x08  STARTCAL
    //   0x09  BWBOOST
    //   0x0A  FS_LOCK
    //   0x13  TX (actively transmitting)
    //   0x14  TX_END
    //
    // TX is complete when MARCSTATE returns to IDLE (0x01).
    // -------------------------------------------------------------------------
    case U_STATE_TX_WAIT:
    {
        uint8_t marc = CC1101_ReadReg(0x35 | 0xC0) & 0x1F;

        // TX complete
        if (marc == 0x01)
        {
            dbg_print("TX_DONE\n");
            u_state = U_STATE_IDLE;
            break;
        }

        // True TXFIFO underflow (rare)
        if (marc == 0x16)
        {
            dbg_print("TX_UNDERFLOW\n");

            CC1101_Strobe(CC1101_StrobeCmd::SIDLE);
            CC1101_Strobe(CC1101_StrobeCmd::SFTX);

            u_state = U_STATE_IDLE;
            break;
        }

        // Otherwise remain in TX_WAIT
        break;
    }

    // -------------------------------------------------------------------------
    // Invalid state fallback
    // -------------------------------------------------------------------------
    default:
        dbg_print("USB_RF: invalid state\n");
        u_state = U_STATE_IDLE;
        break;
    }
}
