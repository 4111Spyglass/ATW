// RF_USB_StateMachine

#pragma once

#include "States_RX.hpp"
#include "CC1101.hpp"
#include "CC1101_Arbiter.hpp"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"
#include <cstring>
#include <cstdio>

template <typename RF_RX_STATE>
class RF_USB_StateMachine
{
private:
    RF_RX_STATE &rx_state_;
    CC1101_Arbiter &arbiter_;

public:
    RF_USB_StateMachine(RF_RX_STATE &rx_state, CC1101_Arbiter &arbiter)
        : rx_state_(rx_state), arbiter_(arbiter) {}

    void process();

private:
    void dbg_print(const char* msg);

    void handle_rx_wait();
    void handle_rx_read();
    void handle_usb_tx();

    bool read_fifo_status(uint8_t &bytesInFifo);
    void flush_and_restart_rx();
    bool validate_packet_length(uint8_t pktLen, uint8_t bytesInFifo);
    void format_payload_and_metrics(uint8_t *raw, uint8_t pktLen);
};

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::process()
{
	// dbg_print("RF_USB_StateMachine<RF_RX_STATE>::process()\n\r");

	switch (rx_state_.state)
    {
        case R_STATE_RX_WAIT:  handle_rx_wait();  break;
        case R_STATE_RX_READ:  handle_rx_read();  break;
        case R_STATE_USB_TX:   handle_usb_tx();   break;
        default:
            rx_state_.state = R_STATE_RX_WAIT;
            break;
    }
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::handle_rx_wait()
{
    // Try to own the radio for RX
    if (!arbiter_.request_rx()) {
        // Someone else (TX) owns it; just wait

        return;
    }

//    char dbg[256];
//    snprintf(dbg, sizeof(dbg), "RX: handle_rx_wait: %d %d \n\r", rx_state_.state, rx_state_.current_buffer_length );
//    dbg_print(dbg);

    uint8_t marc = arbiter_.read_status(CC1101_StatusRegister::MARCSTATE) & 0x1F;

    if (marc == static_cast<uint8_t>(CC1101_MARCState::RXFIFO_OVERFLOW)) {
        arbiter_.strobe(CC1101_StrobeCommand::SIDLE);
        arbiter_.strobe(CC1101_StrobeCommand::SFRX);
        arbiter_.strobe(CC1101_StrobeCommand::SRX);
        return;
    }

    if (marc != static_cast<uint8_t>(CC1101_MARCState::RX)) {
        arbiter_.strobe(CC1101_StrobeCommand::SRX);
        return;
    }

    // At this point we’re in RX and waiting for GDO0/IRQ to move us to R_STATE_RX_READ
}

template <typename RF_RX_STATE>
bool RF_USB_StateMachine<RF_RX_STATE>::read_fifo_status(uint8_t &bytesInFifo)
{
    uint8_t rxStatus = arbiter_.read_status(CC1101_StatusRegister::RXBYTES);
    bytesInFifo = rxStatus & 0x7F;

    if (rxStatus & RXFIFO_OVERFLOW) {
        dbg_print("RX: overflow\n\r");
        flush_and_restart_rx();
        return false;
    }

    if (bytesInFifo < 4) {
        rx_state_.state = R_STATE_RX_WAIT;
        return false;
    }

    return true;
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::flush_and_restart_rx()
{
    arbiter_.strobe(CC1101_StrobeCommand::SIDLE);
    arbiter_.strobe(CC1101_StrobeCommand::SFRX);
    arbiter_.strobe(CC1101_StrobeCommand::SRX);
    rx_state_.state = R_STATE_RX_WAIT;
}

template <typename RF_RX_STATE>
bool RF_USB_StateMachine<RF_RX_STATE>::validate_packet_length(uint8_t pktLen, uint8_t bytesInFifo)
{
    if (pktLen == 0 || pktLen > 61 || bytesInFifo < pktLen + 3) {
        dbg_print("RX: invalid length\n\r");
        flush_and_restart_rx();
        return false;
    }
    return true;
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::format_payload_and_metrics(uint8_t *raw, uint8_t pktLen)
{
    uint8_t pLen = pktLen;
    while (pLen > 0 && (raw[pLen] == '\r' || raw[pLen] == '\n' || raw[pLen] == ' '))
        pLen--;

    uint8_t rssi_raw = raw[pktLen + 1];
    int8_t rssi_dbm = (rssi_raw >= 128)
                        ? (int8_t)((rssi_raw - 256) / 2) - 74
                        : (int8_t)(rssi_raw / 2) - 74;

    uint8_t lqi = raw[pktLen + 2] & 0x7F;

    int formatted_len = snprintf(
        (char*)rx_state_.buffer, RF_RX_BUF_SIZE,
        "%.*s [RSSI:%d LQI:%d]\r\n",
		pLen, &raw[1], rssi_dbm, lqi
    );

    rx_state_.current_buffer_length = (formatted_len > 0)
                                        ? (uint8_t)formatted_len
                                        : 0;

    rx_state_.state = R_STATE_USB_TX;
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::handle_rx_read()
{
    // We assume RX already owns the radio (requested in RX_WAIT)
    if (arbiter_.owner() != CC1101_Arbiter::Owner::RX) {
        // Lost ownership unexpectedly; go back to wait
        rx_state_.state = R_STATE_RX_WAIT;
        return;
    }

//    char dbg[256];
//    snprintf(dbg, sizeof(dbg), "RX: handle_rx_read: %d %d \n\r", rx_state_.state, rx_state_.current_buffer_length );
//    dbg_print(dbg);

    uint8_t bytesInFifo;
    if (!read_fifo_status(bytesInFifo))
        return;

    uint8_t raw[64];
    arbiter_.read_burst(CC1101_BurstRegister::RXFIFO, raw, bytesInFifo);

    uint8_t pktLen = raw[0];
    if (!validate_packet_length(pktLen, bytesInFifo))
        return;

    bool crc_ok = (raw[pktLen + 2] & CRC_OK) != 0;
    if (!crc_ok) {
        dbg_print("RX: CRC fail\n\r");
        flush_and_restart_rx();
        return;
    }

    format_payload_and_metrics(raw, pktLen);
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::handle_usb_tx()
{
//    char dbg[256];
//    snprintf(dbg, sizeof(dbg), "RX: handle_rx_read: %d %d \n\r", rx_state_.state, rx_state_.current_buffer_length );
//    dbg_print(dbg);

	if (rx_state_.current_buffer_length == 0) {
        rx_state_.state = R_STATE_RX_WAIT;
        return;
    }

    if (CDC_Transmit_FS(rx_state_.buffer, rx_state_.current_buffer_length) == USBD_OK) {
        rx_state_.current_buffer_length = 0;
        arbiter_.strobe(CC1101_StrobeCommand::SRX);
        rx_state_.state = R_STATE_RX_WAIT;
    }
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::dbg_print(const char* msg)
{
    // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
}
