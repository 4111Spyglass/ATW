/**
 * @file States.hpp
 * @brief Shared state definitions for the USB → RF transmit pipeline.
 *
 * This header defines:
 *   - The USB→RF state machine states
 *   - The accumulation buffer used by the USB IRQ
 *   - The length counter for accumulated bytes
 *   - The inter‑packet timeout flag
 *
 * The state machine itself is implemented in RF_USB_StateMachine.cpp.
 *
 * Overview of the TX pipeline:
 *
 *      USB IRQ → ACCUM → timeout → TX_PREP → TX_WAIT → IDLE
 *
 * The USB IRQ appends bytes into usb_rf_buf[] and restarts a timer.
 * When the timer expires, the state machine snapshots the buffer and
 * transmits a CC1101 packet.
 */

#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

// -----------------------------------------------------------------------------
// USB → RF accumulation buffer
//
// usb_rf_buf[] is filled by the USB CDC receive callback.
// The RF state machine snapshots this buffer before transmission.
//
// Size = 48 bytes:
//   - 1 byte is used for the CC1101 length field
//   - 47 bytes remain for payload
//   - The CC1101 supports up to 61 bytes payload, but USB chunking and
//     timing constraints make 48 a safe and efficient MTU for this pipeline.
// -----------------------------------------------------------------------------
#define USB_RF_BUF_SIZE   48

// -----------------------------------------------------------------------------
// USB → RF transmit state machine states
//
// U_STATE_IDLE
//      No data pending. Waiting for USB IRQ to append bytes.
//
// U_STATE_ACCUM
//      USB IRQ is filling usb_rf_buf[]. A timer is running.
//      When the timer expires, the buffer is snapshotted for TX.
//
// U_STATE_TX_PREP
//      CC1101 is forced to IDLE, FIFO is flushed, packet is written,
//      and STX is issued. No logging occurs in the timing‑critical window.
//
// U_STATE_TX_WAIT
//      Poll MARCSTATE until TX completes (returns to IDLE).
// -----------------------------------------------------------------------------
typedef enum {
    U_STATE_IDLE = 0,   ///< Waiting for USB data
    U_STATE_ACCUM,      ///< Accumulating USB bytes, timeout running
    U_STATE_TX_PREP,    ///< Preparing CC1101 for TX (IDLE→FIFO→STX)
    U_STATE_TX_WAIT     ///< Waiting for TX to complete
} U_State_t;

// -----------------------------------------------------------------------------
// Global state variables (owned by the TX pipeline)
// -----------------------------------------------------------------------------

/// Current USB→RF state machine state
extern volatile U_State_t u_state;

/// Accumulation buffer filled by USB IRQ
extern uint8_t usb_rf_buf[USB_RF_BUF_SIZE];

/// Number of valid bytes in usb_rf_buf[]
extern uint16_t usb_rf_len;

/// Set to 1 by TIM5 when the inter‑packet timeout expires
extern volatile uint8_t usb_timeout_elapsed;
