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

RF_RX_State rf_rx_state = {
    .state = R_STATE_RX_WAIT,
    .buffer = {0},
    .current_buffer_length = 0
};
