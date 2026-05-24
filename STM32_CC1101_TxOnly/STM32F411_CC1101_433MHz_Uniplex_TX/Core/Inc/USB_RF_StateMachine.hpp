/**
 * @file USB_RF_StateMachine.hpp
 * @brief Interface for the USB → RF transmit state machine.
 *
 * This module implements a deterministic, non‑blocking pipeline that:
 *   - Accumulates bytes arriving from USB CDC (via IRQ callback)
 *   - Emits a single RF packet after an inter‑packet timeout
 *   - Uses a snapshot buffer to avoid races with the USB IRQ
 *   - Transmits via CC1101 in variable‑length packet mode
 *
 * The state machine must be called frequently from the main loop.
 * It performs no blocking operations and is safe to call at high rate.
 *
 * States are defined in States.hpp:
 *      U_STATE_IDLE
 *      U_STATE_ACCUM
 *      U_STATE_TX_PREP
 *      U_STATE_TX_WAIT
 *
 * Only TX functionality is implemented. RX can be added later.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Execute one step of the USB → RF transmit state machine.
 *
 * This function:
 *   - Checks whether USB data has accumulated
 *   - Checks whether the inter‑packet timeout has expired
 *   - Snapshots the USB buffer when ready
 *   - Prepares and transmits a CC1101 RF packet
 *   - Polls MARCSTATE until TX completes
 *
 * It must be called repeatedly from the main loop.
 * It performs no blocking operations and is IRQ‑safe.
 */
void USB_RF_StateMachine(void);

#ifdef __cplusplus
}
#endif
