/**
 * @file States.cpp
 * @brief Global state variables for the RF → USB receive pipeline.
 *
 * This module provides the storage for the RX state machine defined in
 * States.hpp. No logic is implemented here — only the backing variables
 * used by the RF → USB pipeline.
 *
 * Overview:
 *   - r_state tracks the current state of the RX pipeline
 *   - rf_rx_buf[] holds the most recently received CC1101 packet
 *   - rf_rx_len stores the number of valid bytes in rf_rx_buf[]
 *
 * These variables are modified by the RX state machine implementation
 * (e.g., RF_USB_StateMachine.cpp) and read by the USB transmission layer.
 */

#include "States.hpp"

// -----------------------------------------------------------------------------
// RF → USB state machine state
//
// Initialized to R_STATE_RX_WAIT so the firmware begins in the idle
// receive‑waiting state. The state machine transitions through:
//
//   R_STATE_RX_WAIT → R_STATE_RX_READ → R_STATE_USB_TX → R_STATE_RX_WAIT
//
// depending on CC1101 events and USB transmission progress.
// -----------------------------------------------------------------------------
volatile R_State_t r_state = R_STATE_RX_WAIT;

// -----------------------------------------------------------------------------
// RF → USB receive buffer
//
// Holds one complete CC1101 packet as read from the RX FIFO.
// The buffer size (RF_RX_BUF_SIZE = 64) is sufficient for:
//
//   - 1 length byte
//   - Up to 61 bytes payload (CC1101 variable‑length mode)
//   - Safety margin
//
// The RX state machine writes into this buffer; the USB layer reads from it.
// -----------------------------------------------------------------------------
uint8_t rf_rx_buf[RF_RX_BUF_SIZE];

// -----------------------------------------------------------------------------
// Length of the current received packet
//
// rf_rx_len is set by the RX state machine after reading the packet length
// from the CC1101 RX FIFO. It indicates how many bytes in rf_rx_buf[] are
// valid and should be forwarded over USB.
// -----------------------------------------------------------------------------
uint8_t rf_rx_len = 0;
