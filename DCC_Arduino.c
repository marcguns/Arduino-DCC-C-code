// genereren van DCC code + 40KHz PWM: uitgang op pin 7

#include <C:/D/Me/Electronica/Arduino/hardware/tools/avr/avr/include/avr/io.h>
#include <C:/D/Me/Electronica/Arduino/hardware/tools/avr/avr/include/util/delay.h>
#include <C:/D/Me/Electronica/Arduino/hardware/tools/avr/avr/include/avr/interrupt.h>
#define BAUD_RATE 38400
#define BAUD_RATE_DIVISOR (F_CPU / 16 / BAUD_RATE - 1)
#define WACHT_OP_BIT(sfr, bit) do { } while (bit_is_clear(sfr, bit))

int Zend, Bitje;
char j, Index, Aant, P56us = 56;	// duur van Puls56us in microsec.
char Adres, Instr;
char DCC[18]={170,170,170,172,0,0,0,0,0,0,0,0,0,0,0,0,0};	// max 138 bits=18 bytes; preamble ingevuld, adres vanaf char4, bit7 (pos 32)
char CRLF[3]={13,10,0};

void UART_TX_CHAR(char K)		// Function to transmit char via UART
	{ WACHT_OP_BIT(UCSR0A, UDRE0);	// wait for Tx buffer
	  UDR0 = K;}
	  
void UART_TX_STR(char s[])		// Function to transmit string via UART
	{ while (*s) { UART_TX_CHAR(*s); s++; } }

char UART_RX_CHAR(void)		// Get character from terminal via UART
{
	while ((UCSR0A & 128) == 0) { }	// bit7=RXC0 hoog: char klaar
	return UDR0;	// Return 1 char from UART
}

char GetNb(char* Vraag)		// Get number from terminal (max 3 digits)
{
	char i = 1;
	char Toets, VToets, Numbr = 0;

	UART_TX_STR(Vraag);
Lees:
	Toets = UART_RX_CHAR();
	if (Toets == 13 || (i == 4)) goto Einde;
	if ((Toets == 8) && (i > 1)) { Numbr = (Numbr - VToets) / 10; }	// Backspace: vorige input neutraliseren
	else if ((Toets > 47) && (Toets < 58))	// indien cijfer
		{ Toets = Toets - 48; VToets = Toets; UART_TX_CHAR(Toets);
		Numbr = (Numbr * 10) + Toets; i++;
		goto Lees; }
Einde:
	if (i == 1) goto Lees;
	return Numbr;
}

void Adapt_Ind_j(void)
	{Index += 2; j = (j + 6) % 8;}

void PlusEen(void)
	{ DCC[Index/8] |= (1<<j); DCC[Index/8] &= ~(1<<(j-1)); Adapt_Ind_j(); }

void PlusNul(void)
	{ DCC[Index/8] |= ((1<<j) | (1<<(j-1))); Adapt_Ind_j(); DCC[Index/8] &= ~((1<<j) | (1<<(j-1))); Adapt_Ind_j(); }

char VulBuf(char Ad, char In)		// Fill Buf[] with bits from adresbyte and instructionbyte & fill error detection byte
{
	int i;

	Index = 32; j = 7;			// start vanaf positie (Index) 32 (1e bit = bit 0); j = nummer v. bit in char, LSB = 0, MSB = 7
	for (i=7;i>=0;i--) {		// adresbyte in DCC zetten
		if (Ad & (1<<i)) { PlusEen(); } else { PlusNul(); }	// "10" erbij else "1100" erbij
	}
	PlusNul();					// 0 tussen adres en instructie: "1100" erbij
	for (i=7;i>=0;i--) {		// instructiebyte erachter zetten
		if (In & (1<<i)) { PlusEen(); } else { PlusNul(); }
	}
	PlusNul();					// 0 tussen instructie en controle
	for (i=7;i>=0;i--) {		// controlebyte erachter zetten
		if ((Ad & (1<<i)) ^ (In & (1<<i))) { PlusEen(); } else { PlusNul(); }	// exor van adresbit en instructiebit
	}
	PlusEen();					// packet end bit=1
	Bitje = 1;
//	Index is nu = aantal bits dat moet verzonden worden (=argument nbBits van ZendDDC)
	return Index;
}

void ZendDCC(char NbBits)
{
	int BitCnt = 0;
	PORTB |= 0x01;	// zet pin 8 (port B bit 0) hoog
	PORTB |= 0x20;	// zet pin 13 (port B bit 5) hoog
	while (BitCnt != (NbBits + 1))
	{
	while ((TIFR0 & 2) == 0) { }
	if (Bitje) { PORTD |= 0x80; } else { PORTD &= 0x7F; }	// pin 7 hoog of laag zoals bit
	TIFR0 |= 2; BitCnt += 1; Bitje = DCC[BitCnt/8] & (1<<(7 - (BitCnt % 8)));	// volgende bit klaarzetten
	}
	PORTB &= 0xFE;	// zet pin 8 laag
	PORTB &= 0xDF;	// zet pin 13 laag
}

int main(void) {
// UART config
	UCSR0A = 0 << U2X0;//normale snelheid
	UCSR0B = 1 << RXEN0 | 1 << TXEN0;//activeer rx en tx
	UCSR0C = 1 << UCSZ01 | 1 << UCSZ00;//8 data bits 1 stop-bit
	UBRR0 = BAUD_RATE_DIVISOR;
	_delay_ms(11);

	DDRD |= (1 << DDD7); // pin 7 (PD7) als output
	DDRB &= 0xFE;		// pin 8 (PB0) als input

// timer 2 (8 bit) voor 40KHz blokgolf
	TCCR2A |= (1 << WGM20) | (1 << WGM21);
	TCCR2B |= (1 << WGM22);	// Fast PWM mode met OCR2A als top
	TCCR2B |= (1 << CS21);	// prescaler = div 8
	OCR2A = 0x31;	// TOP-waarde op 0x31=49 dec: 16MHz/8x50=40KHz, pin 7 aanzetten als TOV2 flag in TIFR2 is gezet (reset door 1 te schrijven)
	OCR2B = 0x2C;	// Duty cycle 90% 0x2C=44 (bepaalt DC spanning), pin 7 afzetten als OCF2B flag in TIFR2 is gezet (reset door 1 te schrijven)

// timer 0 (8 bit) voor DCC: 1 tik om de 56 microsec.
	OCR0A = 0x6F;	// 6F = 111dec: aantal tikken voor OCF0A flag
	TCCR0A |= (1 << WGM01);	// CTC mode, TIFR0 bit 1 (OCF0A) wordt gezet 1 clock cycle na match van counter met OCR0A
	TCCR0A &= 0xFE;	// schrijf 0 in bit 0 (WGM00)
	TCCR0B |= (1 << CS01);	// prescaler = 8: 16MHz/8 = 2MHz = 1 tik per 0,5µsec; 112 tikken geeft 56 microsec.
	TCCR0B &= 0xF7;	// schrijf 0 in bit 3 (WGM02)

	Zend = 0;
	char Bits[] = {32, 'b', 'i', 't', 's', 32, 'v', 'e', 'r', 's', 't', 'u', 'u', 'r', 'd', '.', 13, 10, 0};
	int k;

	while (1) 
	{
		if (Zend == 0) 
		{	// 40KHz blokgolf en zien of een commando is gegeven
			if (TIFR2 & 1) { PORTD |= 0x80; TIFR2 |= 1; }	// zet pin 7 hoog (=bit 7 in PortD) en clear TOV2 (bit 0)
			if (TIFR2 & 4) { PORTD &= 0x7F; TIFR2 |= 4; }	// zet pin 7 laag en clear OCF2A (bit 2)
VraagAdr:
			Adres = GetNb("Geef het device adres (max 127): ");
			if (Adres > 127) goto VraagAdr; // else { UART_TX_CHAR(Adres); UART_TX_STR(CRLF); }
VraagInstr:
			Instr = GetNb("Geef de instructie (max 127): ");
			if (Instr > 127) goto VraagInstr; // else { UART_TX_CHAR(Instr); UART_TX_STR(CRLF); }

			Aant = VulBuf(Adres, Instr);
			ZendDCC(Aant);
			for (k=0;k<20;k++) 
			{	// wacht 20 * 65 microsec. (1,3 msec)
				while ((TIFR0 & 2) == 0) { }
				TIFR0 |= 2;
			}
		}
	}
   return 0;
}
