/**
 * @file Callbacks.c
 * @brief Timer and GPIO interrupt callbacks for the USB → RF pipeline.
 *
 * This module provides:
 *   - TIM5 period‑elapsed callback used as the inter‑packet timeout
 *   - Placeholder EXTI callback (reserved for future CC1101 GDO0/GDO2 use)
 *
 * TIM5 is started in cppmain.cpp and restarted by the USB CDC receive callback.
 * When TIM5 expires, it sets usb_timeout_elapsed = 1, which signals the
 * USB→RF state machine to snapshot the USB buffer and begin RF transmission.
 */

#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"

#include "cppmain.hpp"
#include "USB_RF_StateMachine.hpp"
#include "States.hpp"

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
//   - usb_timeout_elapsed is set to 1
//   - The timer is stopped (the state machine will restart it as needed)
//
// The RF state machine checks this flag in U_STATE_ACCUM.
// -----------------------------------------------------------------------------
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5)
    {
        usb_timeout_elapsed = 1;
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
    // Reserved for future RX‑side or GDO‑based event handling
}

#ifdef __cplusplus
}
#endif
