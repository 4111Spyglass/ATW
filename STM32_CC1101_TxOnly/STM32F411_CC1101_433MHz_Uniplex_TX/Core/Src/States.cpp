/**
 * @file States.cpp
 * @brief Global state variables for the USB → RF transmit pipeline.
 *
 * These variables are shared between:
 *   - The USB CDC receive callback (which fills usb_rf_buf[])
 *   - The inter‑packet timeout timer callback (TIM5)
 *   - The USB → RF state machine (RF_USB_StateMachine.cpp)
 *
 * All variables here are intentionally global because:
 *   - They are accessed from both IRQ and main‑loop context
 *   - They represent the state of the entire TX pipeline
 *   - They must persist across state‑machine iterations
 */

#include "States.hpp"

// -----------------------------------------------------------------------------
// Current USB → RF state machine state.
// Starts in IDLE and transitions through:
//
//   IDLE → ACCUM → TX_PREP → TX_WAIT → IDLE
//
// Declared volatile because it is modified from both IRQ and main loop.
// -----------------------------------------------------------------------------
volatile U_State_t u_state = U_STATE_IDLE;

// -----------------------------------------------------------------------------
// Accumulation buffer filled by the USB CDC receive callback.
// The RF state machine snapshots this buffer before transmission.
// -----------------------------------------------------------------------------
uint8_t usb_rf_buf[USB_RF_BUF_SIZE];

// -----------------------------------------------------------------------------
// Number of valid bytes currently stored in usb_rf_buf[].
// Reset to zero after snapshotting in TX_PREP.
// -----------------------------------------------------------------------------
uint16_t usb_rf_len = 0;

// -----------------------------------------------------------------------------
// Inter‑packet timeout flag.
// Set to 1 by TIM5 when the timeout expires.
// Cleared by the state machine when TX begins.
// -----------------------------------------------------------------------------
volatile uint8_t usb_timeout_elapsed = 0;
