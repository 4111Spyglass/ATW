// cppmain.cpp

#include "cppmain.hpp"
#include "CC1101.hpp"
#include "States.hpp"
#include "RF_USB_StateMachine.hpp"

int cppmain(void)
{
	HAL_Delay(5000);
	HAL_TIM_Base_Start_IT(&htim5);

	CC1101_ReadStatus(CC1101_StatusRegister::PARTNUM);
	uint8_t partnum = CC1101_ReadStatus(CC1101_StatusRegister::PARTNUM);
	uint8_t version = CC1101_ReadStatus(CC1101_StatusRegister::VERSION);
	char buffer[256];
	sprintf(buffer, "CC1101: %d %d\n", partnum, version);
	CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));

    CC1101_Init();
    uint8_t reg = 0;

    reg = CC1101_ReadConfiguration(CC1101_ConfigurationRegister::IOCFG0);
    sprintf(buffer, "IOCFG0=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadConfiguration(CC1101_ConfigurationRegister::PKTCTRL0);
    sprintf(buffer, "PKTCTRL0=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadConfiguration(CC1101_ConfigurationRegister::PKTCTRL1);
    sprintf(buffer, "PKTCTRL1=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadConfiguration(CC1101_ConfigurationRegister::MDMCFG2);
    sprintf(buffer, "MDMCFG2=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadConfiguration(CC1101_ConfigurationRegister::SYNC1);
    sprintf(buffer, "SYNC1=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadConfiguration(CC1101_ConfigurationRegister::SYNC0);
    sprintf(buffer, "SYNC0=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    static uint32_t last_print = 0;
    RF_USB_StateMachine<RF_RX_State> rf_rx_state_machine(rf_rx_state);

    while (1)
    {

    	if (HAL_GetTick() - last_print > 1000)
    	{
    		last_print = HAL_GetTick();
//    	    char dbg[64];
//    	    sprintf(dbg, "R_STATE=%u rf_rx_len=%u\n", r_state, rf_rx_len);
//    	    CDC_Transmit_FS((uint8_t*)dbg, strlen(dbg));
    	}

    	rf_rx_state_machine.process();
    }
}
