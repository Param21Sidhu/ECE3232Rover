/*
 * Milestone 2 Test Code (PIC16F18855 + PCU)
 * - Sends 0501 (GET_INFO) repeatedly
 * - Receives 0502 (20-byte payload = 10 channels)
 * - Parses with sync-based state machine
 * - Decodes 10 channels into ch[10]
 * - Provides simple “testing hooks”:
 *     * LED toggles on every received byte (RCIF)
 *     * LED ON when a valid 0502 message is received (id=0502, len=20)
 *     * Watch variables in MPLAB: ch[0..9], rx_id, rx_len, msg_count, oerr_count
 *
 * IMPORTANT:
 * - Adjust LED pin if your board uses a different LED.
 * - Ensure RX pin is digital (ANSEL cleared) and PPS mapping is correct for your UART pins.
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

// ================= CONFIG (keep yours if already set) =================
#pragma config FEXTOSC = ECH
#pragma config RSTOSC = HFINT32
#pragma config CLKOUTEN = OFF
#pragma config CSWEN = ON
#pragma config FCMEN = ON
#pragma config WDTE = OFF

#define _XTAL_FREQ 32000000UL

// ================= PCU Protocol =================
#define PCU_SYNC1 0xFE
#define PCU_SYNC2 0x19

// Message IDs
#define PCU_MSG_GET_INFO   0x0501  // request
#define PCU_MSG_USER_DATA  0x0502  // response

// ================= Debug LED (CHANGE IF NEEDED) =================
// Example: LED on RA2
#define LED_TRIS  TRISAbits.TRISA2
#define LED_LAT   LATAbits.LATA2

// ================= RX Parser State Machine =================
typedef enum {
    RX_WAIT_FE = 0,
    RX_WAIT_19,
    RX_ID_LSB,
    RX_ID_MSB,
    RX_LEN_LSB,
    RX_LEN_MSB,
    RX_PAYLOAD
} rx_state_t;

volatile rx_state_t rx_state = RX_WAIT_FE;

volatile uint8_t  rx_id_lsb = 0;
volatile uint8_t  rx_id_msb = 0;
volatile uint16_t rx_len = 0;
volatile uint16_t rx_index = 0;

#define RX_PAYLOAD_MAX 64
volatile uint8_t rx_payload[RX_PAYLOAD_MAX];

volatile bool msg_ready = false;

// ================= Decoded channels (watch these) =================
// Order from your 0502 table:
// ch[0]=Right X, ch[1]=Right Y, ch[2]=Left Y, ch[3]=Left X,
// ch[4]=SWA, ch[5]=SWB, ch[6]=SWC, ch[7]=SWD, ch[8]=VRA, ch[9]=VRB
volatile uint16_t ch[10];

// Debug counters (watch these)
volatile uint32_t rx_byte_count = 0;
volatile uint32_t msg_count = 0;
volatile uint32_t bad_msg_count = 0;
volatile uint32_t oerr_count = 0;

// ================= UART TX helpers =================
static inline void uart_send_byte(uint8_t b)
{
    while (PIR3bits.TXIF == 0) { /* wait until TXREG empty */ }
    TX1REG = b;
}

static void pcu_send_message(uint16_t msg_id, const uint8_t *payload, uint16_t len)
{
    // Header
    uart_send_byte(PCU_SYNC1);
    uart_send_byte(PCU_SYNC2);

    // Msg ID (LSB first)
    uart_send_byte((uint8_t)(msg_id & 0xFF));
    uart_send_byte((uint8_t)(msg_id >> 8));

    // Payload length (LSB first)
    uart_send_byte((uint8_t)(len & 0xFF));
    uart_send_byte((uint8_t)(len >> 8));

    // Payload
    for (uint16_t i = 0; i < len; i++) {
        uart_send_byte(payload[i]);
    }
}

// ================= UART Init =================
static void uart_init_115200(void)
{
    // LED setup
    LED_TRIS = 0;
    LED_LAT = 0;

    // --- Make sure UART pins are DIGITAL if they share analog ---
    // NOTE: You MUST set the correct ANSEL bits for your RX/TX pins.
    // Example if RX is on RA1, you'd do: ANSELAbits.ANSA1 = 0;
    // Example if TX is on RA0, you'd do: ANSELAbits.ANSA0 = 0;

    // --- PPS mapping (ONLY if your project requires PPS config) ---
    // You must map:
    //  - RXPPS to your RX pin
    //  - TX1PPS (or RxyPPS) to your TX pin
    // This is board/schematic dependent, so not hard-coded here.

    // Baud rate for 32MHz: your setting (approx 115942)
    BAUD1CONbits.BRG16 = 1;   // 16-bit BRG
    TX1STAbits.BRGH = 1;      // high speed
    SP1BRGH = 0x00;
    SP1BRGL = 0x44;

    // Async mode
    TX1STAbits.SYNC = 0;

    // Enable serial port + continuous receive
    RC1STAbits.SPEN = 1;
    RC1STAbits.CREN = 1;

    // Enable transmitter
    TX1STAbits.TXEN = 1;

    // Enable RX interrupts (recommended)
    PIE3bits.RCIE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

    // TX interrupt not needed for this test
    PIE3bits.TXIE = 0;
}

// ================= RX ISR =================
void __interrupt() ISR(void)
{
    if (PIR3bits.RCIF) {

        // Count bytes + blink LED quickly (you’ll see flicker)
        rx_byte_count++;
        LED_LAT ^= 1;

        // Overrun handling: if OERR is set, receiver stops until cleared
        if (RC1STAbits.OERR) {
            oerr_count++;
            RC1STAbits.CREN = 0;
            RC1STAbits.CREN = 1;
            rx_state = RX_WAIT_FE;
            msg_ready = false;
        }

        // Read byte (RCIF clears when RCREG/RC1REG is read)
        uint8_t b = RC1REG;

        // Stream parser for PCU header+payload
        switch (rx_state) {

            case RX_WAIT_FE:
                if (b == PCU_SYNC1) rx_state = RX_WAIT_19;
                break;

            case RX_WAIT_19:
                if (b == PCU_SYNC2) rx_state = RX_ID_LSB;
                else rx_state = RX_WAIT_FE;
                break;

            case RX_ID_LSB:
                rx_id_lsb = b;
                rx_state = RX_ID_MSB;
                break;

            case RX_ID_MSB:
                rx_id_msb = b;
                rx_state = RX_LEN_LSB;
                break;

            case RX_LEN_LSB:
                rx_len = b;
                rx_state = RX_LEN_MSB;
                break;

            case RX_LEN_MSB:
                rx_len |= ((uint16_t)b << 8);
                rx_index = 0;

                if (rx_len == 0) {
                    msg_ready = true;
                    rx_state = RX_WAIT_FE;
                } else if (rx_len > RX_PAYLOAD_MAX) {
                    // Drop oversized messages safely
                    bad_msg_count++;
                    rx_state = RX_WAIT_FE;
                } else {
                    rx_state = RX_PAYLOAD;
                }
                break;

            case RX_PAYLOAD:
                rx_payload[rx_index++] = b;
                if (rx_index >= rx_len) {
                    msg_ready = true;
                    rx_state = RX_WAIT_FE;
                }
                break;

            default:
                rx_state = RX_WAIT_FE;
                break;
        }
    }
}

// ================= Decode 0502 payload into 10 channels =================
static void handle_pcu_message(void)
{
    if (!msg_ready) return;
    msg_ready = false;

    // Check for 0502
    if (rx_id_lsb == 0x02 && rx_id_msb == 0x05 && rx_len == 20) {

        // Decode 10 channels (each 2 bytes, LSB then MSB)
        for (uint8_t i = 0; i < 10; i++) {
            uint8_t lsb = rx_payload[2 * i];
            uint8_t msb = rx_payload[2 * i + 1];
            ch[i] = ((uint16_t)msb << 8) | lsb;
        }

        msg_count++;

        // Turn LED ON solid when we successfully parse a good message
        LED_LAT = 1;

    } else {
        // Not the message we want / wrong length
        bad_msg_count++;
        LED_LAT = 0;
    }
}

// ================= Main =================
void main(void)
{
    uart_init_115200();

    // Clear channels
    for (uint8_t i = 0; i < 10; i++) ch[i] = 0;

    while (1) {

        // 1) Request controller data (0501, no payload)
        pcu_send_message(PCU_MSG_GET_INFO, 0, 0);

        // 2) Give time for bytes to arrive; ISR fills rx_payload
        __delay_ms(5);

        // 3) If a full message is ready, decode it
        handle_pcu_message();

        // Slow down request rate slightly (tune as needed)
        __delay_ms(10);

        // TESTING:
        // Open MPLAB Watch and observe:
        // - msg_count should increase steadily
        // - ch[2] (Left joystick Y) should move ~1000..2000 when you move left stick up/down
        // - ch[3] (Left joystick X) should move when left stick left/right
        // - switch channels jump between ~1000/1500/2000
        // - oerr_count should stay 0 (ideally)
    }
}
