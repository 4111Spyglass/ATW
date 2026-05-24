/**
 * @file cppmain.cpp
 * @brief Main application entry point for the CC1101 USB→RF bridge.
 *
 * Responsibilities:
 *   - Initialize CC1101 and verify PARTNUM / VERSION
 *   - Start the inter‑packet timeout timer (TIM5)
 *   - Periodically print USB statistics for debugging
 *   - Run the USB→RF transmit state machine
 *
 * The main loop is intentionally simple and non‑blocking.
 */

#include <Callbacks.h>
#include "cppmain.hpp"
#include "CC1101.hpp"
#include "States.hpp"
#include "USB_RF_StateMachine.hpp"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"

// External timer handle (declared in CubeMX‑generated code)
extern TIM_HandleTypeDef htim5;

int cppmain(void)
{
    // Allow USB enumeration to complete before printing anything
    HAL_Delay(10000);

    // Start inter‑packet timeout timer (TIM5)
    HAL_TIM_Base_Start_IT(&htim5);

    // -------------------------------------------------------------------------
    // Read CC1101 PARTNUM and VERSION (status registers)
    //
    // These registers are accessed via the "status register" read format:
    //   addr | 0x80 (read) | 0x40 (burst/status)
    //
    // PARTNUM should be 0x00 for CC1101
    // VERSION varies by silicon revision (e.g., 0x14, 0x04, etc.)
    // -------------------------------------------------------------------------
	CC1101_ReadStatus(CC1101_StatusRegister::PARTNUM);
	uint8_t partnum = CC1101_ReadStatus(CC1101_StatusRegister::PARTNUM);
	uint8_t version = CC1101_ReadStatus(CC1101_StatusRegister::VERSION);

    char buffer[128];
    sprintf(buffer, "CC1101: PARTNUM=%u VERSION=%u\n", partnum, version);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));

    // -------------------------------------------------------------------------
    // Initialize CC1101 radio (reset + register configuration + FIFO flush)
    // -------------------------------------------------------------------------
    CC1101_Init();

    // -------------------------------------------------------------------------
    // Main loop
    //
    // Tasks:
    //   - Periodically print USB statistics
    //   - Run the USB→RF state machine
    //
    // The state machine is non‑blocking and safe to call at high frequency.
    // -------------------------------------------------------------------------
    static uint32_t last_print = 0;
    USB_RF_StateMachine<RF_TX_State> rf_tx_state_machine(rf_tx_state);

    while (1)
    {
        // Print debug statistics once per second
        if (HAL_GetTick() - last_print > 1000)
        {
            last_print = HAL_GetTick();

            char dbg[128];
            sprintf(dbg,
                    "USB: irq=%lu calls=%lu bytes=%lu len=%u state=%u\n",
                    usb_irq_counter,
                    usb_rx_calls,
                    usb_rx_bytes,
					rf_tx_state.current_buffer_length,
					rf_tx_state.state);

            CDC_Transmit_FS((uint8_t*)dbg, strlen(dbg));
        }

        // Execute one step of the USB→RF transmit state machine
        rf_tx_state_machine.process();
    }
}
