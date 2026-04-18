# CC1101 USB → RF → USB Link
Firmware for a deterministic 433 MHz telemetry pipeline

This repository contains two complementary firmware builds:

- **STM32F411_CC1101_433MHz_Uniplex_RX/** — Receive-side firmware
- **STM32F411_CC1101_433MHz_Uniplex_TX/** — Transmit-side firmware
- **Common/** — Shared drivers

Both sides run on STM32 microcontrollers and communicate using the TI CC1101 sub-GHz transceiver. The goal is a **robust, deterministic USB → RF → USB pipeline** suitable for telemetry links in embedded and aerospace applications.

## Features

- CC1101 operation at **433 MHz**
- Variable-length packet support (up to 61 bytes)
- Clean separation of RX and TX state machines
- Deterministic handling of:
  - SPI timing
  - CC1101 status registers
  - GDO0 interrupt events
  - FIFO read/write boundaries
  - USB CDC chunking and buffering
- Shared, reusable low-level drivers

## Architecture Overview

Host → USB CDC → TX Node → CC1101 → RF Link → CC1101 → RX Node → USB CDC → Host

Each node runs its own state machine:

- **TX Node**
  Buffers USB input, frames packets, manages CC1101 TX FIFO, handles TX completion.

- **RX Node**
  Waits for sync, validates packet length, reads FIFO only when complete, forwards data over USB.

Shared libraries provide:

- CC1101 register/strobe enums
- SPI helpers with correct burst-flag semantics
- Ring buffer implementation
- USB CDC abstraction

## Hardware Notes

- CC1101 modules require stable power and clean connectors
- GDO0 configured for **0x06** (sync detect → end-of-packet)
- FIFO reads only occur when `bytesInFifo >= pktLen + 3`
- USB CDC may fragment host writes into multiple packets

## Repository Structure

- `STM32F411_CC1101_433MHz_Uniplex_RX/` — Receiver firmware
- `STM32F411_CC1101_433MHz_Uniplex_TX/` — Transmitter firmware
- `Common/` — Shared CC1101 and utility code

## Getting Started

1. Clone the repo
2. Open either `STM32F411_CC1101_433MHz_Uniplex_RX/` or `STM32F411_CC1101_433MHz_Uniplex_TX/` in STM32CubeIDE
3. Build and flash to your STM32 board
4. Connect via USB CDC and observe the RF link in action

## Related Video

This firmware is featured in the At the Workbench @ 4111Spyglass episode:

*“[Why My RF Link Failed — And How I Finally Fixed It](https://www.youtube.com/watch?v=5pwPYDL50mc&t=314s&pp=0gcJCdMKAYcqIYzv)”*
