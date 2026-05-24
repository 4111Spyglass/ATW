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

RF_TX_State rf_tx_state = {
    .state = U_STATE_IDLE,
    .current_buffer_length = 0,
	.timeout_elapsed = 0
};
