/*
 * CC1101Manager.hpp
 *
 *  Created on: Apr 16, 2026
 *      Author: mgi
 */

#pragma once

#include "CC1101.hpp"
#include "RingBuffer.hpp"
#include "usbd_cdc_if.h"

enum class RadioState
{
    INIT,
    RX_LISTENING,
    RX_PROCESSING,
    TX_START,
    TX_WAIT_END
};

template <typename RADIO>
class CC1101Manager
{
private:
    RADIO &radio;
    RadioState state = RadioState::INIT;
    RingBuffer<512> tx_ring;

    uint8_t rf_packet_buf[64];
    uint8_t usb_tx_buf[128];

    volatile bool packet_received_flag = false;

    // Helper: Force radio back to a clean RX state
    void force_rx()
    {
        radio.Strobe(RADIO::StrobeCommand::SIDLE);
        radio.Strobe(RADIO::StrobeCommand::SFRX);
        radio.Strobe(RADIO::StrobeCommand::SRX);
        state = RadioState::RX_LISTENING;
    }

public:
    CC1101Manager(RADIO &radio) : radio(radio) {};

    void handle_usb_data(uint8_t *buf, uint32_t len)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            tx_ring.push(buf[i]);
        }
    }

    void notify_packet_received()
    {
        packet_received_flag = true;
    }

    void process()
    {
        switch (state)
        {
        case RadioState::INIT:
            radio.Init();
            force_rx();
            break;

        case RadioState::RX_LISTENING:
            // 1. Check for hardware errors (Overflow)
            if ((radio.ReadStatus(RADIO::StatusRegister::MARCSTATE) & 0x1F) == 0x11)
            {
                force_rx();
                return;
            }

            // 2. Prioritize incoming RF packets
            if (packet_received_flag)
            {
                packet_received_flag = false;
                state = RadioState::RX_PROCESSING;
                return;
            }

            // 3. If air is clear and we have USB data, start TX
            // Check GD2: If HIGH, a sync word was found, packet is incoming.
            if (tx_ring.count() > 0 && HAL_GPIO_ReadPin(GPIOB, GD2_Pin) == GPIO_PIN_RESET)
            {
                state = RadioState::TX_START;
            }
            break;

        case RadioState::RX_PROCESSING:
            handle_rf_read();
            force_rx();
            break;

        case RadioState::TX_START:
            handle_rf_tx();
            break;

        case RadioState::TX_WAIT_END:
            // Wait for radio to return to IDLE after packet sent
            uint8_t marc = radio.ReadStatus(RADIO::StatusRegister::MARCSTATE) & 0x1F;
            if (marc == 0x01)
            { // IDLE
                force_rx();
            }
            else if (marc == 0x16)
            { // TX_UNDERFLOW
                radio.Strobe(RADIO::StrobeCommand::SFTX);
                force_rx();
            }
            break;
        }
    }

private:
    void handle_rf_read()
    {
        uint8_t rxStatus = radio.ReadStatus(RADIO::StatusRegister::RXBYTES);
        uint8_t bytesInFifo = rxStatus & 0x7F;

        if (rxStatus & 0x80 || bytesInFifo < 3)
            return; // Overflow or ghost trigger

        radio.ReadBurst(RADIO::BurstRegister::RXFIFO, rf_packet_buf, bytesInFifo);

        uint8_t pktLen = rf_packet_buf[0];
        bool crc_ok = (rf_packet_buf[pktLen + 2] & 0x80);

        if (crc_ok && pktLen > 0 && pktLen <= 61)
        {
            // Format and send to USB
            int8_t rssi_dbm = (rf_packet_buf[pktLen + 1] >= 128)
                                  ? (int8_t)((rf_packet_buf[pktLen + 1] - 256) / 2) - 74
                                  : (int8_t)(rf_packet_buf[pktLen + 1] / 2) - 74;

            uint8_t lqi = rf_packet_buf[pktLen + 2] & 0x7F;

            int len = snprintf((char *)usb_tx_buf, sizeof(usb_tx_buf),
                               "%.*s [RSSI:%d LQI:%d]\r\n", pktLen, &rf_packet_buf[1], rssi_dbm, lqi);

            // Block briefly to ensure USB delivery
            while (CDC_Transmit_FS(usb_tx_buf, len) == USBD_BUSY)
                ;
        }
    }

    void handle_rf_tx()
    {
        uint8_t payload[64];
        uint8_t len = 0;

        // Pull up to 61 bytes from ring buffer
        while (len < 61 && tx_ring.pop(payload[len + 1]))
        {
            len++;
        }

        if (len > 0)
        {
            payload[0] = len; // Length byte
            radio.Strobe(RADIO::StrobeCommand::SIDLE);
            radio.Strobe(RADIO::StrobeCommand::SFTX);
            radio.WriteBurst(RADIO::BurstRegister::TXFIFO, payload, len + 1);
            radio.Strobe(RADIO::StrobeCommand::STX);
            state = RadioState::TX_WAIT_END;
        }
        else
        {
            state = RadioState::RX_LISTENING;
        }
    }
};
