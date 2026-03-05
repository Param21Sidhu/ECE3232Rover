/*
 * File:   main.c
 * Author: Paramveer Singh
 *
 * Created on March 5, 2026, 4:58 AM
 */
// PIC16F18855 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1
#pragma config FEXTOSC = ECH    // External Oscillator mode selection bits (EC above 8MHz; PFM set to high power)
#pragma config RSTOSC = HFINT32 // Power-up default value for COSC bits (HFINTOSC with OSCFRQ= 32 MHz and CDIV = 1:1)
#pragma config CLKOUTEN = OFF   // Clock Out Enable bit (CLKOUT function is disabled; i/o or oscillator function on OSC2)
#pragma config CSWEN = ON       // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable bit (FSCM timer enabled)

// CONFIG2
#pragma config MCLRE = ON       // Master Clear Enable bit (MCLR pin is Master Clear function)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config LPBOREN = OFF    // Low-Power BOR enable bit (ULPBOR disabled)
#pragma config BOREN = ON       // Brown-out reset enable bits (Brown-out Reset Enabled, SBOREN bit is ignored)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (VBOR) set to 1.9V on LF, and 2.45V on F Devices)
#pragma config ZCD = OFF        // Zero-cross detect disable (Zero-cross detect circuit is disabled at POR.)
#pragma config PPS1WAY = ON     // Peripheral Pin Select one-way control (The PPSLOCK bit can be cleared and set only once in software)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable bit (Stack Overflow or Underflow will cause a reset)

// CONFIG3
#pragma config WDTCPS = WDTCPS_31// WDT Period Select bits (Divider ratio 1:65536; software control of WDTPS)
#pragma config WDTE = OFF       // WDT operating mode (WDT Disabled, SWDTEN is ignored)
#pragma config WDTCWS = WDTCWS_7// WDT Window Select bits (window always open (100%); software control; keyed access not required)
#pragma config WDTCCS = SC      // WDT input clock selector (Software Control)

// CONFIG4
#pragma config WRT = OFF        // UserNVM self-write protection bits (Write protection off)
#pragma config SCANE = available// Scanner Enable bit (Scanner module is available for use)
#pragma config LVP = ON         // Low Voltage Programming Enable bit (Low Voltage programming enabled. MCLR/Vpp pin function is MCLR.)

// CONFIG5
#pragma config CP = OFF         // UserNVM Program memory code protection bit (Program Memory code protection disabled)
#pragma config CPD = OFF        // DataNVM code protection bit (Data EEPROM code protection disabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#define _XTAL_FREQ 32000000UL

static uint16_t ADC_Read(){

    ADPCH = 0b010000; // for ANC0 channel
    __delay_us(10);
    
    ADCON0bits.GO = 1;          // start conversion
    while (ADCON0bits.GO) {;}   // wait until done
    
    return ((uint16_t)ADRESH << 8) | ADRESL;
    
}


void main(void) {
    
     // Debug LED: use RA0 as an output (board LED might be on RA0/RA2 etc.)
    TRISAbits.TRISA0 = 0;
    ANSELAbits.ANSA0 = 0;
    
    
    
    TRISCbits.TRISC0 = 1;
    ANSELCbits.ANSC0 = 1;
    
    
      // Vref+ = VDD, Vref- = VSS
    
     ADCON0bits.ADON = 1;        // ADC ON
     
     
    ADREFbits.ADPREF = 0;       // VDD
    ADREFbits.ADNREF = 0;       // VSS
    
    ADCLKbits.ADCCS = 0b111111; // slow clock fosc/128
    ADACQbits.ADACQ = 0b00010000;
    
       while (1)
    {
        // Average multiple samples for stable reading
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 16; i++)
        {
            sum += ADC_Read();
        }
        uint16_t adc = (uint16_t)(sum / 16);

        // Conductive material -> VOUT low -> ADC low
        // Tune threshold after first run if needed
        if (adc < 300)
            LATAbits.LATA0 = 1;     // LED ON = conductive
        else
            LATAbits.LATA0 = 0;     // LED OFF = not conductive

        __delay_ms(100);
    }
    
     
     
    return;
}
