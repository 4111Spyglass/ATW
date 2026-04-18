// USB_RF_StateMachine

#pragma once

#include "States_TX.hpp"
#include "CC1101.hpp"
#include "CC1101_Arbiter.hpp"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"
#include <cstring>
#include <cstdio>

template <typename RF_TX_STATE>
class USB_RF_StateMachine
{
private:
    RF_TX_STATE &tx_state_;
    CC1101_Arbiter &arbiter_;

    uint8_t tx_snapshot[sizeof(tx_state_.buffer) + 1];
    uint8_t tx_snapshot_len = 0;

public:
    USB_RF_StateMachine(RF_TX_STATE &tx_state, CC1101_Arbiter &arbiter)
        : tx_state_(tx_state), arbiter_(arbiter) {}

    void process();

private:
    void dbg_print(const char* msg);

    void handle_idle();
    void handle_accum();
    void handle_tx_prep();
    void handle_tx_wait();

    void snapshot_accumulated_bytes();
    void prepare_radio_for_tx();
    bool wait_for_idle(uint32_t timeout_ms);
    void start_rf_tx();
};

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::process()
{
//	 char buf[64];
//	 sprintf(buf,
//            "[TXSM] state=%d len=%u timeout=%u\n",
//            tx_state_.state,
//            tx_state_.current_buffer_length,
//            tx_state_.timeout_elapsed);
//    dbg_print(buf);

	switch (tx_state_.state)
    {
        case U_STATE_IDLE:     handle_idle();     break;
        case U_STATE_ACCUM:    handle_accum();    break;
        case U_STATE_TX_PREP:  handle_tx_prep();  break;
        case U_STATE_TX_WAIT:  handle_tx_wait();  break;

        default:
            dbg_print("USB_RF: invalid state\n");
            tx_state_.state = U_STATE_IDLE;
            break;
    }
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::handle_idle()
{
    // Nothing to do — USB IRQ will move us to ACCUM
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::handle_accum()
{
    if (tx_state_.current_buffer_length > 0 && tx_state_.timeout_elapsed)
    {
        snapshot_accumulated_bytes();

        char buffer[64];
        sprintf(buffer, "TX_START len=%u\n", tx_snapshot_len);
        dbg_print(buffer);

        tx_state_.state = U_STATE_TX_PREP;
    }
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::handle_tx_prep()
{
    if (tx_snapshot_len == 0)
    {
        dbg_print("TX_PREP: zero length, abort\n");
        tx_state_.state = U_STATE_IDLE;
        return;
    }

    // Try to own the radio for TX
    if (!arbiter_.request_tx()) {
        // RX currently owns it; try again next tick
        return;
    }

    // Pre‑TX diagnostics
    {
        uint8_t marc = arbiter_.read_status(CC1101_StatusRegister::MARCSTATE) & 0x1F;
        char buffer[128];
        sprintf(buffer, "PRE TX MARCSTATE=%02X, TX len=%u\n", marc, tx_snapshot_len);
        dbg_print(buffer);
    }

    prepare_radio_for_tx();
    start_rf_tx();

    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    tx_state_.state = U_STATE_TX_WAIT;
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::handle_tx_wait()
{
    uint8_t marc = arbiter_.read_status(CC1101_StatusRegister::MARCSTATE) & 0x1F;

    if (marc == static_cast<uint8_t>(CC1101_MARCState::IDLE))  // TX complete
    {
        dbg_print("TX_DONE\n");
        arbiter_.release();          // release radio
        tx_state_.state = U_STATE_IDLE;
        return;
    }

    if (marc == static_cast<uint8_t>(CC1101_MARCState::TXFIFO_UNDERFLOW))
    {
        dbg_print("TX_UNDERFLOW\n");
        arbiter_.strobe(CC1101_StrobeCommand::SIDLE);
        arbiter_.strobe(CC1101_StrobeCommand::SFTX);
        arbiter_.release();
        tx_state_.state = U_STATE_IDLE;
        return;
    }

    // Otherwise remain in TX_WAIT
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::snapshot_accumulated_bytes()
{
    tx_snapshot_len = tx_state_.current_buffer_length;
    if (tx_snapshot_len > 61)
        tx_snapshot_len = 61;

    tx_snapshot[0] = tx_snapshot_len;
    memcpy(&tx_snapshot[1], tx_state_.buffer, tx_snapshot_len);

    tx_state_.current_buffer_length = 0;
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::prepare_radio_for_tx()
{
    arbiter_.strobe(CC1101_StrobeCommand::SIDLE);

    wait_for_idle(5);

    arbiter_.strobe(CC1101_StrobeCommand::SFTX);
    arbiter_.write_burst(CC1101_BurstRegister::TXFIFO, tx_snapshot, tx_snapshot_len + 1);

    uint8_t txbytes = arbiter_.read_status(CC1101_StatusRegister::TXBYTES);
    char buffer[64];
    sprintf(buffer, "TXBYTES=%02X\n", txbytes);
    dbg_print(buffer);
}

template <typename RF_TX_STATE>
bool USB_RF_StateMachine<RF_TX_STATE>::wait_for_idle(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (1)
    {
        uint8_t marc = arbiter_.read_status(CC1101_StatusRegister::MARCSTATE) & 0x1F;
        if (marc == static_cast<uint8_t>(CC1101_MARCState::IDLE)) return true;
        if (HAL_GetTick() - t0 > timeout_ms) return false;
    }
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::start_rf_tx()
{
    arbiter_.strobe(CC1101_StrobeCommand::STX);
}

template <typename RF_TX_STATE>
void USB_RF_StateMachine<RF_TX_STATE>::dbg_print(const char* msg)
{
    // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
}
