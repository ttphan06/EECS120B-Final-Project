
/*
 * final_project.c
 *
 * Created: 2/27/2019 12:42:48 AM
 * Author : thanp
 */ 

#include <avr/io.h>
#include <stdio.h>
//#include "io.c"
#include <avr/interrupt.h>
#include "usart_ATmega1284.h"

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;    // Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

void adc_init()
{
	// AREF = AVcc
	ADMUX = (1<<REFS0);
	
	// ADC Enable and prescaler of 128
	// 16000000/128 = 125000
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

uint16_t adc_read(uint8_t ch)
{
	// select the corresponding channel 0~7
	// ANDing with 7? will always keep the value
	// of ch between 0 and 7
	ch &= 0b00000111;  // AND operation with 7
	ADMUX = (ADMUX & 0xF8)|ch; // clears the bottom 3 bits before ORing
	
	// start single convertion
	// write 1? to ADSC
	ADCSRA |= (1<<ADSC);
	
	// wait for conversion to complete
	// ADSC becomes 0? again
	// till then, run loop continuously
	while(ADCSRA & (1<<ADSC));
	
	return (ADC);
}

// global variable
unsigned char up, down, left, right;


void stick()
{
		adc_init();
		uint16_t x, y;
		y = adc_read(2);
		x = adc_read(3);
		if ((y > 1000) && ((x > 100) && (x < 900))) // up
		{
			PORTB = 0x01;
			up = 1;
			down = left = right = 0;
		}
		else if ((y < 50) && ((x > 100) && (x < 900))) // down
		{
			PORTB = 0x02;
			down = 1;
			up = left = right = 0;
		}
		else if ((x > 1000) && ((y > 100) && (y < 900))) // right
		{
			PORTB = 0x04;
			right = 1;
			up = down = left = 0;
		}
		else if ((x < 50) && ((y > 100) && (y < 900))) // left
		{
			PORTB = 0x08;
			left = 0;
			up = down = right = 0;
		}
		else
		{
			PORTB = 0x00;
			up = down = left = right = 0;
		}
}


unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
	return (b ? x | (0x01 << k) : x & ~(0x01 << k));
}
unsigned char GetBit(unsigned char x, unsigned char k) {
	return ((x & (0x01 << k)) != 0);
}

enum PlayerLight_States {PL_Start, PL_Move} PL_state;

void PL_Tick()
{
	switch (PL_state)
	{
		case PL_Start:
			if (!up)
			{
				PL_state = PL_Start;
			}
			if (up)
			{
				PL_state = PL_Move;
			}
			break;
			
	}
}
void transmit_data(unsigned char data)
{
	//SER : C0
	// RCLK : C1
	// SRCLK : C2
	// SRCLR : C3
	unsigned char i;
	
	for (i = 0; i < 8; ++i)
	{
		PORTC = 0x08; // srclr high and rclk low
		PORTC |= ((data >> i ) & 0x01 );
		PORTC |= 0x04;
	}
	PORTC |= 0x02;
	
	PORTC = 0x00;
}

int main(void)
{
    /* Replace with your application code*/
	DDRA = 0xFF; PORTA = 0x00;
	DDRD = 0x01; PORTD = 0xFE;
	DDRC = 0xFF; PORTC = 0x00;

	//LCD_init();
	TimerSet(500);
	TimerOn();
	initUSART(0);
    while (1) 
    {
		//transmit_data(0x12);
		if (USART_IsSendReady(0))
		{
			//PORTA = 0x02;
			USART_Send(0x01, 0);
		}
		if (USART_HasTransmitted(0))
		{
			PORTA = 0x04;
		}
		if (USART_HasReceived(0))
		{
			PORTA = 0x01;
			PORTA = USART_Receive(0);
			USART_Flush(0);
		}
		while(!TimerFlag){}
		TimerFlag = 0;
    }
}




