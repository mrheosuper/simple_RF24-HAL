/*
 * Systick.cpp
 *
 * Created: 13-Mar-19 8:35:25 PM
 *  Author: bacph
 */ 
#include "Systick.h"
#include "avr/io.h"
#include "bit_macro.h"
#include "avr/interrupt.h"
volatile uint64_t mil=0, mic=0; 
void systick_begin()
{
	cli();
	// we use timer 2 for systick
	// Clear registers
	TCCR2B = 0;
	TCCR2A = 0;
	TCNT2 = 0;
	TIMSK2=0;
	TCCR2B|=(1 << CS21)|(1<<CS20) ;// prescaler/64
	TCCR2A|=(1 << WGM21); // CTC mode
	OCR2A=249; // 1us=16 tick
	TIMSK2 |= (1 << OCIE2A); // Output Compare Match A Interrupt Enable
	sei();
	
}
unsigned long micros()
{
	return (mil*1000)+ (TCNT2*4);
}
unsigned long millis()
{
	return mil;
}
ISR(TIMER2_COMPA_vect) {
	mil++;
}