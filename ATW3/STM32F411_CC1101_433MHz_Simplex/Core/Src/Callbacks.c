/**
 * @file Callbacks.c
 * @brief Timer and GPIO interrupt callbacks for the USB → RF pipeline.
 *
 * This module provides:
 *   - TIM5 period‑elapsed callback used as the inter‑packet timeout
 *   - Placeholder EXTI callback (reserved for future CC1101 GDO0/GDO2 use)
 *
 * TIM5 is started in cppmain.cpp and restarted by the USB CDC receive callback.
 * When TIM5 expires, it sets rf_tx_state.timeout_elapsed = 1, which signals the
 * USB→RF state machine to snapshot the USB buffer and begin RF transmission.
 */

#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"

#include "cppmain.hpp"
#include "States_RX.h"
#include "States_TX.h"

#ifdef __cplusplus
extern "C" {
#endif

// External timer handle (declared in CubeMX‑generated code)
extern TIM_HandleTypeDef htim5;

// -----------------------------------------------------------------------------
// TIM5 Period Elapsed Callback
//
// TIM5 is used as the inter‑packet timeout for the USB→RF pipeline.
// When it expires:
//
//   - rf_tx_state.timeout_elapsed is set to 1
//   - The timer is stopped (the state machine will restart it as needed)
//
// The RF state machine checks this flag in U_STATE_ACCUM.
// -----------------------------------------------------------------------------
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5)
    {
    	rf_tx_state.timeout_elapsed = 1;
        HAL_TIM_Base_Stop_IT(htim);
    }
}

// -----------------------------------------------------------------------------
// GPIO EXTI Callback (unused)
//
// This project does not currently use GPIO interrupts for CC1101 events.
// The CC1101 TX path is fully polled via MARCSTATE.
//
// This function is provided for completeness and future expansion:
//   - GDO0: packet received
//   - GDO2: sync detect / end‑of‑packet
// -----------------------------------------------------------------------------
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // Only GDO0 is relevant: sync word detected → packet incoming
    if (GPIO_Pin == GD2_Pin)
    {
        rf_rx_state.state = R_STATE_RX_READ;
        //HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
}

extern USBD_HandleTypeDef hUsbDeviceFS;

volatile uint32_t usb_rx_calls = 0;
volatile uint32_t usb_rx_bytes = 0;
volatile uint32_t usb_irq_counter = 0;

int8_t User_CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
	usb_irq_counter++;
	usb_rx_calls++;
	usb_rx_bytes += *Len;

	// Only accumulate when not in TX pipeline
	if (rf_tx_state.state == U_STATE_IDLE || rf_tx_state.state == U_STATE_ACCUM)
	{
		for (uint32_t i = 0; i < *Len && rf_tx_state.current_buffer_length < USB_RF_BUF_SIZE; i++)
			rf_tx_state.buffer[rf_tx_state.current_buffer_length++] = Buf[i];

		if (rf_tx_state.current_buffer_length > 0)
		{
			rf_tx_state.state = U_STATE_ACCUM;

			// (Re)start inter‑packet timeout
			rf_tx_state.timeout_elapsed = 0;
			HAL_TIM_Base_Stop_IT(&htim5);
			__HAL_TIM_SET_COUNTER(&htim5, 0);
			HAL_TIM_Base_Start_IT(&htim5);
		}
	}
	// else: we are in TX_PREP / TX_WAIT → drop or later extend with a second buffer

	USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
	USBD_CDC_ReceivePacket(&hUsbDeviceFS);
	return USBD_OK;
}


#ifdef __cplusplus
}
#endif
