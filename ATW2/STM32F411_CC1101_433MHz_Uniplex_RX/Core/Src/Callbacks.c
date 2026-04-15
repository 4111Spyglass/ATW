// Callbacks.c

#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"

#include "cppmain.hpp"
#include "RF_USB_StateMachine.hpp"
#include "States.hpp"

#ifdef __cplusplus
extern "C" {
#endif

extern USBD_HandleTypeDef hUsbDeviceFS;

uint8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    // RX-only node: ignore USB OUT packets
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // RX-only node: no timers used
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // Only GDO0 is relevant: sync word detected → packet incoming
    if (GPIO_Pin == GD0_Pin)
    {
        r_state = R_STATE_RX_READ;
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
}

#ifdef __cplusplus
}
#endif
