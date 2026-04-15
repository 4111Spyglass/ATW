#include "RF_USB_StateMachine.hpp"
#include "States.hpp"
#include "CC1101.hpp"
#include "usbd_cdc_if.h"
#include <cstring>

// Note: Ensure CC1101_ReadStatus is available or use (addr | 0xC0) in ReadReg
#define MARCSTATE 0x35
#define RXBYTES   0x3B

// -----------------------------------------------------------------------------
// Debug printing helper (non‑blocking enough for low‑rate logging)
// -----------------------------------------------------------------------------
static void dbg_print(const char* msg)
{
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
}

void RF_USB_StateMachine(void)
{
    switch (r_state)
    {
    case R_STATE_RX_WAIT:
    {
        // Use 0xF5 (0x35 | 0xC0) to read MARCSTATE status register
        uint8_t marc = CC1101_ReadStatusReg(MARCSTATE) & 0x1F;

        if (marc == 0x11) { // RX_OVERFLOW
            CC1101_Strobe(CC1101_StrobeCmd::SIDLE);
            CC1101_Strobe(CC1101_StrobeCmd::SFRX);
            CC1101_Strobe(CC1101_StrobeCmd::SRX);
        }
        else if (marc != 0x0D) { // Not in RX
            CC1101_Strobe(CC1101_StrobeCmd::SRX);
        }
        break;
    }

    case R_STATE_RX_READ:
    {
        // 1) Check RXBYTES (Address 0x3B | 0x40 for status read)
        uint8_t rxStatus = CC1101_ReadStatusReg(RXBYTES);
        uint8_t bytesInFifo = rxStatus & 0x7F;

        // 2) Check for Overflow (Bit 7)
        if (rxStatus & 0x80) {
        	dbg_print("read overflow failure\n\r");
        	CC1101_Strobe(CC1101_StrobeCmd::SIDLE);
            CC1101_Strobe(CC1101_StrobeCmd::SFRX);
            CC1101_Strobe(CC1101_StrobeCmd::SRX);
            r_state = R_STATE_RX_WAIT;
            break;
        }

        if (bytesInFifo < 4) { // Min: Len(1) + Payload(1) + Status(2)
        	//dbg_print("read bytesInFifo < 4 failure\n\r");
        	r_state = R_STATE_RX_WAIT;
            break;
        }

        // 3) Read the whole chunk from FIFO
        uint8_t raw[64];
        CC1101_ReadBurst(0x3F, raw, bytesInFifo);

        uint8_t pktLen = raw[0];

        // 4) Basic length validation
        if (pktLen == 0 || pktLen > 61 || (pktLen + 2) >= bytesInFifo) {
        	// dbg_print("read pkLen failure\n\r");
        	CC1101_Strobe(CC1101_StrobeCmd::SIDLE);
            CC1101_Strobe(CC1101_StrobeCmd::SFRX);
            CC1101_Strobe(CC1101_StrobeCmd::SRX);
            r_state = R_STATE_RX_WAIT;
            break;
        }

        // 5) Check CRC (Bit 7 of LQI byte, which is at raw[pktLen + 2])
        bool crc_ok = (raw[pktLen + 2] & 0x80) != 0;

        if (crc_ok) {
            // --- PAYLOAD TRIMMING ---
            // The payload is in raw[1] through raw[pktLen]
            uint8_t pLen = pktLen;
            while (pLen > 0 && (raw[pLen] == '\r' || raw[pLen] == '\n' || raw[pLen] == ' ')) {
                pLen--;
            }

            // --- RSSI CALCULATION ---
            uint8_t rssi_raw = raw[pktLen + 1];
            int8_t rssi_dbm;
            if (rssi_raw >= 128) {
                rssi_dbm = (int8_t)((rssi_raw - 256) / 2) - 74;
            } else {
                rssi_dbm = (int8_t)(rssi_raw / 2) - 74;
            }

            // --- LQI EXTRACTION ---
            uint8_t lqi = raw[pktLen + 2] & 0x7F;

            // --- FORMAT DEBUG STRING ---
            // We use snprintf to build the final string in the transmission buffer
            // %.*s takes the length (pLen) and the pointer to the payload (&raw[1])
            int formatted_len = snprintf((char*)rf_rx_buf, 128,
                                         "%.*s [RSSI:%d LQI:%d]\r\n",
                                         pLen, &raw[1], rssi_dbm, lqi);

            if (formatted_len > 0) {
                rf_rx_len = (uint8_t)formatted_len;
            } else {
                rf_rx_len = 0;
            }

            r_state = R_STATE_USB_TX;
        } else {
        	// dbg_print("read crc failure\n\r");
        	// CRC fail: Flush
            CC1101_Strobe(CC1101_StrobeCmd::SIDLE);
            CC1101_Strobe(CC1101_StrobeCmd::SFRX);
            CC1101_Strobe(CC1101_StrobeCmd::SRX);
            r_state = R_STATE_RX_WAIT;
        }
        break;
    }

    case R_STATE_USB_TX:
        if (rf_rx_len > 0) {
            // USBD_CDC_Transmit_FS returns USBD_OK (0) or USBD_BUSY (1)
            // We loop here until the previous transmission is actually finished
            if (CDC_Transmit_FS(rf_rx_buf, rf_rx_len) == 0) {
                rf_rx_len = 0;
                // Only go back to wait once transmission is accepted
                CC1101_Strobe(CC1101_StrobeCmd::SRX);
                r_state = R_STATE_RX_WAIT;
            }
        } else {
            r_state = R_STATE_RX_WAIT;
        }
        break;
    }
}
