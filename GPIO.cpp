#include "GPIO.h"
#include <avr/io.h>
#include "bit_macro.h"
// only can set input or output, can't enable pull_up resistor
void GPIO_Mode(volatile uint8_t * pin_port, uint8_t pin,uint8_t mode)
{
	if(mode>=2) return;
	bitWrite(*pin_port,pin,mode);
}
void GPIO_Mode(char port, uint8_t pin, uint8_t mode)
{
	if(mode>=3) return;
	switch (port)
	{
		case 'B':
		case 'b':
		bitWrite(DDRB,pin,bitRead(mode,0));
		bitWrite(PORTB,pin,bitRead(mode,1)); // enable input_pullup
		break;
		case 'C':
		case 'c':
		bitWrite(DDRC,pin,bitRead(mode,0));
		bitWrite(PORTC,pin,bitRead(mode,1)); // enable input_pullup
		break;
		case 'D':
		case 'd':
		bitWrite(DDRD,pin,bitRead(mode,0));
		bitWrite(PORTD,pin,bitRead(mode,1)); // enable input_pullup
		break;
	}
	return;
}
void GPIO_Write(volatile uint8_t * pin_port, uint8_t pin, uint8_t value)
{
	if (value>=1) value=1;
	bitWrite(*pin_port,pin,value);
	return;
}
void GPIO_Write(char port,uint8_t pin, uint8_t value)
{
	if(value>0) value=1;
	switch (port)
	{
		case 'B':
		case 'b':
		bitWrite(PORTB,pin,value); // enable input_pullup
		break;
		case 'C':
		case 'c':
		bitWrite(PORTC,pin,value); // enable input_pullup
		break;
		case 'D':
		case 'd':
		bitWrite(PORTD,pin,value); // enable input_pullup
		break;
	}
}
void GPIO_Set(volatile uint8_t *pin_port, uint8_t pin)
{
	*pin_port|=bit(pin);
}
void GPIO_Clear(volatile uint8_t *pin_port, uint8_t pin)
{
	*pin_port&= ~bit(pin);
}
uint8_t GPIO_Read(volatile uint8_t * pin_port,uint8_t pin)
{
	return *pin_port & bit(pin);
}
uint8_t GPIO_Read(char port,uint8_t pin)
{
	volatile uint8_t * pin_port;
	switch (port)
	{
		case 'B':
		case 'b':
		pin_port=&PINB;
		break;
		case 'C':
		case 'c':
		pin_port=&PINC;
		break;
		case 'D':
		case 'd':
		pin_port=&PIND;
		break;
	}
	return *pin_port & (1<<pin);
}
void GPIO_Toggle(volatile uint8_t * pin_port, uint8_t pin)
{
	bitSet(*pin_port,pin);
}
void GPIO_Toggle(char port,uint8_t pin)
{
	switch (port)
	{
		case 'B':
		case 'b':
		bitSet(PINB,pin); // enable input_pullup
		break;
		case 'C':
		case 'c':
		bitSet(PINC,pin); // enable input_pullup
		break;
		case 'D':
		case 'd':
		bitSet(PIND,pin); // enable input_pullup
		break;
	}
}