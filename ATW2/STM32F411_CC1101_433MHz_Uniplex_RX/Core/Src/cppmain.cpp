// cppmain.cpp

#include "cppmain.hpp"
#include "CC1101.hpp"
#include "States.hpp"
#include "RF_USB_StateMachine.hpp"

int cppmain(void)
{
	HAL_Delay(5000);
	HAL_TIM_Base_Start_IT(&htim5);

	CC1101_ReadStatusReg(0x30);
	uint8_t partnum = CC1101_ReadStatusReg(0x30);
	uint8_t version = CC1101_ReadStatusReg(0x31);
	char buffer[256];
	sprintf(buffer, "CC1101: %d %d\n", partnum, version);
	CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));

    CC1101_Init();
    uint8_t reg = 0;

    reg = CC1101_ReadReg(0x02);
    sprintf(buffer, "IOCFG0=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadReg(0x08);
    sprintf(buffer, "PKTCTRL0=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadReg(0x07);
    sprintf(buffer, "PKTCTRL1=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadReg(0x12);
    sprintf(buffer, "MDMCFG2=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadReg(0x04);
    sprintf(buffer, "SYNC1=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);

    reg = CC1101_ReadReg(0x05);
    sprintf(buffer, "SYNC0=%02X\n", reg);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    HAL_Delay(100);



    static uint32_t last = 0;
    while (1)
    {

    	if (HAL_GetTick() - last > 1000)
    	{
    	    last = HAL_GetTick();
//    	    char dbg[64];
//    	    sprintf(dbg, "R_STATE=%u rf_rx_len=%u\n", r_state, rf_rx_len);
//    	    CDC_Transmit_FS((uint8_t*)dbg, strlen(dbg));
    	}

    	RF_USB_StateMachine();
    }
}
