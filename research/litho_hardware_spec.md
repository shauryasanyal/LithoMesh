# Litho-Mesh Hardware Specification

## 1. Core Architecture
The Litho-Mesh Leaf Node represents a departure from traditional battery-powered, quartz-clocked IoT devices. It is a completely self-sustaining, environment-clocked monolithic unit designed for subterranean deployment.

## 2. The Seismic Clock (Ambient Entanglement)
*   **Sensor:** High-precision MEMS Accelerometer (e.g., ADXL355).
*   **Function:** Constantly monitors the $z$-axis for a highly specific physical signature—a $1\text{ Hz}$ pneumatic impact driven into the bedrock by the surface gateway facility.
*   **Mechanism:** When the seismic wavefront arrives, the shockwave physically jolts the MEMS cantilever. This mechanical movement generates a hardware interrupt to the CPU. 
*   **Result:** This physical interrupt resets the internal timer to $t=0.000$. Because the speed of sound through solid rock is a constant ($\approx 5000\text{ m/s}$), and the nodes never move, the acoustic delay is perfectly static. All 1 Billion nodes are now synchronized to the sub-microsecond without exchanging a single RF control packet.

## 3. Piezo-Acoustic Energy Harvester
*   **Generator:** Lead Zirconate Titanate (PZT) Piezoelectric Cantilever arrays.
*   **Tuning:** The cantilevers are mass-tuned to resonate at $50-60\text{ Hz}$ (ambient industrial machinery hum) and $<10\text{ Hz}$ (heavy vehicle rumbling).
*   **Storage:** 10-Farad Supercapacitor. **No chemical batteries.** Chemical batteries leak, degrade, and die. Supercapacitors have an infinite charge/discharge lifecycle.
*   **Power Budget:** The harvester trickles $10-50 \text{ \mu W}$ continuously. The supercapacitor stores the energy and discharges rapidly during the 50ms Constructive RF transmission slot.

## 4. Compute & Radio Module
*   **CPU:** ARM Cortex-M0+ (e.g., inside an STM32WL or nRF module).
*   **RAM:** $64\text{ KB}$ SRAM.
*   **State Machine:** Executes the IBLT (Invertible Bloom Lookup Table) matrix. Because the IBLT strictly requires $3.2\text{ KB}$ of memory for $O(1)$ state representation, the $64\text{ KB}$ RAM is more than sufficient to house the entire infinite-history CRDT logic.
*   **Radio:** Sub-GHz transceiver operating strictly as a dumb wave-generator. It blindly transmits at the exact microsecond dictated by the Seismic Clock interrupt to achieve Constructive Interference.

## 5. Summary
The node has no battery to die, no quartz crystal to drift, and no routing tables to overflow. It is a pure manifestation of physics.
