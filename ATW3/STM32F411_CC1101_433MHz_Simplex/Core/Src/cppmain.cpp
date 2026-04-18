#include "stm32f4xx_hal.h"
#include "usbd_cdc_if.h"

#include "cppmain.hpp"

#include "CC1101.hpp"
#include "CC1101_Arbiter.hpp"
#include "RF_USB_StateMachine.hpp"
#include "USB_RF_StateMachine.hpp"
#include "States_RX.hpp"
#include "States_TX.hpp"

extern TIM_HandleTypeDef htim5;

// Global RX/TX state structures (you already have these)
extern RF_RX_State rf_rx_state;
extern RF_TX_State rf_tx_state;

int cppmain(void)
{
    HAL_Delay(5000);
    HAL_TIM_Base_Start_IT(&htim5);

    // Warm‑up SPI / CC1101
    CC1101_ReadStatus(CC1101_StatusRegister::PARTNUM);
    uint8_t partnum = CC1101_ReadStatus(CC1101_StatusRegister::PARTNUM);
    uint8_t version = CC1101_ReadStatus(CC1101_StatusRegister::VERSION);
    char buffer[64];
    sprintf(buffer, "CC1101: %u %u\r\n", partnum, version);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));

    CC1101_Init();

    CC1101_Arbiter arbiter;

    RF_USB_StateMachine<RF_RX_State> rf_rx_state_machine(rf_rx_state, arbiter);
    USB_RF_StateMachine<RF_TX_State> rf_tx_state_machine(rf_tx_state, arbiter);

    uint32_t last_print = HAL_GetTick();

    while (1)
    {
        if (HAL_GetTick() - last_print > 1000)
        {
            last_print = HAL_GetTick();
            // optional heartbeat
            // const char* hb = ".";
            // CDC_Transmit_FS((uint8_t*)hb, 1);
        }

        rf_rx_state_machine.process();
        rf_tx_state_machine.process();
        // arbiter.status();
    }
}
