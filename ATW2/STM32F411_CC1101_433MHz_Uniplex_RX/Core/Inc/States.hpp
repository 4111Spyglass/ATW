/**
 * @file States.hpp
 * @brief Shared state definitions for the RF → USB receive pipeline.
 *
 * This header defines:
 *   - The RF→USB state machine states
 *   - The RX buffer used to hold CC1101 payloads
 *   - The length field for the current received packet
 *
 * The state machine itself is implemented in the RX pipeline source
 * (e.g., RF_USB_StateMachine.cpp or equivalent).
 *
 * Overview of the RX pipeline:
 *
 *      CC1101 → R_STATE_RX_WAIT → R_STATE_RX_READ → R_STATE_USB_TX → R_STATE_RX_WAIT
 *
 * Typical flow:
 *   - In R_STATE_RX_WAIT, the firmware waits for a CC1101 event
 *     (e.g., GDO line, polling, or status check) indicating a packet
 *     is available in the RX FIFO.
 *   - In R_STATE_RX_READ, the packet length and payload are read from
 *     the CC1101 RX FIFO into rf_rx_buf[], and rf_rx_len is updated.
 *   - In R_STATE_USB_TX, the received packet is forwarded over USB CDC
 *     to the host. Once transmission is complete, the state machine
 *     returns to R_STATE_RX_WAIT.
 */

#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

// -----------------------------------------------------------------------------
// RF → USB receive buffer
//
// rf_rx_buf[] holds a single CC1101 packet as received from the RX FIFO.
// Size = 64 bytes:
//   - Sufficient for the CC1101 variable-length payload (up to 61 bytes)
//   - Plus length byte and margin
// -----------------------------------------------------------------------------
#define RF_RX_BUF_SIZE  64

// -----------------------------------------------------------------------------
// RF → USB receive state machine states
//
// R_STATE_RX_WAIT
//      Default idle state. Waiting for a CC1101 packet to become available.
//
// R_STATE_RX_READ
//      A packet is pending in the CC1101 RX FIFO. The firmware reads the
//      length and payload into rf_rx_buf[], and updates rf_rx_len.
//
// R_STATE_USB_TX
//      The received packet is being sent over USB CDC to the host. When
//      USB transmission completes, the state machine returns to RX_WAIT.
// -----------------------------------------------------------------------------
typedef enum {
    R_STATE_RX_WAIT = 0,  ///< Waiting for CC1101 packet
    R_STATE_RX_READ,      ///< Reading packet from CC1101 RX FIFO
    R_STATE_USB_TX        ///< Forwarding packet to USB host
} R_State_t;

// -----------------------------------------------------------------------------
// Global state variables (owned by the RX pipeline)
// -----------------------------------------------------------------------------

/// Current RF→USB receive state machine state
extern volatile R_State_t r_state;

/// Buffer holding the most recently received CC1101 packet
extern uint8_t rf_rx_buf[RF_RX_BUF_SIZE];

/// Number of valid bytes in rf_rx_buf[]
extern uint8_t rf_rx_len;
