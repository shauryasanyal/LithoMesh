
# LithoMesh: Feasibility of Subterranean Mesh Sync & Reconciliation

**LithoMesh** is an experimental systems research project investigating the boundaries of underground wireless sensor networks (WSNs). Specifically, it aggressively tests the feasibility of combining Conflict-free Replicated Data Types (CRDTs), acoustic synchronization, and synchronous flooding in extreme RF environments.

**Status:** Research & Validation Phase  
**Target Publication Venues:** IPSN, EWSN, SenSys, IEEE IoT-J

> **Note:** This project is explicitly **not** designed for commercial deployment. Strict regulatory limits (e.g., ETSI 1% duty cycle for 868MHz), underground physics (RF multi-path, acoustic jitter), and energy constraints make real deployment non-trivial. This is a **feasibility study**, not production code.

---

## 🛡️ Prior Art Declaration & Patent Defense

### Prior Art & Public Disclosure

**This repository serves as timestamped prior art for all innovations contained herein.**

Published on GitHub: **June 18, 2026** at [https://github.com/shauryasanyal/LithoMesh](https://github.com/shauryasanyal/LithoMesh)

All commits, documentation, and code herein constitute public disclosure under patent law. Any patent application claiming priority to innovations disclosed in this repository **after this date** will face invalidation challenges based on prior art.

### Core Innovations (Prior Art Notice)

The following innovations are hereby publicly disclosed and dedicated to preventing proprietary lockdown of decentralized, offline-first, and mesh networking infrastructure:

1. **Fixed-Memory IBLT Reconciliation for Embedded CRDTs**
   - Bounded-state CRDT synchronization using fixed-size Invertible Bloom Lookup Tables
   - Static memory allocation preventing tombstone accumulation on resource-constrained devices
   - Mathematical modeling of reconciliation failure boundaries

2. **Seismic/Acoustic Clock Synchronization for Underground WSNs**
   - Physical acoustic pulse (hammer-strike) synchronization without GPS
   - Sub-10µs jitter time-sync feasibility analysis for TDMA systems
   - Underground propagation model integration with ETSI regulatory constraints

3. **Energy-Harvested Embedded CRDT Engine**
   - Template-based C++ CRDT implementation using <1.5KB RAM
   - Reed-Solomon FEC integration for lossy transport reconciliation
   - Testbed framework for validating synchronous flooding over underground channels

### Patent License Grant

The author grants **all users a perpetual, worldwide, royalty-free, irrevocable patent license** to use any patents that may issue for the innovations contained in this repository.

If any third party attempts to file a patent claiming priority to innovations disclosed in this repository, the author reserves the right to assert prior art based on this public release and GitHub timestamps.

**This project is dedicated to preventing proprietary lockdown of decentralized, offline-first infrastructure.**

---

## 📜 License

This project is licensed under the **GNU AFFERO GENERAL PUBLIC LICENSE v3 (AGPLv3)** with the following additional protections:

- All innovations herein are released as prior art
- Any derivative network/cloud-based projects must comply with AGPLv3 terms (source code sharing requirement)
- Patent claims over public disclosures in this repository may be challenged

See [LICENSE](LICENSE) for full terms and defensive patent grant.

---

## 🔬 Core Research Hypotheses & Findings

### 1. Bounded-Memory CRDT State Sync via Fixed IBLT
CRDTs accumulate tombstones indefinitely, destroying memory on embedded devices during prolonged offline periods. LithoMesh attempts to use a fixed-memory Invertible Bloom Lookup Table (IBLT) to achieve **bounded-state reconciliation**.

*   **Current Finding:** Implemented template-based static C++ engine requiring `1,328 bytes` of RAM. Tested over simulated LoRa with Reed-Solomon FEC framing.
*   **Mathematical Boundary:** An IBLT of $M$ cells can successfully peel a divergence $D$ only if $M > 1.5 \times D$. If $D$ exceeds this threshold (e.g., extended offline state generation), the peel fails.

### 2. Seismic/Acoustic Clock Synchronization
Can a physical acoustic pulse (e.g., a hammer strike on rock) provide <10 µs jitter time-sync for underground nodes without GPS?
*   **Hypothesis Limit:** If acoustic jitter exceeds >10 µs, Time Division Multiple Access (TDMA) and synchronous flooding collapse. 

### 3. Global Synchronous Flooding via Constructive Interference
Extending the Glossy architecture (Ferrari et al., IPSN'11) to underground environments where multi-path reflections and severe attenuation dominate.

---

## 🛣️ Research Roadmap

### Phase 0: The Hard Stops (Current)
*   **IBLT Math:** Formally prove when fixed IBLT bounds succeed (e.g., $D < M/1.22$ for $K=3$).
*   **Regulatory & Propagation:** Replicate Vuran/Akyildiz underground 868 MHz path-loss models in simulation.

### Phase 1: Lab Prototypes
*   **Acoustic Sync Testbed:** Build hammer-pulse experiment. Measure sync jitter on 3 nodes via accelerometers. *(Gate: <10 µs jitter required).*
*   **Glossy on ESP32:** Port synchronous flooding to ESP32. Chart delivery ratio vs. alignment error.
*   **Piezo Harvester Bench:** Measure µW output under mine-spectrum vibrations.

### Phase 2: Simulation & Field
*   **NS3 Simulation:** Model 500-1000 nodes with real underground propagation models.
*   **IBLT Emulation:** Simulate 72h divergence within a fixed 2 MB IBLT and observe failure cascading.

### Phase 3: Targeted Publications
1.  *Seismic Time Synchronization for Underground Wireless Sensor Networks* (Focusing on why acoustic sync cannot achieve sub-microsecond precision).
2.  *Fixed-Memory IBLT-Based CRDT State Reconciliation: Feasibility and Limits* (Systems paper).
3.  *Level 4 Gauntlet: Embedded IBLT Reconciliation Engine Validation Framework* (Toolkit paper based on this repository).

---

## 🛠️ The Level 4 Gauntlet (IBLT Validation Framework)

This repository houses the **Level 4 Gauntlet**, an embedded framework for validating IBLT reconciliation over simulated noisy transports.

*   **Test A (Healthy):** Proves sublinear CPU scaling (Decode Iterations).
*   **Test B (Loss/FEC):** Proves Reed-Solomon error correction mathematically recovers 30% byte loss without triggering naive round-trip NACKs.
*   **Test C (Threshold Exceedance):** Deliberately explodes the mathematics to prove the $M > 1.5 \times D$ failure boundary.
*   **Test D (Soak):** 100 consecutive syncs proving 0-byte heap leakage due to pure static allocation.

*Run the framework:* `python simulation/level4_gauntlet.py` and `python simulation/fec_integration_test.py`
