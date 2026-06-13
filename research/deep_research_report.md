# Litho-Mesh: Deep Research Report

**Executive Summary (200–300 words):** The *Litho-Mesh* concept proposes a sub‑surface resilient IoT network using novel physical-layer mechanisms (seismic clock, synchronous RF flooding, IBLT-based state sync, piezo power) to avoid centralized infrastructure. We find **no prior art** for using seismic/acoustic pulses as a global time base for network synchronization – existing systems use GPS/PTP or wired sync even underground (Confidence: *Low* because novel). In contrast, *constructive-interference flooding* is well-studied (Glossy, Splash, CIRF) and achieves sub-microsecond sync by design. Glossy (IPSN’11) exploits simultaneous IEEE 802.15.4 transmissions to achieve >99.99% flood reliability with <1 μs error (Confidence: *High*). Similarly, set reconciliation via Bloom+IBLT is known (Graphene SIGCOMM’19 for blockchains, ConflictSync ’25 for CRDTs). These works show IBLTs can compress state differences efficiently (though not strictly *fixed-size* memory – they use rateless IBLTs for high success). We also note extensive research on underground wireless channels, confirming very high soil attenuation (2.4 GHz only works ~0.5 m) and multi-path (Confidence: *High*). On energy harvesting, Kahrobaee & Vuran (ICC’13) demonstrated piezo-vibration harvesters can yield μW power for WUSNs (Confidence: *Medium*). 

The dominant constraints are **physics and regulations**. ETSI mandates ~0.1–1% duty cycles in 863–870 MHz ISM bands, whereas FCC Part 15.247 (US) allows continuous digital transmission. Thus, continuous synchronized flooding (to overcome collisions) violates EU limits (Death of design). High packet loss (>30%) and multipath in metal tunnels will break rigid TDMA (if nodes miss slots, data is lost) (Confidence: *High*). Clock drift at ±10–50 ppm in mines (–20°C to +50°C) causes sub-μs slots to slip in minutes unless re-synced; seismic sync has unknown jitter/propagation delays, so slot collisions become inevitable (Confidence: *High*). Large-scale simulation confirms catastrophic collapse: 10k nodes with CSMA had near-0% success after collisions (see user data), consistent with known studies (FireChat, Helium failures). Security is also a risk: no PHY-layer crypto means few $10 jammers can drown the FHSS mesh (Duty-cycle limits aside). Economically, deploying ~1B nodes (~$100B CapEx plus labor) is infeasible compared to existing solutions (private 5G towers ~<$1M). 

**Conclusion:** Litho-Mesh as presented conflicts with fundamental physics and regulations. The invention blueprints propose mechanisms that, to our knowledge, are unprecedented. While individual components (synchronous flooding, IBLTs, piezo harvesters) exist, their combination into a TDMA mesh with “seismic clocks” lacks scientific grounding. Any success would require breakthroughs (e.g. absolute time reference sans GPS, truly zero-overhead MAC, lawful power/legal changes). We assess most ideas as impractical under current tech/regimes. (Confidence: *Varies by element; overall viability very low.)

## 1. Prior Art and Related Work

### Seismic/Acoustic Time Synchronization
- **No known practice** uses controlled earth vibrations for clock sync in networks. Existing “seismic synchronization” patents rely on traditional methods: e.g. Precision Time Protocol (IEEE 1588) over wireless in seismic rigs, or wired sync in sensor arrays. We found **no publications or patents** for using seismic pulses (pneumatic hammer, acoustic transducers) as network-wide clock signals (Confidence: *High* on lack of prior art). The closest is ocean-acoustic time sync (US Patent 8427900B2), but that uses underwater sonar, not terrestrial solids. In brief, lithospheric clock sync appears novel.

### Synchronized Flooding / Constructive-Interference
- Glossy (Ferrari et al., IPSN’11) pioneered **synchronous flooding**: nodes flood with identical packets, exploiting constructive RF interference. Glossy achieves <1 µs global time sync and ~99.99% reliability. Numerous follow-ups (Splash/USENIX’13, CIRF/EWSN’14) confirm the approach. E.g. [29†L88-L96] notes constructing interference allows sub-µs sync and exceptional reliability. 
- **Patents:** Chinese patent CN105262693A (2016) by Nanjing Univ. claims “efficient flooding based on constructive interference” in asynchronous WSNs. It describes RI-MAC, sleep cycles, and neighbor ACK slots for CI flooding. This suggests IP interest, but no identical concept to synchronized global flooding.
- Glossy’s constraints (MHz-range transceivers, slot alignment) are documented. Glossy’s authors discuss that for 2.4 GHz, ∆<0.5 µs alignment (150 m equivalent) is needed, else capture effect dominates. Lower frequencies (e.g. 868 MHz) have longer symbols (~4x), loosening ∆ but still requiring ≲1 µs. The flood literature thus bounds sync precision and range.

### TDMA and Time-Slotted Access
- TSCH (IEEE 802.15.4e) is a standardized slot-based MAC. It requires multi-hop sync, usually via periodic beacons from a coordinator. These assume modest drift and occasional anchors. No known standard solves *GPS-less* global sync at large scale using Earth signals. Some research on self-organizing TDMA (e.g. DESYNC protocols) exists, but none for 1µs-level precision or megascale mesh. A 868 MHz WUSN (Guo et al., 2010) used TDMA MAC at 10 mW; likely synced by narrow beacons, not atomic clocks. In short, TDMA needs some sync mechanism – glossy's approach inherently syncs via flooding.

### State Sync: IBLT and Bounded CRDTs
- **IBLT-based reconciliation:** Graphene (SIGCOMM’19) uses Bloom filters + an invertible Bloom lookup table (IBLT) to reconcile sets (blockchain transactions) with ~12% of usual bandwidth. It requires pre-sizing the IBLT for expected set difference. ConflictSync (arXiv’25) extends Bloom+rateless-IBLT methods to CRDT states. They confirm IBLT+Bloom can efficiently sync large states with high probability, but typically still use memory ~O(n) or rateless coding to guarantee success. **Fixed-size, never-growing CRDT** appears novel: existing works assume memory grows with state, or trade off success probability. Some blockchain research (Graphene) uses fixed-size buffers but requires probabilistic decoding and parameter tuning. No literature claims *strictly* O(1) state with guaranteed sync for arbitrary downtime (Confidence: *Medium*).
- **Bounded-tombstone CRDTs:** Garbage-collecting CRDTs is an open problem. Some CRDT libraries allow tombstone compression (e.g. Yjs), but none ensure fixed memory while offline. The IBLT approach resembles **set reconciliation** more than CRDTs. In summary, while IBLTs are used for sync, their application to unbounded offline CRDTs is not established.

### Energy Harvesting (Piezoelectric)
- Piezoelectric vibration harvesting in underground sensors has been studied. Kahrobaee & Vuran (ICC’13) analyze WUSN vibration sources and experiment above ground. They conclude harvestable power is limited but **feasible** for low-duty devices (µW-level). Other works (B. C. Vuran’s group) consider solar and thermal in mines; piezo works only with strong periodic vibration. No prior art uses sub-surface **seismic pulses** to power sensors. However, general piezoharvesters and energy-neutral WSN nodes are well-known (e.g. UBC's Edlab, MDPI reviews) (Confidence: *High* that piezo can produce some power, *Low* that it can run high-duty mesh radios).

### Underground Wireless Sensor Networks (WUSN)
- Akyildiz & Stuntebeck’s survey (Ad Hoc Netw 2006) highlights severe attenuation and multi-path underground. More recent channel studies (Ghosh et al., MobiHoc’08, Vuran/Akyildiz CACM’20) show soil water content dramatically affects loss. For example, 2.4 GHz radio can’t penetrate beyond ~0.5 m of earth; even 868 MHz suffers >80 dB loss per meter under wet soil. Multi-path and soil inhomogeneity cause unpredictable fading. These works conclude subterranean links require very low frequencies and/or cables; multi-hop becomes unreliable at scale. (Confidence: *High* that underground propagation is poor).
- Some practical underground networks use LoRa (868/915 MHz) with linear mesh and repeaters (Li et al. 2024, onemine). They report **nodes far from gateway suffer high collision loss** due to multi-hop LoRa interference. This aligns with “Shift Change Burst” in Table tests: long multi-hop, low reliability at scale. 

### FHSS/Sub-GHz Regulations
- **Europe (ETSI)**: EN 300 220 restricts duty cycle: *Band L (865–868 MHz)* ≤1% TX duty; *Band N (868.7–869.2 MHz)* ≤0.1%; only narrow subband P (869.4–869.65 MHz) allows 10% at 500 mW. Unlicensed FHSS (IEEE 802.15.4 chirp) can use LBT instead of duty limits. 
- **USA (FCC)**: Part 15.247 for digital mod allows effectively **unlimited continuous Tx** in 902–928 MHz (no duty-cycle limit). However, FHSS (15.247) requires ≥50 hopping channels and ≤0.4 s per channel. In practice, LoRa in US has no duty restriction (just power limit 30 dBm).
- **China**: 800 MHz SRD bands exist (e.g. 779–787 MHz for NB-IoT), but specific duty limits vary. 
- **Regulatory**: Any high-duty mesh (like continuous flood per slot) would violate ETSI limits (death sentence in Europe). (Confidence: *High* on rules.)

### Standards & Platforms
- **IEEE 802.15.4/LoRa/etc.**: Existing mesh (Zigbee, Thread) target indoor IoT, not line-of-sight, and do not implement global TDMA. LoRaWAN uses Aloha (no sync). 
- **5G/CBRS**: Private LTE/5G can cover mines legally (UWB or coax), but require infrastructure. 
- No standard supports 1µs-level global sync in ad-hoc environment. 

## 2. Technical Feasibility Analysis

### (a) Seismic Timing (“Lithospheric Clock”)
- **Physics:** Rock compressional waves travel ~3000–6000 m/s depending on soil/rock. A surface hammer pulse could take tens of milliseconds to traverse mine-scale (km). Even with a perfect launch, **arrival time jitter** across nodes due to distance differences is tens of microseconds or more, swamping a 1 µs slot. Temperature-induced speed variations add further error. 
- **Sensors:** MEMS accelerometers or geophones can detect strong low-frequency pulses, but with noise. MEMS have limited sensitivity to deep soil waves (they are optimized for vibration, not static or low frequency shocks). Extracting a sub-µs precise edge from a damped impulse is infeasible. 
- **Clock Recovery:** Even if nodes detect pulse *time*, they still need to convert it into clock ticks. Without an absolute reference, relative offsets remain. Handoff (bearing issue: near vs far nodes hear pulse at different times).
- **Conclusion:** Achieving *microsecond precision* via seismic pulses is extremely unlikely without additional cues or calibration. (Confidence: *High* impracticality.) 

### (b) Constructive-Interference Flooding
- **Requirements:** Glossy showed success for co-located radios: symbol-level alignment <0.5 µs, frequency/phase match via same chip clocks, and relatively static channels. In an industrial or mine setting, conditions degrade:
  - **Multipath:** Metallic tunnels cause rich reflections, frequency-selective fading. Glossy is more robust on open spaces; in a cave, signals may cancel unpredictably.
  - **Phase/Amplitude:** For true constructive combining, waveforms must be identical and in phase at receiver. Even 1% frequency error causes phase slip. Low-cost oscillators (10–50 ppm drift) would desync 1 µs slot in ~20–100 s. 
  - **SNR:** If 30% of packets are lost on first try, as simul. assumed, then retransmissions steal slots. Glossy assumes ideal channel; here, retransmissions will swamp bandwidth.
- **Interference limits:** Glossy analysis: with long links, ∆max=0.5 µs (~150 m path diff). In practice, nodes spread out in tunnels exceed this by far. Without TX control, near nodes drown far ones by capture, so network behaves like CSMA flood, not CI. 
- **Theoretical bound:** Even if perfect, the information-theoretic limit for network broadcast delay scales linearly with node count (Ω(n) in worst-case). Achieving 10k floods in <10 s is impossible without collisions. 
- **Conclusion:** Synchronous flooding concept is valid (as Glossy) but only under controlled, small-area conditions. In a huge mine (km scale, metal walls), maintaining slot alignment with arbitrary nodes is not feasible (Confidence: *High* risk of failure).

### (c) IBLT-Based State Sync
- **Theory:** IBLT *set reconciliation* can recover difference if both sides’ IBLTs are sufficiently large. Success probability can approach 1 by scaling IBLT memory (rateless codes). A fixed 2 MB buffer for an *unbounded* CRDT requires low state entropy per period. With heavy updates (e.g. 60 ops/hr) plus 95% reductions, 2 MB may fill quickly if nodes are disconnected days.
- **Information bounds:** If two nodes diverge by many updates, compressing into 2 MB IBLT implies lost information or high collision risk. Graphene requires tuning for expected block size; offline unknown divergence is similar to unknown blockchain size – it either fails or under/over-allocates.
- **Practicality:** Even if ideal, nodes must still process symmetric difference after reconnect, incurring CPU and energy. Early studies (Graphene) dealt with small (kilobyte) blocks, not continuous IoT logs.
- **Conclusion:** IBLT + Bloom sync is promising (Graphene), but guaranteeing *bounded memory* for unbounded updates is not supported by theory: either false negatives or stale data. (Confidence: *Medium* – technique exists, but true O(1) memory is unproven.)

### (d) Piezoelectric Power Harvesting
- **Vibration Sources:** Mines have heavy machinery and drills. Piezo cantilevers can harvest from periodic vibrations (Hz–kHz). Kahrobaee & Vuran found μW/cm³ from agriculture vibes; mines are more intense, maybe 10–100×, but so are demands.
- **Energy Budget:** A GHz radio flood uses ~100–200 mW during TX. Harvesters yield a few μW average at best – orders of magnitude short. Even if nodes sleep, 24/7 radio listen or relay drains >> piezo can refill.
- **Capacitor vs Battery:** Using supercaps doesn’t solve energy deficit; it just buffers from an inadequate source.
- **Durability:** Piezo elements can fatigue. Drills cause broadband noise; designing a resonant harvester that covers all needed frequencies is hard.
- **Conclusion:** Piezo harvesters can modestly extend battery life for low-duty sensors, but cannot sustain high-duty mesh relay radios. (Confidence: *High* insufficiency.)

## 3. Failure Modes & Correlated Risks

- **Clock Drift (Thermodynamics):** TCXO drift in ±20–50 ppm (−20…+50°C) yields 20–50 µs/sec error. Microsecond-slot TDMA needs resync <0.05 s. With no GPS, the proposed seismic sync itself has 10–100 µs jitter. In minutes, slots misalign catastrophically (Test 1 Fail, *Confidence: high*). 
- **Cluster-Chair Failure (Cascading Outage):** If a CH fails, its leaves must elect new CH. Without centralized control, 100 nodes start random TDMA elections simultaneously: RF channel saturates (Test 2 Fail). Recovery (slow self-desync protocols) takes hours in worst-case. (Confidence: *High*.)
- **Traffic Spikes (Rigid TDMA limits):** Shift-change: if everyone bursts data together, fixed slot allocations overflow queues. Unlike CSMA, TDMA can’t adapt burst to available spectrum, so >50–90% packets drop (Test 3). (Confidence: *High*.)
- **Control Overhead (Sync Storm):** Perfect sync requires periodic beacons. In a 10M-node mesh, even low-duty beacons consume huge aggregate airtime (Test 4). Our failure sim showed gateway outage is irrelevant – the real limit is PHY congestion, not routing.
- **Offline State Explosion:** With 72h offline, each node accumulates un-acknowledged state. Even with epoch compaction, without a mechanism to GC safely (missing peers), CRDT tombstones grow unchecked. Real industrial CHs have only a few MB RAM; OOM in <48h is likely (Test 5). (Confidence: *High*.)
- **Correlated Collapses:** A CH reboot floods neighbors in wrong slot, forcing retransmissions that heat/electrically strain others, causing more drift/jams (Test 6). This positive feedback is a realistic “cascading failure” in TDMA fields. (Confidence: *High*.)
- **Noise & Packet Loss:** Tests assumed 30% packet loss. In TDMA, a lost slot means skip until next cycle (seconds later). Increased noise or fading (often >30% in mines) will continuously steal throughput. With constructive flooding, multipath may actually cancel signals, further rising BER. (Test 7 Fail.) (Confidence: *High*.)
- **Jamming/Viruses:** With *no PHY security*, a single malicious device can disrupt a cluster. A cheap SDR jammer or bot ignoring TDMA can flood any channel. The system has no built-in resilience (Test 8). (Confidence: *High*.)
- **Updates & Maintenance:** Upgrading firmware via such a mesh is extremely slow (serial boot floods). A node that reboots mid-update loses TDMA slot sync and may never rejoin (Test 9). (Confidence: *High* practical obstacle.)
- **Regulatory Shutdown:** Continuous high-power pulses (pneumatic or RF) likely violate safety limits. ETSI strictly forbids >1% duty at 1 W. Non-compliance (e.g. 100% duty for flooding) invites legal fines (Test 11). (Confidence: *Very High*.)

**Failure Summary:** Top hazards are regulatory (ETSI duty-cycle kill), and thermodynamics (drift ruining TDMA). Table: 

| Failure Cause                     | Description                                                         | Confidence |
|-----------------------------------|---------------------------------------------------------------------|------------|
| **Regulation Violation**          | 100% duty in ISM band = illegal (ETSI/FCC fines)  | High       |
| **Clock Drift/Sync Loss**         | 50ppm clocks without GPS drift >1μs in 20s; seismic sync jitter≪    | High       |
| **State Overflow**                | 72h offline → CRDT state balloons beyond RAM (no GC)               | High       |
| **Collision Flooding**            | 10k nodes CSMA flood → 0% delivery, RF jam (simulated, test1)      | High       |
| **Jamming (malicious)**          | $10 jammer wipes FHSS mesh (unprotected PHY)                      | High       |
| **Economic Feasibility**         | 1B nodes, $100B+ install cost vs $0.2M tower; hopeless ROI         | High       |
| **Technical Integration**        | Integrating all these novel parts is unprecedented; cross-depends   | Medium     |

The overall architecture survives **no** realistic stress test (score: *Essentially 0% viability*).  

## 4. Implementation Primitives & Validation Experiments

To properly vet the concept, we outline experiments and required hardware:

- **Pneumatic/Acoustic Clock Testbed:** Deploy a strong actuator (hammer or solenoid) on bedrock. Use high-sensitivity accelerometers/geophones on test nodes. Send periodic pulses (e.g. 1 Hz). Measure clock offset: timestamp the arrival and compare nodes via a high-speed data bus (e.g. coax). Evaluate jitter and propagation. *Metrics:* sync error (μs), reliability of detection, power draw. *Confidence:* Anticipate ms-level variance. If >10 μs, TDMA fails.

- **Synchronous Flooding Demo:** Use off-the-shelf radios (e.g. 868 MHz transceivers like Semtech SX1280) in a small lab mesh. Implement Glossy-like floods at 10 Hz, measure delivery and sync. Introduce phase/freq offsets to simulate drift. *Metrics:* Packet success rate vs time alignment error. Create a **chart**: X-axis = alignment error (µs), Y-axis = reliability (e.g. 0–100%). (We might embed an illustrative graph from Glossy theory.) If >1 µs error yields <90% delivery, architecture cannot scale. 

- **Propagation Tests in Mines:** Measure RSSI and PER between 868 MHz nodes at various tunnel depths and soils. Benchmark path loss and multi-hop viability. *Metrics:* Link budget vs distance; multi-path delay spread via channel sounder. (These confirm [68]’s results.)

- **IBLT State Sync Emulation:** In software, simulate two offline nodes accumulating random integers for 72 h, encoding state in fixed 2 MB IBLT. After reconnect, attempt to decode missing elements. *Metrics:* Decoding success vs number of updates. Compare with dynamic IBLT sizing (Graphene style). Likely failure if updates >100k (hypothesis). 

- **Piezoharvester Lab:** Build a prototype sensor with a piezo cantilever and capacitor. Mount on a vibrating test rig replicating mine vibration spectrum. Measure power output under realistic driving (4–20 Hz frequencies, amplitude). *Metrics:* µW output vs frequency; battery recharge rate. Existing harvesters often <100μW under heavy vibration.

- **TDMA Resilience Simulation:** Using an event-driven simulator or modified mesh_sim.py, implement TDMA schedules with clock drift/noise. Simulate up to 100k nodes, 500 ms cycle, 10 s flooding. Introduce random drift and duty violations. *Metrics:* Delivery ratio, slot collision count, how quickly slots desync. Our hybrid model (from user sim) suggests collapse beyond ~1,000 nodes.

- **Regulatory Compliance Check:** Acquire or consult ETSI/FCC specs (like [42]). For each proposed waveform (e.g. 500 mW FHSS ping), verify allowed time. If design violates (almost certain), document needed changes (e.g. permission, LBT).
  
- **Security Testing:** On a small testbed, have an intentional “rogue” node blast noise at nominal channel. Measure the network’s ability to mitigate (if any). Expect rapid collapse.

## 5. Patentability & Novelty

We surveyed patent databases (Google Patents, Espacenet):

- **Seismic Sync:** No matching patents. This idea appears **novel**. Patent search terms (“acoustic clock synchronization”, “seismic time sync WSN”) returned none (Confidence: *High novelty*).
- **Synchronous Flooding:** Glossy itself was published (likely no patent). CN105262693A covers CI flooding in async WSN, and US20150341874A1 (Mesh network system) discusses bit-synchronized broadcast. But our exact scheme (global seismic sync + flooding) is unique. 
- **IBLT Sync:** Graphene was patented via ACM (not patent, but open). No patent specifically for using IBLT to bound CRDT history. Bloom filter patents exist but none cover this combined usage.
- **Piezo Harvest:** Numerous patents on energy harvesting exist, but none specifically for underground WSN. Patent freedom is likely for surface and structural health monitoring.
- **Overall Novelty Map:** 
  - *Novel:* Seismic clock, fixed-size IBLT CRDT, continuous mesh (subject to reg change), vibrating-earth power.
  - *Prior art:* Glossy and variant flooding (public domain research), IBLT+Bloom sync (open research), piezoharvest tech (patents for wearables, etc.), channel models (academic).
- **Search strategy:** We used Google Patents and scholar queries (e.g. “acoustic synchronization network”, “IBLT synchronization patent”). Key families: Chinese patent CN105262693A (Glossy-like), Graphene SIGCOMM (no patent), ETSI/FCC regs (public).
- **Patent Assessment:** The core inventions (1,2,3,4 as enumerated) appear at most barely described academically, making them arguably patentable. But many are fundamentally impossible/illicit, limiting commercial appeal.

## 6. Standards & Regulatory Checklist

- **ETSI EN 300 220 (Europe):** 
  - 863–870 MHz ISM: 1% duty (25 mW), except 0.1% in 868.7–869.2 MHz, and one subband (869.4–869.65) allowing 10% at 500 mW. 
  - No special license covers >1% duty. Using continuous mesh TX would exceed these.
- **FCC Part 15 (USA):** 
  - 902–928 MHz under 15.247 (digital mod): no duty limit, Max 1 W. FHSS 902-928: 50 channels min.  
  - 26 GHz, etc: irrelevant. 
- **3GPP (NB-IoT/LTE-M):** Licensed bands (e.g. 800/900 MHz). These allow full-duplex comm, but need infrastructure. Not applicable offline.  
- **Other Regions:** Most regulatory regimes follow EU/US style rules for ISM. Some countries (e.g. Japan 920 MHz) impose duty limits or LBT.
- **Frequency Selection:** Sub-GHz (868/915 MHz) are likely (for range). But they are crowded (ISM).
- **Conclusion:** Any design must either operate under 1% duty (→ 360 ms airtime per hour) or justify LBT. A full-mesh flood/relay network cannot meet this. (Confidence: *Very high* regulatory showstopper.)

## 7. Prioritized Research Agenda

**Months 1–2:** *Foundation.* Intensive literature review (wireless underground, HDR synchronization, CRDT sync). Regulatory/patent analysis (ESI: tasks 1,9,10). Procurement of equipment (transceivers, accelerometers, piezos). **Gate:** If initial studies show >0.1 s sync error with acoustic method, abort clock-synch path.

**Months 2–3:** *Lab Prototypes.* 
- Build a “sandpit” environment: short-range (10m) rock simulant, actuate pulses. Test clock offset across 2–3 nodes. *Goal:* <10 µs jitter.  
- RF Prototype: Build Glossy-like floods on 868 MHz hardware. Measure delivery under introduced timing offsets. *Goal:* ≥95% rel under 100-node local test.  
- Piezo Harvester Bench: Attach candidate piezo discs to shaker. Evaluate ~μW yields. *Goal:* ~100 μW average (very optimistic).
  
**Months 4–5:** *Scale Testing.* 
- Simulate larger mesh (500–1000 nodes) in NS3 or custom code, include real channel model (based on [68]). *Goal:* Determine N where flood drops below 50%.  
- IBLT Sync Emulation: Use synthetic CRDT loads to test 2 MB IBLT syncing after 48h divergence. *Goal:* successful decode with <1e-6 failure.
- Field Trial (if possible): Deploy 5–10 radios in a real mine or tunnel. Test connectivity, floods, and acoustic pulses.  
**Gate:** If any core claim fails (e.g. clock sync error >10 µs or multi-hop fail), pivot or terminate.

**Month 6:** *Synthesis & Reporting.* Compile results, refine understanding. Decide go/no-go on path (likely kill). Draft publication(s) on findings (pointers to IEEE IoT Journal, ACM SenSys, IPSN for floods, or patent write-ups).

## 8. Suggested Publications & Conferences

- **Wireless Sensor Networks & Systems:** IPSN, EWSN, ACM/IEEE IoT-J, IEEE TMC/TNSM, ACM SenSys (for any implemented mesh/Ci flooding results).
- **Distributed Systems/CRDT:** USENIX ATC (for system evaluation papers), ICDCS/EuroSys, Middleware (ConflictSync-type work). SIGCOMM/INFOCOM for IBLT-set work (Graphene).
- **Energy Harvesting:** IEEE TIE, IEEE IoT Journal, Sensors, WCNC (for piezo results).
- **Regulatory/Standardization:** IEEE Communication Magazine (regulatory summary), 3GPP workshops (for underground IoT).
- **Minerals/Industry:** IEEE T-MT (Mining Tech), or journals of mining (for underground comm).
- **Potential Journal:** ACM TOMPECS for socio-technical analysis, but architecture is too broken.
- **Conferences:** If any element succeeds (e.g. if CI flooding in harsh environment shown feasible), venues like RTSS (if real-time aspects), DCOSS (for underground).
- Also relevant are symposia on broadband communications or IoT in rural areas (IEEE ICC/Globecom).
