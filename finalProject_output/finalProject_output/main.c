/*
 * finalProject_output.c
 *
 * Created: 3/7/2019 4:58:12 PM
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
	// ANDing with ’7? will always keep the value
	// of ‘ch’ between 0 and 7
	ch &= 0b00000111;  // AND operation with 7
	ADMUX = (ADMUX & 0xF8)|ch; // clears the bottom 3 bits before ORing
	
	// start single convertion
	// write ’1? to ADSC
	ADCSRA |= (1<<ADSC);
	
	// wait for conversion to complete
	// ADSC becomes ’0? again
	// till then, run loop continuously
	while(ADCSRA & (1<<ADSC));
	
	return (ADC);
}

// global variable
unsigned char receiveInput;
unsigned char light;

void transmit_data_light(unsigned char data)
{
	//SER : C0
	// RCLK : C1
	// SRCLK : C2
	// SRCLR : C3
	unsigned char i;
	
	for (i = 0; i < 8; ++i)
	{
		PORTB = 0x08; // srclr high and rclk low
		PORTB |= ((data >> i ) & 0x01 );
		PORTB |= 0x04;
	}
	PORTB |= 0x02;
	
	PORTB = 0x00;
}

enum Player_States {P_start, P_stay, P_right, P_left} Player_state;
	
void player_tick()
{
	unsigned char stickInput = receiveInput & 0x0F;
	switch (Player_state)
	{
		case P_start:
			light = 0x00;
			if (stickInput == 0x00)
				Player_state = P_start;
			else if (stickInput == 0x08)
				Player_state = P_right;
			else
				Player_state = P_start;
			break;
		case P_right:
			if (stickInput == 0x08)
				Player_state = P_right;
			else if (stickInput == 0x04)
				Player_state = P_left;
			else
				Player_state = P_stay;
			break;
		case P_left:
			if (stickInput == 0x08)
				Player_state = P_right;
			else if (stickInput == 0x04)
				Player_state = P_left;
			else
				Player_state = P_stay;
			break;		
		case P_stay:
			if (stickInput == 0x08)
				Player_state = P_right;
			else if (stickInput == 0x04)
				Player_state = P_left;
			else
				Player_state = P_stay;
			break;			
		default:
			break;
	}
	switch(Player_state)
	{
		case P_start:
			transmit_data_light(light);
			break;
		case P_right:
			if (light == 0x00)
				light = 0x01;
			else if (light == 0x80)
				light = 0x80;
			else
				light = light << 1;
			transmit_data_light(light);
			break;
		case P_left:
			if (light == 0x01)
				light = 0x01;
			else 
				light = light >> 1;
			transmit_data_light(light);
			break;
		case P_stay:
			transmit_data_light(light);
			break;
		default:
			break;
	}
}

int main(void)
{
	DDRB = 0xFF; PORTB = 0x00;
    unsigned long player_elTime = 300;
	unsigned period = 100;
	TimerSet(period);
	TimerOn();
	initUSART(0);
	//transmit_data_light(0xFF);
    while (1) 
    {
		if (USART_HasReceived(0))
		{
			receiveInput = USART_Receive(0);
			USART_Flush(0);
		}
		if (player_elTime >= 500)
		{
			player_tick();
			player_elTime = 0;
		}
		while(!TimerFlag);
		TimerFlag = 0;
		player_elTime += period;
    }
}

