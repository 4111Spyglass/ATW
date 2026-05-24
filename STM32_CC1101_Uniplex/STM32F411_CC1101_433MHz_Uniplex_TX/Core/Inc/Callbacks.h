/**
 * @file Callbacks.h
 * @brief Declarations for interrupt callback handlers used in the USB → RF pipeline.
 *
 * This header exists to provide forward declarations for callback functions
 * implemented in Callbacks.c. These callbacks are invoked by the STM32 HAL:
 *
 *   - HAL_TIM_PeriodElapsedCallback()
 *       Used for the inter‑packet timeout (TIM5)
 *
 *   - HAL_GPIO_EXTI_Callback()
 *       Reserved for future CC1101 GDO0/GDO2 interrupt handling
 *
 * No user‑defined state is stored here; all pipeline state lives in States.hpp.
 */

#pragma once

#include "stdint.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Timer interrupt callback (implemented in Callbacks.c).
 *
 * TIM5 is used as the inter‑packet timeout for the USB→RF pipeline.
 * When the timer expires, usb_timeout_elapsed is set to 1.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/**
 * @brief GPIO EXTI callback (implemented in Callbacks.c).
 *
 * Currently unused. Reserved for future CC1101 GDO0/GDO2 interrupt support.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

extern volatile uint32_t usb_rx_calls;
extern volatile uint32_t usb_rx_bytes;
extern volatile uint32_t usb_irq_counter;

int8_t User_CDC_Receive_FS(uint8_t* Buf, uint32_t *Len);

#ifdef __cplusplus
}
#endif
