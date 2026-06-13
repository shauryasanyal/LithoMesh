# Synchronized Concurrent Transmission (Glossy Protocol Variant)

## 1. The Collision Avoidance Fallacy
Traditional wireless networks rely on CSMA/CA (Carrier-Sense Multiple Access with Collision Avoidance). Nodes listen to the channel; if it is busy, they apply random exponential backoff. In an ultra-dense $10^9$ node network, the channel is permanently busy. Collision avoidance mathematically devolves into network paralysis.

## 2. The Physics of Constructive Interference
Electromagnetic waves follow the principle of superposition. If two nodes, $N_1$ and $N_2$, transmit the exact same baseband waveform $s(t)$ simultaneously on the same frequency $f_c$, the signals add together in the air.

Let the received signal at a target node be $r(t)$:
$r(t) = h_1 s(t - \tau_1) + h_2 s(t - \tau_2) + w(t)$
Where $h$ is channel attenuation, $\tau$ is propagation delay, and $w(t)$ is noise.

If the temporal displacement $|\tau_1 - \tau_2|$ is significantly smaller than the duration of a modulation symbol $T_s$ (i.e., $|\tau_1 - \tau_2| \ll T_s$), the signals **constructively interfere**. The receiver perceives a single, reinforced signal with higher Signal-to-Noise Ratio (SNR), not a collision.

## 3. The Seismic Clock Sync
To achieve $|\tau_1 - \tau_2| \ll T_s$, the physical transmission jitter must be sub-microsecond. Quartz RTCs cannot provide this underground. 
Instead, a surface-level seismic actuator injects a $1 \text{ Hz}$ acoustic pulse into the bedrock. Because the speed of sound through solid granite is constant ($c_{rock} \approx 5000 \text{ m/s}$), and nodes have static coordinates, nodes calibrate their internal TDMA interrupts to the exact arrival of the seismic wave.

## 4. The Flooding Avalanche
1. **Initiator**: A node transmits a telemetry IBLT payload $P$.
2. **First Hop**: All surrounding nodes receive $P$. 
3. **Reflector Phase**: Using the seismic clock, the receiving nodes wait exactly $\Delta t$ microseconds, and all re-transmit $P$ *simultaneously*.
4. **Constructive Bounce**: The re-transmissions collide constructively in the air. The combined wave front propagates outward.

**Conclusion:** Routing tables are eliminated. Multihop routing becomes a singular, propagating RF tsunami. The network scales infinitely because network density *increases* SNR rather than destroying it.
