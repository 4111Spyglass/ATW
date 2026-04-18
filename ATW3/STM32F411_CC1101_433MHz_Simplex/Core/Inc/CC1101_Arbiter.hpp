// CC1101_Arbiter (Instrumented, USB CDC logging)

#pragma once

#include "CC1101.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>


class CC1101_Arbiter
{
public:
    enum class Owner : uint8_t {
        NONE,
        RX,
        TX
    };

private:
    Owner owner_ = Owner::NONE;

    // Provided by your USB CDC layer
    void dbg_print(const char* msg)
    {
        // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    }

    void dbg(const char* fmt, ...)
    {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        dbg_print(buf);
    }

    void log_owner(const char* action)
    {
        dbg("[ARB] %s -> owner=%d\n", action, static_cast<int>(owner_));
    }

public:
    CC1101_Arbiter() = default;

    Owner owner() const { return owner_; }

    void status()
    {
    	log_owner("Current Owner");
    }

    bool request_rx()
    {
        dbg("[ARB] request_rx (current=%d)\n", static_cast<int>(owner_));

        if (owner_ == Owner::NONE || owner_ == Owner::RX) {
            owner_ = Owner::RX;
            log_owner("grant RX");
            return true;
        }

        dbg("[ARB] request_rx DENIED (owned by TX)\n");
        return false;
    }

    bool request_tx()
    {
        dbg("[ARB] request_tx (current=%d)\n", static_cast<int>(owner_));

        if (HAL_GPIO_ReadPin(GD2_Port, GD2_Pin) == GPIO_PIN_SET) {
            return false; // Denial: We are currently receiving a packet
        }

        if (owner_ == Owner::NONE || owner_ == Owner::RX) {
            owner_ = Owner::TX;
            log_owner("grant TX");
            return true;
        }

        dbg("[ARB] request_tx DENIED (owned by TX)\n");
        return false;
    }

    void release()
    {
        dbg("[ARB] release (old owner=%d)\n", static_cast<int>(owner_));
        owner_ = Owner::NONE;
        log_owner("released");
    }

    // --- Wrapped CC1101 accessors (all radio access goes through here) ---

    uint8_t strobe(CC1101_StrobeCommand cmd)
    {
        dbg("[ARB] STROBE 0x%02X (owner=%d)\n",
            static_cast<uint8_t>(cmd),
            static_cast<int>(owner_));

        uint8_t result = CC1101_Strobe(cmd);

        uint8_t marc = CC1101_ReadStatus(CC1101_StatusRegister::MARCSTATE);
        dbg("[ARB]   MARCSTATE=0x%02X\n", marc);

        return result;
    }

    uint8_t read_status(CC1101_StatusRegister reg)
    {
        uint8_t v = CC1101_ReadStatus(reg);
        dbg("[ARB] read_status %d -> 0x%02X\n", static_cast<int>(reg), v);
        return v;
    }

    uint8_t read_config(CC1101_ConfigurationRegister reg)
    {
        uint8_t v = CC1101_ReadConfiguration(reg);
        dbg("[ARB] read_config %d -> 0x%02X\n", static_cast<int>(reg), v);
        return v;
    }

    void write_config(CC1101_ConfigurationRegister reg, uint8_t value)
    {
        dbg("[ARB] write_config %d = 0x%02X\n",
            static_cast<int>(reg), value);
        CC1101_WriteConfiguration(reg, value);
    }

    void write_burst(CC1101_BurstRegister reg, const uint8_t *data, uint8_t len)
    {
        dbg("[ARB] write_burst %d, len=%u\n", static_cast<int>(reg), len);
        CC1101_WriteBurst(reg, data, len);
    }

    void read_burst(CC1101_BurstRegister reg, uint8_t *data, uint8_t len)
    {
        dbg("[ARB] read_burst %d, len=%u\n", static_cast<int>(reg), len);
        CC1101_ReadBurst(reg, data, len);
    }
};
