/*
 * RF24.cpp
 *
 * Created: 12-Mar-19 9:27:35 AM
 * Author : bacph
 */ 

#include <avr/io.h>
#include "GPIO.h"
#include "RF24.h"
#include <util/delay.h>
#include "RF24.h"
#include "Systick.h"

NRFLite _radio;
uint8_t _data;

int main()
{
	_radio.init(0, 'D',5,'D',6);
	while(1)
	{
	 _data++; // Change some data.
	 _radio.send(1, &_data, sizeof(_data)); // Send to the radio with Id = 1
	 delay_ms(10);
	}
}
