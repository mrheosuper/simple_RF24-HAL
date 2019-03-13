
#include <avr/io.h>
#include <avr/interrupt.h>
#include "bit_macro.h"
#include "SPI.h"
//uint8_t read_data=0;
void SPI_master_begin()
{
	DDRB=(1<<5)|(1<<3)|(1<<2); // SCK, MOSI, SS
	SPCR=(1<<SPE)|(1<<MSTR); // enable SPI, master role
}
void SPI_setClkDiv(uint8_t clk)
{
	if(clk>0x07) return ;
	bitWrite(SPCR,1,bitRead(clk,1));
	bitWrite(SPCR,0,bitRead(clk,0));
	bitWrite(SPSR,0,bitRead(clk,2));
}
uint8_t SPI_transfer(uint8_t data)
{
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}
/*
uint8_t SPI_read()
{
	SPDR = 0xff;
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}*/
// TO DO: rewrite this function: DONE
void SPI_setMode(uint8_t spi_mode)
{
	if(spi_mode>3) return ; // there are only 4 mode
	bitWrite(SPCR,3,bitRead(spi_mode,1));
	bitWrite(SPCR,2,bitRead(spi_mode,0));
}
void SPI_setBitOrder(uint8_t bitOrder)
{
	if(bitOrder) bitWrite(SPCR,5,1);
	else bitWrite(SPCR,5,0);
}