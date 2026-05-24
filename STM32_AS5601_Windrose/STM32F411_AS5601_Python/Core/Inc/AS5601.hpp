/*
 * AS5601.hpp
 *
 *  Created on: May 15, 2026
 *      Author: mgi
 */

#ifndef INC_AS5601_HPP_
#define INC_AS5601_HPP_

#include <cstdint>
#include <optional>

static constexpr uint8_t AS5601_ADDR = 0x36 << 1;


template<typename I2C, uint8_t Address = AS5601_ADDR>
class AS5601
{
public:
	enum class REGISTER : uint8_t {
	    ZMCO          = 0x00,
	    ZPOS          = 0x01,
	    ZPOS_H        = 0x01,
	    ZPOS_L        = 0x02,
	    CONF          = 0x07,
	    CONF_H        = 0x07,
	    CONF_L        = 0x08,
	    ABN           = 0x09,
	    PUSHTHR       = 0x0A,
	    STATUS        = 0x0B,
	    RAW_ANGLE     = 0x0C,
	    RAW_ANGLE_H   = 0x0C,
	    RAW_ANGLE_L   = 0x0D,
	    ANGLE         = 0x0E,
	    ANGLE_H       = 0x0E,
	    ANGLE_L       = 0x0F,
	    AGC           = 0x1A,
	    MAGNITUDE     = 0x1B,
	    MAGNITUDE_H   = 0x1B,
	    MAGNITUDE_L   = 0x1C,
	    BURN          = 0xFF
	};

public:
    explicit AS5601(I2C& i2c) : i2c_(i2c) {}

    std::optional<uint8_t> readU8(AS5601::REGISTER reg) const {
        uint8_t value = 0;
        if (HAL_I2C_Mem_Read(&i2c_, Address,
                             static_cast<uint8_t>(reg),
                             I2C_MEMADD_SIZE_8BIT,
                             &value, 1, 10) != HAL_OK)
        {
            return std::nullopt;
        }
        return value;
    }

    bool writeU8(AS5601::REGISTER reg, uint8_t value) const {
        return HAL_I2C_Mem_Write(&i2c_, Address,
                                 static_cast<uint8_t>(reg),
                                 I2C_MEMADD_SIZE_8BIT,
                                 &value, 1, 10) == HAL_OK;
    }

    std::optional<uint16_t> readU16(AS5601::REGISTER reg_high) const {
        uint8_t buf[2] = {0};
        if (HAL_I2C_Mem_Read(&i2c_, Address,
                             static_cast<uint8_t>(reg_high),
                             I2C_MEMADD_SIZE_8BIT,
                             buf, 2, 10) != HAL_OK)
        {
            return std::nullopt;
        }
        return (static_cast<uint16_t>(buf[0]) << 8) |
               static_cast<uint16_t>(buf[1]);
    }

    // ---- Convenience: 12‑bit RAW_ANGLE ----
    std::optional<uint16_t> getRawAngle() const {
        auto v = readU16(REGISTER::RAW_ANGLE_H);
        if (!v) return std::nullopt;
        return v.value() & 0x0FFF;
    }

    // ---- Convenience: 12‑bit ANGLE ----
    std::optional<uint16_t> getAngle() const {
        auto v = readU16(REGISTER::ANGLE_H);
        if (!v) return std::nullopt;
        return v.value() & 0x0FFF;
    }

private:
    I2C& i2c_;
};

#endif
