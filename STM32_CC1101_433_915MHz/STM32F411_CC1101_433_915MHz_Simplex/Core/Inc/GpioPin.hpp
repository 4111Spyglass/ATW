// GPIOPin.hpp

#pragma once

#include <cstdint>

#ifdef __linux__
#include "mock_hal.h"
#endif
#ifdef __arm__
#include "stm32f4xx_hal.h"
#endif

enum class PinState : uint8_t
{
    LOW = 0,
    HIGH = 1
};

template <std::uintptr_t PortBase, uint16_t Pin>
struct GpioPin
{
    static GPIO_TypeDef *port() { return reinterpret_cast<GPIO_TypeDef *>(PortBase); }
    static constexpr uint16_t pin = Pin;

    static void set(PinState state)
    {
        HAL_GPIO_WritePin(port(), Pin, state == PinState::HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    static void high()
    {
        HAL_GPIO_WritePin(port(), Pin, GPIO_PIN_SET);
    }

    static void low()
    {
        HAL_GPIO_WritePin(port(), Pin, GPIO_PIN_RESET);
    }

    static bool read()
    {
        return HAL_GPIO_ReadPin(port(), Pin) == GPIO_PIN_SET;
    }
};

// GPIOPin.hpp
