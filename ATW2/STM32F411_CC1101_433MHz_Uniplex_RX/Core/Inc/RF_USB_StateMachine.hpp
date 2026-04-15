#pragma once

#include "States.hpp"
#include "CC1101.hpp"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"

template <typename RF_RX_STATE>
class RF_USB_StateMachine
{
private:
    RF_RX_STATE &rx_state_;

public:
    RF_USB_StateMachine(RF_RX_STATE &rx_state);
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
RF_USB_StateMachine<RF_RX_STATE>::RF_USB_StateMachine(RF_RX_STATE &rx_state) : rx_state_(rx_state) {}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::process()
{
    switch (rf_rx_state.state)
    {
        case R_STATE_RX_WAIT:  handle_rx_wait();  break;
        case R_STATE_RX_READ:  handle_rx_read();  break;
        case R_STATE_USB_TX:   handle_usb_tx();   break;
    }
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::handle_rx_wait()
{
    uint8_t marc = CC1101_ReadStatus(CC1101_StatusRegister::MARCSTATE) & 0x1F;

    if (marc == 0x11) { // RX_OVERFLOW
        CC1101_Strobe(CC1101_StrobeCommand::SIDLE);
        CC1101_Strobe(CC1101_StrobeCommand::SFRX);
        CC1101_Strobe(CC1101_StrobeCommand::SRX);
        return;
    }

    if (marc != 0x0D) { // Not in RX
        CC1101_Strobe(CC1101_StrobeCommand::SRX);
    }
}

template <typename RF_RX_STATE>
bool RF_USB_StateMachine<RF_RX_STATE>::read_fifo_status(uint8_t &bytesInFifo)
{
    uint8_t rxStatus = CC1101_ReadStatus(CC1101_StatusRegister::RXBYTES);
    bytesInFifo = rxStatus & 0x7F;

    if (rxStatus & 0x80) { // Overflow
        dbg_print("read overflow failure\n\r");
        flush_and_restart_rx();
        return false;
    }

    if (bytesInFifo < 4) {
        rf_rx_state.state = R_STATE_RX_WAIT;
        return false;
    }

    return true;
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::flush_and_restart_rx()
{
    CC1101_Strobe(CC1101_StrobeCommand::SIDLE);
    CC1101_Strobe(CC1101_StrobeCommand::SFRX);
    CC1101_Strobe(CC1101_StrobeCommand::SRX);
    rf_rx_state.state = R_STATE_RX_WAIT;
}

template <typename RF_RX_STATE>
bool RF_USB_StateMachine<RF_RX_STATE>::validate_packet_length(uint8_t pktLen, uint8_t bytesInFifo)
{
    if (pktLen == 0 || pktLen > 61 || bytesInFifo < pktLen + 3) {
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
        (char*)rf_rx_state.buffer, RF_RX_BUF_SIZE,
        "%.*s [RSSI:%d LQI:%d]\r\n",
        pLen, &raw[1], rssi_dbm, lqi
    );

    rf_rx_state.current_buffer_length = (formatted_len > 0)
                                        ? (uint8_t)formatted_len
                                        : 0;

    rf_rx_state.state = R_STATE_USB_TX;
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::handle_rx_read()
{
    uint8_t bytesInFifo;
    if (!read_fifo_status(bytesInFifo))
        return;

    uint8_t raw[64];
    CC1101_ReadBurst(CC1101_BurstRegister::RXFIFO, raw, bytesInFifo);

    uint8_t pktLen = raw[0];
    if (!validate_packet_length(pktLen, bytesInFifo))
        return;

    bool crc_ok = (raw[pktLen + 2] & 0x80) != 0;
    if (!crc_ok) {
        flush_and_restart_rx();
        return;
    }

    format_payload_and_metrics(raw, pktLen);
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::handle_usb_tx()
{
    if (rf_rx_state.current_buffer_length == 0) {
        rf_rx_state.state = R_STATE_RX_WAIT;
        return;
    }

    if (CDC_Transmit_FS(rf_rx_state.buffer, rf_rx_state.current_buffer_length) == 0) {
        rf_rx_state.current_buffer_length = 0;
        CC1101_Strobe(CC1101_StrobeCommand::SRX);
        rf_rx_state.state = R_STATE_RX_WAIT;
    }
}

template <typename RF_RX_STATE>
void RF_USB_StateMachine<RF_RX_STATE>::dbg_print(const char* msg)
{
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
}
