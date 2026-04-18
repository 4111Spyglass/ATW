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
#include "CC1101Manager.hpp"
#include "GpioPin.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Same aliases as in cppmain.cpp
using CC1101_GPIO      = GpioPin<GPIOA_BASE, SPI1_CS_Pin>;
using CC1101_T         = CC1101<CC1101_GPIO>;
using CC1101Manager_T  = CC1101Manager<CC1101_T>;

// These are defined in cppmain.cpp
extern CC1101_T cc1101;
extern CC1101Manager_T radio_manager;

extern USBD_HandleTypeDef hUsbDeviceFS;

// GD2 Pin is configured as EXTI Falling Edge
// CC1101 IOCFG2 = 0x06 (Sync word detect -> End of packet)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GD2_Pin) {
        radio_manager.notify_packet_received();
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
}

int8_t User_CDC_Receive_FS(uint8_t* Buf, uint32_t *Len) {
    radio_manager.handle_usb_data(Buf, *Len);

    // Always keep USB ready to receive more
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

#ifdef __cplusplus
}
#endif
