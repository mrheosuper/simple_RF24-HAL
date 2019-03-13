
#ifndef SPI_H_
#define SPI_H_
	#define SPI_CLOCK_DIV2 0x04
	#define SPI_CLOCK_DIV4 0x00
	#define SPI_CLOCK_DIV8 0x05
	#define SPI_CLOCK_DIV16 0x01
	#define SPI_CLOCK_DIV32 0x06
	#define SPI_CLOCK_DIV64 0x02
	#define SPI_CLOCK_DIV128 0x03
	
	#define SPI_MODE0 0x00
	#define SPI_MODE1 0x01
	#define SPI_MODE2 0x02
	#define SPI_MODE3 0x03
	
	#define MSBFIRST 0x00
	#define LSBFIRST 0x01
	void SPI_master_begin();  // joining SPI bus as master
	void SPI_setClkDiv(uint8_t clk); // set SPI clock divider, para defined above
	void SPI_setMode(uint8_t spi_mode); //set SPI mode, read SPI wiki http://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus#Clock_polarity_and_phase
	uint8_t SPI_transfer(uint8_t data); // send a byte on SPI bus, return data read;
	uint8_t SPI_read(); // read data from SPI bus, not necessary
	void SPI_setBitOrder(uint8_t bitOrder); // set LSB or MSB send first
#endif /* SPI_H_ */