#include <Arduino.h>
#include "LithoMesh.h"
#include "LithoMeshFEC.h"

#define ROLE_NODE_A true

#define RADIO_SERIAL Serial2
#define RADIO_RX_PIN 16
#define RADIO_TX_PIN 17

#define DATA_BLOCKS 20
#define BLOCK_SIZE 120
#define PACKET_PAYLOAD BLOCK_SIZE

// Frame definition to survive fragmentation and ghosting
struct FrameHeader {
    uint8_t magic[2];
    uint16_t session_id;
    uint8_t seq_id;
} __attribute__((packed));

struct FrameTrailer {
    uint16_t crc;
} __attribute__((packed));

LithoMeshEngine<200, 3, 2000, 4> localNode;

// Static buffers
static uint8_t raw_payload[2400];
static uint8_t data_blocks[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t parity_block[BLOCK_SIZE];
static bool packet_received[DATA_BLOCKS + 1];
static uint8_t recovered_data[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t received_parity[BLOCK_SIZE];
static uint8_t reconstructed_payload[2400];
static uint32_t added_buf[200], removed_buf[200];

uint16_t current_session_id = 100;

// Test Settings
bool test_slow_consumer = false;
bool test_fragment_tx = false;
bool test_poison_sequence = false;
int  burst_loss_injection = 0;

// Simple CRC16 for the frame
uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc = (crc >> 1);
        }
    }
    return crc;
}

void setup() {
    Serial.begin(115200);
    
    // Deliberately SHRINK the buffer to force overflow pressure if we aren't handling stream right
    RADIO_SERIAL.setRxBufferSize(256); 
    RADIO_SERIAL.begin(115200, SERIAL_8N1, RADIO_RX_PIN, RADIO_TX_PIN);

    while (!Serial); delay(2000);

    Serial.println("\n\n============================================");
    Serial.println(" LithoMesh Level 7: UART Torture ");
    Serial.println("============================================");

    for(uint32_t i=1; i<=1000; i++) localNode.log_event(i);

    if (ROLE_NODE_A) {
        for(uint32_t i=1001; i<=1050; i++) localNode.log_event(i);
        Serial.println("Commands: SYNC, SLOW, FRAG, POISON");
    } else {
        Serial.println("Commands: SLOW");
    }
}

// ------------------------------------------------------------------
// TX
// ------------------------------------------------------------------
void send_chunk(uint8_t* buf, size_t len) {
    if (test_fragment_tx) {
        size_t sent = 0;
        while(sent < len) {
            size_t chunk = random(1, 42); // Send random chunks to smash assumptions
            if (sent + chunk > len) chunk = len - sent;
            RADIO_SERIAL.write(buf + sent, chunk);
            sent += chunk;
            delay(5); // Simulate random serial timing gaps
        }
    } else {
        RADIO_SERIAL.write(buf, len);
    }
}

void tx_framed_iblt() {
    current_session_id++;
    Serial.printf("\n[TX] Initiating Session %d\n", current_session_id);
    
    memcpy(raw_payload, &localNode.iblt.cells, sizeof(localNode.iblt.cells));
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(data_blocks[i], raw_payload + (i * BLOCK_SIZE), BLOCK_SIZE);
    }
    LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::generate_parity(data_blocks, parity_block);

    // Build the frames in memory to allow reordering
    uint8_t out_frames[DATA_BLOCKS + 1][sizeof(FrameHeader) + BLOCK_SIZE + sizeof(FrameTrailer)];
    for(int i=0; i<=DATA_BLOCKS; i++) {
        FrameHeader header = {{0xAA, 0x55}, current_session_id, (uint8_t)i};
        memcpy(&out_frames[i][0], &header, sizeof(FrameHeader));
        
        if (i < DATA_BLOCKS) memcpy(&out_frames[i][sizeof(FrameHeader)], data_blocks[i], BLOCK_SIZE);
        else memcpy(&out_frames[i][sizeof(FrameHeader)], parity_block, BLOCK_SIZE);
        
        uint16_t crc = crc16(&out_frames[i][0], sizeof(FrameHeader) + BLOCK_SIZE);
        FrameTrailer trailer = {crc};
        memcpy(&out_frames[i][sizeof(FrameHeader) + BLOCK_SIZE], &trailer, sizeof(FrameTrailer));
    }

    // Transmission Plan
    int sequence[DATA_BLOCKS + 1];
    for(int i=0; i<=DATA_BLOCKS; i++) sequence[i] = i;

    if (test_poison_sequence) {
        Serial.println("[TX] Injecting Poison: Duplicating 4, Reordering 8, Dropping 12");
        sequence[12] = -1; // Drop
        int temp = sequence[8];
        sequence[8] = sequence[9];
        sequence[9] = temp; // Reorder
        
        // Dup 4 will be sent inline manually
    }

    // Execute TX Plan
    for(int i=0; i<=DATA_BLOCKS; i++) {
        if (sequence[i] == -1) continue;
        
        if (test_poison_sequence && i == 4) {
            // Send Duplicate
            send_chunk(&out_frames[4][0], sizeof(out_frames[0]));
        }

        send_chunk(&out_frames[sequence[i]][0], sizeof(out_frames[0]));
    }
}

// ------------------------------------------------------------------
// RX Stream Parser
// ------------------------------------------------------------------
void rx_framed_iblt() {
    uint32_t t_rx_start = millis();
    memset(packet_received, 0, sizeof(packet_received));
    int packets_got = 0;
    
    // Parse State Machine
    enum ParseState { FIND_MAGIC, READ_HEADER, READ_PAYLOAD, READ_TRAILER };
    ParseState state = FIND_MAGIC;
    
    FrameHeader current_header;
    uint8_t current_payload[BLOCK_SIZE];
    FrameTrailer current_trailer;
    
    uint8_t buf[256];
    size_t buf_idx = 0;
    uint32_t last_rx_time = millis();

    Serial.println("\n[RX] Listening for framed stream...");
    
    while(millis() - t_rx_start < 5000) {
        if (RADIO_SERIAL.available()) {
            uint8_t b = RADIO_SERIAL.read();
            last_rx_time = millis();
            
            switch(state) {
                case FIND_MAGIC:
                    if (b == 0xAA) {
                        buf_idx = 0;
                        buf[buf_idx++] = b;
                    } else if (b == 0x55 && buf_idx == 1) {
                        buf[buf_idx++] = b;
                        state = READ_HEADER;
                    } else {
                        buf_idx = 0;
                    }
                    break;
                case READ_HEADER:
                    buf[buf_idx++] = b;
                    if (buf_idx == sizeof(FrameHeader)) {
                        memcpy(&current_header, buf, sizeof(FrameHeader));
                        state = READ_PAYLOAD;
                        buf_idx = 0;
                    }
                    break;
                case READ_PAYLOAD:
                    current_payload[buf_idx++] = b;
                    if (buf_idx == BLOCK_SIZE) {
                        state = READ_TRAILER;
                        buf_idx = 0;
                    }
                    break;
                case READ_TRAILER:
                    buf[buf_idx++] = b;
                    if (buf_idx == sizeof(FrameTrailer)) {
                        memcpy(&current_trailer, buf, sizeof(FrameTrailer));
                        
                        // Validate CRC
                        uint8_t crc_check_buf[sizeof(FrameHeader) + BLOCK_SIZE];
                        memcpy(crc_check_buf, &current_header, sizeof(FrameHeader));
                        memcpy(crc_check_buf + sizeof(FrameHeader), current_payload, BLOCK_SIZE);
                        uint16_t calc_crc = crc16(crc_check_buf, sizeof(crc_check_buf));
                        
                        if (calc_crc == current_trailer.crc) {
                            if (current_header.session_id >= current_session_id) { // Drop ghost sessions
                                current_session_id = current_header.session_id; // Sync to active
                                uint8_t seq = current_header.seq_id;
                                
                                if (seq <= DATA_BLOCKS && !packet_received[seq]) {
                                    packet_received[seq] = true;
                                    if (seq < DATA_BLOCKS) memcpy(recovered_data[seq], current_payload, BLOCK_SIZE);
                                    else memcpy(received_parity, current_payload, BLOCK_SIZE);
                                    packets_got++;
                                }
                            } else {
                                Serial.printf("[RX] Dropped ghost frame from old session %d\n", current_header.session_id);
                            }
                        } else {
                            Serial.println("[RX] Frame CRC Error.");
                        }
                        
                        if (test_slow_consumer) {
                            delay(50); // Intentionally stall the CPU after reading a frame to force UART RX buffer pressure
                        }
                        
                        state = FIND_MAGIC;
                        buf_idx = 0;
                    }
                    break;
            }
        }
        
        if (packets_got >= DATA_BLOCKS + 1) break; 
        if (packets_got > 0 && (millis() - last_rx_time > 1000)) break; // Stream finished/aborted
    }
    
    Serial.printf("\n[RX] Stream end. Collected %d/%d distinct frames.\n", packets_got, DATA_BLOCKS + 1);
    
    // FEC Recovery
    int missing_count = 0;
    int first_missing_idx = -1;
    for(int i=0; i<=DATA_BLOCKS; i++) {
        if(!packet_received[i]) {
            missing_count++;
            if (first_missing_idx == -1) first_missing_idx = i;
        }
    }
    
    if (missing_count > 0) {
        if (missing_count == 1 && first_missing_idx != -1 && first_missing_idx != DATA_BLOCKS) {
            Serial.printf("  -> [FEC] Missing frame %d. Engaging XOR recovery...\n", first_missing_idx);
            bool rec = LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::recover_missing(recovered_data, received_parity, first_missing_idx);
            if (!rec) { Serial.println("  -> [FEC] Recovery failed."); return; }
        } else {
            Serial.printf("  -> [FEC] Collapse! Lost %d frames. Aborting.\n", missing_count);
            return;
        }
    }
    
    // Reconstruct and Decode
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(reconstructed_payload + (i * BLOCK_SIZE), recovered_data[i], BLOCK_SIZE);
    }
    
    static IBLT<200, 3> incoming_iblt;
    memcpy(&incoming_iblt.cells, reconstructed_payload, sizeof(incoming_iblt.cells));

    static IBLT<200, 3> delta;
    incoming_iblt.subtract(localNode.iblt, delta);

    size_t added_count = 0, removed_count = 0, iterations = 0;
    uint32_t t_decode_start = millis();
    bool success = delta.decode(added_buf, 200, &added_count, removed_buf, 200, &removed_count, &iterations);
    
    Serial.printf("Decode Result     : %s\n", success ? "PASS" : "FAIL");
}

void process_command(String cmd) {
    if (cmd == "SYNC") {
        test_fragment_tx = false;
        test_poison_sequence = false;
        tx_framed_iblt();
    } else if (cmd == "FRAG") {
        test_fragment_tx = true;
        test_poison_sequence = false;
        Serial.println("[INJECT] Fragmentation enabled for next TX.");
        tx_framed_iblt();
    } else if (cmd == "POISON") {
        test_fragment_tx = false;
        test_poison_sequence = true;
        tx_framed_iblt();
    } else if (cmd == "SLOW") {
        test_slow_consumer = !test_slow_consumer;
        Serial.printf("[INJECT] Slow Consumer RX is now %s\n", test_slow_consumer ? "ON" : "OFF");
    }
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        process_command(cmd);
    }
    rx_framed_iblt();
}
