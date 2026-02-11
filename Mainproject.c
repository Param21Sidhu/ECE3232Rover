/*
 * File:   Mainproject.c
 * Author: colem
 *
 * Created on February 11, 2026, 2:35 PM
 */

#include <xc.h>

// CONFIG1
#pragma config FEXTOSC = ECH    // External Oscillator mode selection bits (EC above 8MHz; PFM set to high power)
//32 MHz used for baud rate (DO NOT CHANGE WITHOUT CONSIDERATION)
#pragma config RSTOSC = HFINT32 // Power-up default value for COSC bits (HFINTOSC with OSCFRQ= 32 MHz and CDIV = 1:1)
#pragma config CLKOUTEN = OFF   // Clock Out Enable bit (CLKOUT function is disabled; i/o or oscillator function on OSC2)
#pragma config CSWEN = ON       // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable bit (FSCM timer enabled)

#pragma config WDTE = OFF  //Turn off WDT

//variables (storage registers)
uint8_t TXMSG = 0; //declaring as Tx outgoing 8 bits to be placed on TX1REG for transmission
int TXRDY = 0; //declaring TXRDY as the ready for transmission bit (there might be a cleaner way to implement)
uint8_t RXMSG = 0; //declaring as Rx incoming 8 bits to be stored from RC1REG as received

void __interrupt() ISR(){ //Interrupt handler
    
    if(PIR3bits.TXIF == 1){ //ready for another Transmission (Tx1REG is empty)
        
        TX1REG = TXMSG; //byte to be transmitted
        PIR3bits.TXIF = 0; //clear Tx flag
        TXRDY = 1; //Ready for next Transmission
        TXMSG = 0x00; //reset TXMSG so same message is not continuously sent
    }
    
    if(PIR3bits.RCIF == 1){ //Data on receiver (incoming Transmission)
       
        RXMSG = RC1REG; //store received byte
        PIR3bits.RCIF = 0; //clear Rx flag
    }
}

void UARTSYNC(){ //sends Tx sync bits
    
    int x = 0; //sync sent counter
    
    while(x < 2){ //loop until both sync sent
    
        if((TXRDY == 1) && (x == 0)){ //if ready for transmission and no other sync send, send first sync
            TXMSG = 0xF3; //first sync code
            TXRDY = 0; //unready TXMSG
            x = 1; //ready for next sync
        }
        else if((TXRDY == 1) && (x == 1)){ //if ready for transmission and first sync sent, send second sync
            TXMSG = 0x19; //second sync code
            TXRDY = 0; //unready sync
            x = 2;
        }
    }
}

void main(void) { //main function
    
    //set up desired baud rate (115200 bps desired for Dev board communication)
    BAUD1CONbits.BRG16 = 1; //16 bit baud rate generator is used (BRGH & BRGL)
    TX1STAbits.BRGH = 1; //High baud rate selected
    SP1BRGH = 0x00; //set n = 68
    SP1BRGL = 0x44; //set n = 68, at Fosc = 32 MHz, actual baud rate ~= 115942.029
    
    //turn on UART send and receive
    TX1STAbits.TXEN = 1; //Enable transmitter
    TX1STAbits.SYNC = 0; //Asynchronous mode
    RC1STAbits.SPEN = 1; //Receiver enabled
   
    //interrupts setup
    INTCONbits.PEIE = 1; //Peripheral interrupts enabled 
    INTCONbits.GIE = 1; //Global interrupts enabled
    PIE3bits.TXIE = 1; //USART Transmit interrupt enabled 
    PIE3bits.RCIE = 1; //USART Receive interrupt enabled

    while(1){ //always active loop
        //do something
    }
    
    return;
}