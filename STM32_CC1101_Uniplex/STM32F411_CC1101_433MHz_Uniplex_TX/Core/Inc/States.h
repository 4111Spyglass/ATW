#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// USB → RF accumulation buffer
//
// usb_rf_buf[] is filled by the USB CDC receive callback.
// The RF state machine snapshots this buffer before transmission.
//
// MTU Size = 64 bytes:
//	 - one Length byte — always the first byte of a packet
//   - 61 bytes for payload
//	 - 1 byte for RSSI byte — appended by the modem after the payload
//   - 1 byte for LQI/CRC byte — appended after RSSI
// -----------------------------------------------------------------------------
#define USB_RF_BUF_SIZE 61

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

typedef struct {
    volatile U_State_t state;
    uint8_t buffer[USB_RF_BUF_SIZE];
    uint16_t current_buffer_length;
    volatile uint8_t timeout_elapsed;
} RF_TX_State;

extern RF_TX_State rf_tx_state;

#ifdef __cplusplus
}
#endif
