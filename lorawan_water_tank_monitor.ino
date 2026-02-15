/*
 * Water Tank Monitor - LoRaWAN (SX1276 Shield)
 * 
 * Hardware:
 * - Arduino UNO R4 WiFi
 * - Duinotech Long Range LoRa Shield (SX1276, 915MHz)
 * - 0.5-5V Pressure Sensor on pin A0
 * 
 * Pin Mapping (typical for SX1276 shields):
 * - NSS (CS): Pin 10
 * - RST: Pin 9
 * - DIO0: Pin 2
 * - DIO1: Pin 6
 * - DIO2: Pin 7
 * 
 * Required Library: MCCI LoRaWAN LMIC library
 * Install via: Arduino Library Manager -> "MCCI LoRaWAN LMIC library"
 */

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// LoRaWAN Credentials (from your LA66 card - works with any LoRaWAN device)
// IMPORTANT: These must be in MSB (Most Significant Byte) format for LMIC
static const u1_t PROGMEM APPEUI[8] = { 0x01, 0x00, 0x00, 0x00, 0x10, 0x40, 0x40, 0xA8 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

static const u1_t PROGMEM DEVEUI[8] = { 0x86, 0x6C, 0x89, 0x11, 0xD1, 0x41, 0x40, 0xA8 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

static const u1_t PROGMEM APPKEY[16] = { 0x57, 0xDD, 0x34, 0x6B, 0x8C, 0x6C, 0x87, 0xFB, 0xB3, 0xAB, 0xFB, 0x74, 0x85, 0x01, 0xDE, 0xC1 };
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16);}

// Pin mapping for SX1276 shield
const lmic_pinmap lmic_pins = {
    .nss = 10,                      // CS pin
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,                       // Reset pin
    .dio = {2, 6, 7},              // DIO0, DIO1, DIO2
};

// Sensor Configuration
const int PRESSURE_SENSOR_PIN = A0;
const float SENSOR_MIN_VOLTAGE = 0.5;
const float SENSOR_MAX_VOLTAGE = 1.44;
const float ADC_REFERENCE = 5.0;
const int ADC_RESOLUTION = 1023;

// Timing
static osjob_t sendjob;
// const unsigned TX_INTERVAL = 600;  // 10 minutes (in seconds)
const unsigned TX_INTERVAL = 60;   // 1 minute for testing

// Track join status
bool joined = false;

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING - Attempting to join network..."));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            Serial.println(F("*** NETWORK JOIN SUCCESSFUL! ***"));
            joined = true;
            
            // Disable link check validation
            LMIC_setLinkCheckMode(0);
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            Serial.println(F("*** Join failed! Check credentials and gateway ***"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART - Transmitting..."));
            Serial.print(F("  Frequency: "));
            Serial.println(LMIC.freq);
            Serial.print(F("  Data Rate: "));
            Serial.println(LMIC.datarate);
            break;
        case EV_TXCANCELED:
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

float readPressureSensor() {
  // Read analog value (average of 10 readings)
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PRESSURE_SENSOR_PIN);
    delay(10);
  }
  int rawValue = sum / 10;
  
  // Convert to voltage
  float voltage = (rawValue / (float)ADC_RESOLUTION) * ADC_REFERENCE;
  
  // Convert to percentage
  float percentage = ((voltage - SENSOR_MIN_VOLTAGE) / 
                     (SENSOR_MAX_VOLTAGE - SENSOR_MIN_VOLTAGE)) * 100.0;
  
  // Constrain to 0-100%
  percentage = constrain(percentage, 0, 100);
  
  Serial.println(F("========================================"));
  Serial.println(F("Sensor Reading:"));
  Serial.print(F("  Raw ADC: "));
  Serial.println(rawValue);
  Serial.print(F("  Voltage: "));
  Serial.print(voltage, 2);
  Serial.println(F(" V"));
  Serial.print(F("  Tank Level: "));
  Serial.print(percentage, 1);
  Serial.println(F(" %"));
  Serial.println(F("========================================"));
  
  return percentage;
}

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Read sensor
        float tankLevel = readPressureSensor();
        
        // Prepare payload (2 bytes: tank level * 100)
        uint16_t levelScaled = (uint16_t)(tankLevel * 100);
        uint8_t payload[2];
        payload[0] = (levelScaled >> 8) & 0xFF;  // High byte
        payload[1] = levelScaled & 0xFF;         // Low byte
        
        Serial.print(F("Sending payload: "));
        Serial.print(payload[0], HEX);
        Serial.print(F(" "));
        Serial.println(payload[1], HEX);
        Serial.print(F("Decoded: "));
        Serial.print(tankLevel, 2);
        Serial.println(F(" %"));
        
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, payload, sizeof(payload), 0);
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000);
    delay(2000);
    
    Serial.println(F("========================================"));
    Serial.println(F("Water Tank Monitor - LoRaWAN"));
    Serial.println(F("SX1276 Shield (915MHz)"));
    Serial.println(F("========================================"));
    Serial.println();
    
    // Initialize sensor
    pinMode(PRESSURE_SENSOR_PIN, INPUT);
    
    // LMIC init
    Serial.println(F("Initializing LMIC..."));
    os_init();
    
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    
    // Set up the channels used by AU915
    // Australia uses sub-band 2 (channels 8-15) + channel 65
    LMIC_selectSubBand(1);  // Sub-band 2 (0-indexed, so 1 = sub-band 2)
    
    // CRITICAL: Set RX delays for Join Accept
    LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);
    
    // Disable link check validation
    LMIC_setLinkCheckMode(0);
    
    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;
    
    // Set data rate and transmit power for uplink
    LMIC_setDrTxpow(DR_SF7, 14);
    
    Serial.println(F("Configuration complete!"));
    Serial.println(F("Region: AU915 Sub-band 2"));
    Serial.println(F("Starting OTAA join..."));
    Serial.println();
    
    // Start job (sending automatically starts OTAA too)
    do_send(&sendjob);
}

void loop() {
    os_runloop_once();
}
