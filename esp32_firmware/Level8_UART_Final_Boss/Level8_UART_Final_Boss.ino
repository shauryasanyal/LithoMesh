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

struct FrameHeader {
    uint8_t magic[2];
    uint16_t session_id;
    uint8_t seq_id;
} __attribute__((packed));

struct FrameTrailer {
    uint16_t crc;
} __attribute__((packed));

LithoMeshEngine<200, 3, 2000, 4> localNode;

static uint8_t raw_payload[2400];
static uint8_t data_blocks[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t parity_block[BLOCK_SIZE];
static bool packet_received[DATA_BLOCKS + 1];
static uint8_t recovered_data[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t received_parity[BLOCK_SIZE];
static uint8_t reconstructed_payload[2400];
static uint32_t added_buf[200], removed_buf[200];

uint16_t current_session_id = 65530; // Start close to wrap

bool test_slow_consumer = false;
bool test_fragment_tx = false;
bool test_poison_sequence = false;
bool test_desync_attack = false;

// Metrics
uint32_t crc_failures = 0;
uint32_t bad_sessions = 0;

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
    RADIO_SERIAL.setRxBufferSize(256); 
    RADIO_SERIAL.begin(115200, SERIAL_8N1, RADIO_RX_PIN, RADIO_TX_PIN);

    while (!Serial); delay(2000);

    Serial.println("\n\n============================================");
    Serial.println(" LithoMesh Level 8: UART Final Boss ");
    Serial.println("============================================");

    for(uint32_t i=1; i<=1000; i++) localNode.log_event(i);

    if (ROLE_NODE_A) {
        for(uint32_t i=1001; i<=1050; i++) localNode.log_event(i);
        Serial.println("Commands: SYNC, DESYNC, WRAP, SOAK10K, BAUDTEST");
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
            size_t chunk = random(1, 42); 
            if (sent + chunk > len) chunk = len - sent;
            RADIO_SERIAL.write(buf + sent, chunk);
            sent += chunk;
            delay(5); 
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

    uint8_t out_frames[DATA_BLOCKS + 1][sizeof(FrameHeader) + BLOCK_SIZE + sizeof(FrameTrailer)];
    for(int i=0; i<=DATA_BLOCKS; i++) {
        FrameHeader header = {{0xAA, 0x55}, current_session_id, (uint8_t)i};
        memcpy(&out_frames[i][0], &header, sizeof(FrameHeader));
        
        if (i < DATA_BLOCKS) memcpy(&out_frames[i][sizeof(FrameHeader)], data_blocks[i], BLOCK_SIZE);
        else memcpy(&out_frames[i][sizeof(FrameHeader)], parity_block, BLOCK_SIZE);
        
        if (test_desync_attack && i == 2) {
            // Inject fake magic bytes randomly inside the payload
            out_frames[i][sizeof(FrameHeader) + 10] = 0xAA;
            out_frames[i][sizeof(FrameHeader) + 11] = 0x55;
            // Also truncate the frame so the parser looks for trailing bytes
            RADIO_SERIAL.write(&out_frames[i][0], sizeof(FrameHeader) + 20);
            continue; 
        }

        uint16_t crc = crc16(&out_frames[i][0], sizeof(FrameHeader) + BLOCK_SIZE);
        if (test_desync_attack && i == 3) crc = 0xDEAD; // Bad CRC to test trailer skip

        FrameTrailer trailer = {crc};
        memcpy(&out_frames[i][sizeof(FrameHeader) + BLOCK_SIZE], &trailer, sizeof(FrameTrailer));
    }

    int sequence[DATA_BLOCKS + 1];
    for(int i=0; i<=DATA_BLOCKS; i++) sequence[i] = i;

    if (test_poison_sequence) {
        sequence[12] = -1; // Drop
        int temp = sequence[8];
        sequence[8] = sequence[9];
        sequence[9] = temp; // Reorder
    }

    for(int i=0; i<=DATA_BLOCKS; i++) {
        if (sequence[i] == -1) continue;
        if (test_desync_attack && sequence[i] == 2) continue; // Already handled partial write

        if (test_poison_sequence && i == 4) {
            send_chunk(&out_frames[4][0], sizeof(out_frames[0])); // Dup 4
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
    
    enum ParseState { FIND_MAGIC, READ_HEADER, READ_PAYLOAD, READ_TRAILER };
    ParseState state = FIND_MAGIC;
    
    FrameHeader current_header;
    uint8_t current_payload[BLOCK_SIZE];
    FrameTrailer current_trailer;
    
    uint8_t buf[256];
    size_t buf_idx = 0;
    uint32_t last_rx_time = millis();

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
                        
                        uint8_t crc_check_buf[sizeof(FrameHeader) + BLOCK_SIZE];
                        memcpy(crc_check_buf, &current_header, sizeof(FrameHeader));
                        memcpy(crc_check_buf + sizeof(FrameHeader), current_payload, BLOCK_SIZE);
                        uint16_t calc_crc = crc16(crc_check_buf, sizeof(crc_check_buf));
                        
                        if (calc_crc == current_trailer.crc) {
                            // Safely handle uint16_t wrap
                            int16_t session_diff = (int16_t)(current_header.session_id - current_session_id);
                            if (session_diff >= 0) { 
                                current_session_id = current_header.session_id; 
                                uint8_t seq = current_header.seq_id;
                                
                                if (seq <= DATA_BLOCKS && !packet_received[seq]) {
                                    packet_received[seq] = true;
                                    if (seq < DATA_BLOCKS) memcpy(recovered_data[seq], current_payload, BLOCK_SIZE);
                                    else memcpy(received_parity, current_payload, BLOCK_SIZE);
                                    packets_got++;
                                }
                            } else {
                                bad_sessions++;
                            }
                        } else {
                            crc_failures++;
                        }
                        
                        if (test_slow_consumer) delay(50);
                        state = FIND_MAGIC;
                        buf_idx = 0;
                    }
                    break;
            }
        }
        
        if (packets_got >= DATA_BLOCKS + 1) break; 
        if (packets_got > 0 && (millis() - last_rx_time > 1000)) break; 
    }
    
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
            bool rec = LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::recover_missing(recovered_data, received_parity, first_missing_idx);
            if (!rec) return;
        } else {
            return;
        }
    }
    
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(reconstructed_payload + (i * BLOCK_SIZE), recovered_data[i], BLOCK_SIZE);
    }
    
    static IBLT<200, 3> incoming_iblt;
    memcpy(&incoming_iblt.cells, reconstructed_payload, sizeof(incoming_iblt.cells));

    static IBLT<200, 3> delta;
    incoming_iblt.subtract(localNode.iblt, delta);

    size_t added_count = 0, removed_count = 0, iterations = 0;
    delta.decode(added_buf, 200, &added_count, removed_buf, 200, &removed_count, &iterations);
}

void do_soak10k() {
    Serial.println("\n[SOAK] Starting 10,000 Cycle Torture Test (SLOW+FRAG+POISON)...");
    test_slow_consumer = true;
    test_fragment_tx = true;
    test_poison_sequence = true;
    
    uint32_t t_start = millis();
    int failures = 0;
    
    for(int i=0; i<10000; i++) {
        tx_framed_iblt();
        // Since it's loopback logic required for test running on one board (just testing parser here if needed, but this is a dual script). 
        // In dual ESP32, A just transmits. Let's output metrics per 1000 anyway.
        if (i % 1000 == 0) Serial.printf("Cycle %d...\n", i);
    }
    Serial.printf("Soak Complete. Runtime: %d ms\n", millis() - t_start);
}

void process_command(String cmd) {
    if (cmd == "SYNC") {
        tx_framed_iblt();
    } else if (cmd == "DESYNC") {
        test_desync_attack = true;
        tx_framed_iblt();
        test_desync_attack = false;
    } else if (cmd == "WRAP") {
        current_session_id = 65535; // Next call will wrap to 0
        Serial.println("[TEST] Next session will wrap to 0");
        tx_framed_iblt();
    } else if (cmd == "SOAK10K") {
        do_soak10k();
    } else if (cmd.startsWith("BAUDTEST ")) {
        int target = cmd.substring(9).toInt();
        RADIO_SERIAL.updateBaudRate(target);
        Serial.printf("Baud switched to %d. Blasting payload...\n", target);
        uint32_t t = millis();
        for(int i=0; i<100; i++) tx_framed_iblt();
        Serial.printf("Throughput: %d bytes in %d ms\n", 100 * 2541, millis() - t);
    }
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        process_command(cmd);
    }
    
    if (RADIO_SERIAL.available() >= 2) {
        char c1 = RADIO_SERIAL.peek();
        if (c1 == 0xAA) {
            rx_framed_iblt();
        } else {
            RADIO_SERIAL.read(); 
        }
    }
}
