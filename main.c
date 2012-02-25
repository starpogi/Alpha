//******************************************************************************
//	Light Switching Using RFID Protocol
//	1.5.1.2
//	April 24, 2010
//
//	Changelog available at changelog.txt
//
//	Made for RFID ID-20 (ID Innovations)
//
//	By Javier Onglao and Sergio Shin
//	
//     UART Transmit and Receive by D. Dang (Texas Instruments, October 2010)
//     LED Display Driver by R. Giles (Boston University, April 2011)
//	   RFID Read/Write Protocol by Bildr Blog (http://bildr.org, February 2011)
//
//  Built with CCS Version 4.2.0 and IAR Embedded Workbench Version: 5.10
//******************************************************************************

#include "msp430g2231.h"

//------------------------------------------------------------------------------
// Channel Tag Definitions
//	- Determines which tag id's activate which channel
//------------------------------------------------------------------------------
char channel1[] = "3D002154377F";	// Channel1 is Javie
char channel2[] = "4400E6B60E1A";	// Channel2 is Sergio

//------------------------------------------------------------------------------
// Hardware-related definitions
//------------------------------------------------------------------------------
#define UART_RXD   		0x04                    // RXD on P1.2 (Timer0_A.CCI1A)
#define LED1	   		1						// LED1 on P1.0
#define LED2 	   		0x02					// LED2 on P1.1
#define RST	   	   		0x08					// RFID Reset on P1.3
#define ENABLE_READ 	0x10					// LCD Enable on P1.5
#define OEbar 			0x80					// LCD OE on P1.7

#define BIC(location,mask) ((location) &= ~(mask))
#define BIS(location,mask) ((location) |= mask)

//------------------------------------------------------------------------------
// Conditions for 9600 Baud SW UART, SMCLK = 1MHz
//------------------------------------------------------------------------------
#define UART_TBIT_DIV_2     (1000000 / (9600 * 2))
#define UART_TBIT           (1000000 / 9600)

//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------
unsigned char rxBuffer;                     // Received UART character
volatile unsigned int i = 0;				// Buffer write index control
volatile unsigned int j = 0;				// Buffer read index
char tagId[12];								// Buffer for storing the ID
unsigned int hasRead = 0;					// Done Flag
volatile unsigned int reading = 0;			// Read Flag
volatile unsigned int personA = 0;			// select person id
volatile unsigned int personB = 0;			// select person id
volatile unsigned int personC = 0;			// select person id

volatile unsigned int counter = 50;			// delay for resetting pin
unsigned delay_counter; 					// variable for delays by watchdog interval timer
char needs_init = 1; 						// global flag to avoid multiple poweron inits

//------------------------------------------------------------------------------
// Function prototypes
//------------------------------------------------------------------------------
void TimerA_UART_init(void);
void SR_put_byte(unsigned char);  // send a byte out thru the shift register
void LCD_put(int value); // send a value to the LCD
void LCD_init(void); // initialize LCD
void LCD_put_string(char *);

void delay(unsigned int); // delay a number of microseconds
//------------------------------------------------------------------------------
// main()
//------------------------------------------------------------------------------
void main(void)
{
    WDTCTL = WDTPW + WDTHOLD;               // Stop watchdog timer
    
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;
    
	// Watchdog Timer 
	//========================================
	//   sample the reader every 8 ms to see 
	//   if the tag has been removed
	//=========================================
	WDTCTL =(WDTPW +	// (bits 15-8) password
                        // bit 7=0 => watchdog timer on
                        // bit 6=0 => NMI on rising edge (not used here)
                        // bit 5=0 => RST/NMI pin does a reset (not used here)
           WDTTMSEL +   // (bit 4) select interval timer mode
  		   WDTCNTCL + 	// (bit 3) clear watchdog timer counter
  		                // bit 2=0 => SMCLK is the source
  		   WDTIS1+WDTIS0// bits 1-0 = 11 => source/64 => 8 microsec
  		   );              
  	IE1 |= WDTIE;			// enable the WDT interrupt (in the system interrupt register IE1)	


	//==============================================================
	// LCD Screen
	//==============================================================
	// setup output ports for output to the SR
	BIS(P1OUT,OEbar);	// SR output disabled
	BIC(P1OUT,ENABLE_READ); // controller reads on rising edge of Enable 
	BIS(P1DIR,OEbar|ENABLE_READ); // out pins	
	// setup SPI interface
	USICTL0=USIPE6+USIPE5+USILSB+USIMST+USIOE; // master, LSB first, enable SPI clk, out
	USICTL1=USICKPH;// write on first transition
	USICKCTL=USIDIV_7+USISSEL_2; // clock divisor 7; SM clock source
		
  	_bis_SR_register(GIE);	// enable interrupts
	
	LCD_init();
	LCD_put_string("Welcome!");
	LCD_put(0x80+40); // cursor to line 2
	LCD_put_string("Place your key");

	//===============================
	// I/O Pins
	//===============================

    P1OUT = 0x00;                           // Initialize all GPIO
    P1SEL = UART_RXD;      					// Timer function for TXD/RXD pins
    P1DIR = 0xFF & ~UART_RXD;               // Set all pins but RXD to output
    P2OUT = 0x00;
    P2SEL = 0x00;
    P2DIR = 0xFF;

    __enable_interrupt();
    
    TimerA_UART_init();                     // Start Timer_A UART   
    P1OUT &= ~RST; 
    P1OUT |= RST;							// Enable Read
    
    // Initialize
    P1OUT |= LED1;
	P1OUT &= LED2;
    
    for(;;)
    {	
        // Wait for incoming character
        __bis_SR_register(LPM0_bits);
        
	        if(rxBuffer == 2)
	        {
	        	P1OUT |= LED1;
	        	P1OUT &= LED2;
	        	reading = 1;
	        }
	        if(rxBuffer == 3)
	        {
	        	i = 0;
	        	reading = 0;
	        	hasRead = 1;
	        	
	        	while(j < 12)
	        	{
	        		// Determine person using weights
		        	if(channel1[j] == tagId[j])
		        	{
		        		personA++;
		        	}
		        	if (channel2[j] == tagId[j])
		        	{
		        		personB++;
		        	}
		        	
		        	j++;
	        	}
	        	
	        	if(personA == 12)
	        	{
	        		P1OUT |= LED1;
	        		//P1OUT |= LED2;
	        		
					delay(16000); // must wait 1.52ms after clear!
					LCD_init();
					LCD_put_string("Welcome Sergio!");
					LCD_put(0x80+40); // cursor to line 2
					LCD_put_string("Hi");
	        	}
	        	else if(personB == 12)
	        	{
	        		P1OUT |= LED1;
	        		//P1OUT |= LED2;
	        		
	        		delay(16000); // must wait 1.52ms after clear!
					LCD_init();
					LCD_put_string("Welcome Javie!");
					LCD_put(0x80+40); // cursor to line 2
					LCD_put_string("Hi");
	        	}
	        	else
	        	{
	        		P1OUT &= ~LED1;
	        		//P1OUT |= LED1;
	        		
	        		delay(16000); // must wait 1.52ms after clear!
					LCD_init();
					LCD_put_string("NOT RECOGNIZED");
					LCD_put(0x80+40); // cursor to line 2
					LCD_put_string(tagId);
	        	}
	        	
	        	j = 0;
	        	personA = 0;
	        	personB = 0;
	        	hasRead = 0;
	        }
	        
	        if(hasRead == 0 && reading && rxBuffer != 2 && rxBuffer != 10 && rxBuffer != 13)
	        {
	        	tagId[i] = rxBuffer;
	        	i++;
	        }
		}
    }

//------------------------------------------------------------------------------
// Function configures Timer_A for full-duplex UART operation
//------------------------------------------------------------------------------
void TimerA_UART_init(void)
{
    TACCTL0 = OUT;                          // Set TXD Idle as Mark = '1'
    TACCTL1 = SCS + CM1 + CAP + CCIE;       // Sync, Neg Edge, Capture, Int
    TACTL = TASSEL_2 + MC_2;                // SMCLK, start in continuous mode
}

//------------------------------------------------------------------------------
// Timer_A UART - Receive Interrupt Handler
//------------------------------------------------------------------------------
#pragma vector = TIMERA1_VECTOR
__interrupt void Timer_A1_ISR(void)
{
    static unsigned char rxBitCnt = 24;			// Using Wiegand24 Protocol
    static unsigned char rxData = 0;

    switch (__even_in_range(TAIV, TAIV_TAIFG)) { // Use calculated branching
        case TAIV_TACCR1:                        // TACCR1 CCIFG - UART RX
            TACCR1 += UART_TBIT;                 // Add Offset to CCRx
            if (TACCTL1 & CAP) {                 // Capture mode = start bit edge
                TACCTL1 &= ~CAP;                 // Switch capture to compare mode
                //__delay_cycles(4000);
                TACCR1 += UART_TBIT_DIV_2;       // Point CCRx to middle of D0
            }
            else {
                rxData >>= 1;
                if (TACCTL1 & SCCI) {            // Get bit waiting in receive latch
                    rxData |= 0x80;
                }
                rxBitCnt--;
                if (rxBitCnt == 0) {             // All bits RXed?
                    rxBuffer = rxData;           // Store in global variable
                    rxBitCnt = 8;                // Re-load bit counter
                    TACCTL1 |= CAP;              // Switch compare to capture mode
                    __bic_SR_register_on_exit(LPM0_bits);  // Clear LPM0 bits from 0(SR)
                }
            }
            break;
    }
}
//------------------------------------------------------------------------------
interrupt void WDTHandler()
{
	/*if(--counter == 0)
	{
		//P1OUT ^= RST;
		P1OUT &= ~RST;
		P1OUT |= RST;
		counter = 180;
	}*/

	if ((delay_counter!=0) && (--delay_counter==0)){
		//P1OUT &= ~RST;
		//P1OUT |= RST;
		_bic_SR_register_on_exit(LPM0_bits);
	}
}

//interrupt void WDT_interval_handler(){
//}

// DECLARE WDT_interval_handler as handler for interrupt 10
//ISR_VECTOR(WDT_interval_handler, ".int10")
ISR_VECTOR(WDTHandler,".int10")

void delay(unsigned int n){ // delays at least n microseconds using WDT
	if (delay_counter==0){ // only delay once
		delay_counter=(n+7)/8; // set rounded up number of wdt ticks
		_bis_SR_register(GIE+LPM0_bits);  // go into low power mode
	}
}

void SR_put_byte(unsigned char b){	
	USISRL=b;  // transfer data to register
	USICNT=9+USI16B;  // 9 bits since the read and serial clocks on SR are connected
	while (!(USICTL1&USIIFG)) {}; // wait for flag 
}

void LCD_put(int value){  // send a regular command or data to the LCD
	unsigned char high,low;
	high = value/16;  // 0 0 RS R/W DB7 DB6 DB5 DB4
	low = (value & 0x0F) | (high & 0x30); // 0 0 RS R/W DB3 DB2 DB1 DB0
	SR_put_byte(high);
	BIC(P1OUT,OEbar);   // assert data
	BIS(P1OUT,ENABLE_READ); // latch into controller
	delay(80);
	BIC(P1OUT,ENABLE_READ);
	BIS(P1OUT,OEbar);
	SR_put_byte(low);
	BIC(P1OUT,OEbar);   // assert data
	BIS(P1OUT,ENABLE_READ); // latch into controller
	delay(80);
	BIC(P1OUT,ENABLE_READ);
	BIS(P1OUT,OEbar);	
}

void LCD_put_string (char *s){
	char c;
	int value;
	while ((c = *s++)!=0){
		value=0x200+c;
		LCD_put(value);
		delay(100);
	}
}

void LCD_init(){
	if (needs_init){
		delay(15000); // insure that 1.5 ms have elapsed
		// 3 function sets with delays
		SR_put_byte(3);
		BIC(P1OUT,OEbar); BIS(P1OUT,ENABLE_READ); 
		delay(4100);
		BIC(P1OUT,ENABLE_READ); BIS(P1OUT,OEbar);
		SR_put_byte(3);
		BIC(P1OUT,OEbar); BIS(P1OUT,ENABLE_READ); 
		delay(100);
		BIC(P1OUT,ENABLE_READ); BIS(P1OUT,OEbar);
		// set mode to 4 bit mode
		SR_put_byte(2);
		BIC(P1OUT,OEbar); BIS(P1OUT,ENABLE_READ); 
		delay(80);
		BIC(P1OUT,ENABLE_READ); BIS(P1OUT,OEbar);
		needs_init=0; // only do this stuff once per session
	}
	
	// now we can send using the regular instructions
	LCD_put(0x28); // 2 lines, 5x8 characters
	LCD_put(0x0F); // display, cursor, blink on
	LCD_put(1);    // clear display		
	delay(16000); // must wait 1.52ms after clear!
}

