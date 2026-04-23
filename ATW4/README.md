# CC1101 Unified Radio Manager  
A clean, deterministic firmware architecture for reliable 433 MHz packet radio

This repository contains the firmware for a pair of STM32‑based nodes communicating over TI’s CC1101 sub‑GHz transceiver.  

It replaces the original multi‑state‑machine design (arbiter + RX SM + TX SM) with a **single, unified radio state machine** that is dramatically more reliable under load. For the curious, the original multi-state machine is still available in the previous commit.

The result:  
- Symmetric behavior across nodes  
- No more RX starvation  
- No more TX lockups  
- Stable operation with 61‑byte packets at 100 ms intervals  
- Clean, predictable radio control

## ✨ Features
  **Allows setting the carrier frequency and the power level**

- **Unified 4‑state radio controller**  
  `INIT → RX_LISTENING → RX_PROCESSING → TX_START → TX_WAIT_END`

- **Deterministic RX‑first scheduling**  
  RX always wins; TX only starts when the channel is clear.

- **Robust error recovery**  
  All MARCSTATE anomalies, FIFO overflows/underflows, and partial packets collapse into a single recovery path (`force_rx()`).

- **Minimal, safe CC1101 driver (`CC1101.hpp`)**  
  - TI‑correct reset sequence  
  - Full register configuration for 433.92 MHz, 38.4 kbps 2‑FSK  
  - Whitening + CRC  
  - Burst FIFO helpers  
  - Clean strobe API

- **USB ↔ RF bridging**  
  - USB CDC input is buffered in a 512‑byte ring  
  - RF packets are forwarded to USB with RSSI/LQI metadata

- **Symmetric node behavior**  
  Both nodes run identical firmware and behave identically under load.

## 📡 Architecture Overview

### Old System (Removed)
- Arbiter state machine  
- USB→RF TX state machine  
- RF→USB RX state machine  
- Hardware timing drift between them  
- RX could be interrupted mid‑packet  
- Nodes diverged: one behaved normally, the other collapsed into TX‑heavy mode

### New System (Current)
A single, unified state machine:
```
INIT
↓
RX_LISTENING  ←───────────────┐
↓                             │
RX_PROCESSING                 │
↓                             │
TX_START → TX_WAIT_END ───────┘
```

This architecture eliminates timing drift, prevents RX interruption, and ensures both nodes remain in sync.

## 🔧 CC1101 Configuration

The radio is configured for:

- **434.2 MHz**
- **38.4 kbps 2‑FSK**
- **Whitening + CRC**
- **Variable‑length packets (max payload 61 bytes)**
- **Auto‑calibration on IDLE→RX**

The initialization table is based on TI’s SWRS061 reference values.


## 🚀 How It Works

### Receiving
- GDO0 asserts on packet with valid CRC  
- ISR sets `packet_received_flag`  
- `RX_PROCESSING` reads FIFO, extracts payload, RSSI, LQI  
- Packet is forwarded to USB

### Transmitting
- USB data accumulates in a ring buffer  
- TX only begins when the channel is clear (GD2 low)  
- Up to 61 bytes are pulled into a packet  
- Radio enters TX, then returns to RX automatically

### Error Handling
Any of the following triggers a full reset to RX:

- RX FIFO overflow  
- TX FIFO underflow  
- MARCSTATE anomalies  
- Partial packets  
- Ghost triggers  

## Performance

With the unified state machine:

- 61-byte packets
- 100 ms intervals
- Bidirectional traffic
- No packet loss
- No TX lockups
- No asymmetric node behavior

---

## 🛠️ Requirements

- 2 x STM32F4 MCU
- 2 x CC1101 transceiver module
- STM32Cube HAL
- USB CDC enabled


## 🎥 Related Video

This firmware is featured in the At the Workbench episode:
“I Deleted 3 State Machines… and the Radio Finally Worked”