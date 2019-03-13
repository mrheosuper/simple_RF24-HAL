/*
 * GPIO.h
 *
 * Created: 12-Mar-19 1:51:19 PM
 *  Author: bacph
 */ 


#ifndef GPIO_H_
#define GPIO_H_
#include "stdio.h"
	#define INPUT 0
	#define INPUT_PULLUP 2
	#define OUTPUT 1
	void GPIO_Mode(volatile uint8_t * pin_port, uint8_t pin,uint8_t mode);
	void GPIO_Mode(char port, uint8_t pin, uint8_t mode);
	void GPIO_Write(volatile uint8_t * pin_port, uint8_t pin, uint8_t value);
	void GPIO_Write(char port,uint8_t pin, uint8_t value);
	void GPIO_Set(volatile uint8_t *pin_port, uint8_t pin);
	void GPIO_Clear(volatile uint8_t *pin_port, uint8_t pin);
	uint8_t GPIO_Read(volatile uint8_t * pin_port,uint8_t pin);
	uint8_t GPIO_Read(char port,uint8_t pin);
	void GPIO_Toggle(char pin_port,uint8_t pin);
	void GPIO_Toggle(volatile uint8_t * port, uint8_t pin);
#endif /* GPIO_H_ */