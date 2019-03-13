#include "GPIO.h"
#include "SPI.h"
#include "Systick.h"
#include <avr/io.h>
#include "math.h"
#include "RF24.h"
#include "nRF24L01.h"
#include "util/delay.h"
#include "stdio.h"
#include "avr/interrupt.h"
void delay_ms(uint32_t ms){
	for(uint32_t i=0;i<ms;i++) _delay_ms(1);
}
void delay_us(uint32_t us)
{
	for(uint32_t i=0;i<us;i++) _delay_us(1);
}
uint8_t NRFLite::init(uint8_t radioId, char cePin_port, uint8_t cePin, char csnPin_port, uint8_t csnPin, Bitrates bitrate , uint8_t channel)
{
	_useTwoPinspiTransfer = 0;
	_cePin = cePin;
	_csnPin = csnPin;
	_cePin_port=cePin_port;
	_csnPin_port=csnPin_port;
	
	// Default states for the radio pins.  When CSN is LOW the radio listens to SPI communication,
	// so we operate most of the time with CSN HIGH.
	GPIO_Mode(_csnPin_port,_csnPin,OUTPUT);
	GPIO_Mode(_cePin_port,_cePin,OUTPUT);
	GPIO_Write(_csnPin_port, _csnPin,1);
	
	// Setup the microcontroller for SPI communication with the radio.
	/*
	#if defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
	pinMode(USI_DI, INPUT ); digitalWrite(USI_DI, HIGH);
	pinMode(USI_DO, OUTPUT); digitalWrite(USI_DO, LOW);
	pinMode(USI_SCK, OUTPUT); digitalWrite(USI_SCK, LOW);
	#else
	*/
	// Arduino SPI makes SS (D10 on ATmega328) an output and sets it HIGH.  It must remain an output
	// for Master SPI operation, but in case it started as LOW, we'll set it back.
	SPI_master_begin();
	SPI_setClkDiv(SPI_CLOCK_DIV4);
	SPI_setBitOrder(MSBFIRST);
	SPI_setMode(SPI_MODE0);
	//#endif

	uint8_t success = prepForRx(radioId, bitrate, channel);
	return success;
}
uint8_t NRFLite::initTwoPin(uint8_t radioId, char momiPin_port, uint8_t momiPin, char sckPin_port, uint8_t sckPin, Bitrates bitrate, uint8_t channel)
{
	_useTwoPinspiTransfer = 1;
	_cePin = sckPin;
	_csnPin = sckPin;

	// Default states for the 2 multiplexed pins.
	GPIO_Mode(momiPin_port, momiPin, INPUT);
	GPIO_Mode(sckPin_port,sckPin, OUTPUT); 
	GPIO_Write(sckPin_port,sckPin, HIGH);

	// These port and mask functions are in Arduino.h, e.g. arduino-1.8.1\hardware\arduino\avr\cores\arduino\Arduino.h
	// Direct port manipulation is used since timing is critical for the multiplexed SPI bit-banging.
	switch(momiPin_port)
	{
		case 'B':
		case 'b':
		_momi_PORT=&PORTB;
		_momi_DDR=&DDRB;
		_momi_PIN=&PINB;
		break;
		case 'C':
		case 'c':
		_momi_PORT=&PORTC;
		_momi_DDR=&DDRC;
		_momi_PIN=&PINC;
		break;
		case 'D':
		case 'd':
		_momi_PORT=&PORTD;
		_momi_DDR=&DDRD;
		_momi_PIN=&PIND;
		break;
	}
	//_momi_PORT = portOutputRegister(digitalPinToPort(momiPin));
	//_momi_DDR = portModeRegister(digitalPinToPort(momiPin));
	//_momi_PIN = portInputRegister(digitalPinToPort(momiPin));
	_momi_MASK = (1<<momiPin);
	switch(sckPin_port)
	{
		case 'B':
		case 'b':
		_sck_PORT =&PORTB;
		//_sck_DDR =DDRB;
		//_sck_PIN =PINB;
		break;
		case 'C':
		case 'c':
		_sck_PORT =&PORTC;
		//_sck_DDR =DDRC;
		//_sck_PIN =PINC;
		break;
		case 'D':
		case 'd':
		_sck_PORT =&PORTD;
		//_sck_DDR =DDRD
		//_sck_PIN =PIND;
		break;
	}
	//_sck_PORT = portOutputRegister(digitalPinToPort(sckPin));
	_sck_MASK = 1<<sckPin;

	return prepForRx(radioId, bitrate, channel);
}

void NRFLite::addAckData(void *data, uint8_t length, uint8_t removeExistingAcks)
{
	if (removeExistingAcks)
	{
		spiTransfer(WRITE_OPERATION, FLUSH_TX, NULL, 0); // Clear the TX FIFO buffer.
	}
	
	// Add the packet to the TX FIFO buffer for pipe 1, the pipe used to receive packets from radios that
	// send us data.  When we receive the next transmission from a radio, we'll provide this ACK data in the
	// auto-acknowledgment packet that goes back.
	spiTransfer(WRITE_OPERATION, (W_ACK_PAYLOAD | 1), data, length);
}

uint8_t NRFLite::hasAckData()
{
	// If we have a pipe 0 packet sitting at the top of the RX FIFO buffer, we have auto-acknowledgment data.
	// We receive ACK data from other radios using the pipe 0 address.
	if (getPipeOfFirstRxPacket() == 0)
	{
		return getRxPacketLength(); // Return the length of the data packet in the RX FIFO buffer.
	}
	else
	{
		return 0;
	}
}

uint8_t NRFLite::hasData(uint8_t usingInterrupts)
{
	// If using the same pins for CE and CSN, we need to ensure CE is left HIGH long enough to receive data.
	// If we don't limit the calling program, CE could be LOW so often that no data packets can be received.
	int8_t mustStopRadioRxToCheckForData = _cePin == _csnPin;
	if (mustStopRadioRxToCheckForData)
	{
		if (usingInterrupts)
		{
			// Since the calling program is using interrupts, we can trust that it only calls hasData when an
			// interrupt is received, meaning only when data is received.  So we don't need to limit it.
		}
		else
		{
			uint8_t giveRadioMoreTimeToReceive = fabs(micros() - _microsSinceLastDataCheck) < _maxHasDataIntervalMicros;
			if (giveRadioMoreTimeToReceive)
			{
				return 0; // Return to prevent the calling program from forcing us to bring CE low, making the radio stop receiving.
			}
			else
			{
				_microsSinceLastDataCheck = micros();
			}
		}
	}
	
	// Ensure radio is powered on and in RX mode in case the radio was powered down or in TX mode.
	uint8_t originalConfigReg = readRegister(CONFIG);
	uint8_t newConfigReg = originalConfigReg | _BV(PWR_UP) | _BV(PRIM_RX);
	if (originalConfigReg != newConfigReg)
	{
		writeRegister(CONFIG, newConfigReg);
	}
	
	// Ensure we're listening for packets.
	GPIO_Write(_cePin_port,_cePin, HIGH);
	
	// If the radio was initially powered off, wait for it to turn on.
	if ((originalConfigReg & _BV(PWR_UP)) == 0)
	{
		delay_us(POWERDOWN_TO_RXTX_MODE_MICROS);
	}

	// If we have a pipe 1 packet sitting at the top of the RX FIFO buffer, we have data.
	if (getPipeOfFirstRxPacket() == 1)
	{
		return getRxPacketLength(); // Return the length of the data packet in the RX FIFO buffer.
	}
	else
	{
		return 0;
	}
}

uint8_t NRFLite::hasDataISR()
{
	// This method should be used inside an interrupt handler for the radio's IRQ pin to bypass
	// the limit on how often the radio is checked for data.  This optimization greatly increases
	// the receiving bitrate when CE and CSN share the same pin.
	return hasData(1);
}

void NRFLite::readData(void *data)
{
	// Determine length of data in the RX FIFO buffer and read it.
	uint8_t dataLength;
	spiTransfer(READ_OPERATION, R_RX_PL_WID, &dataLength, 1);
	spiTransfer(READ_OPERATION, R_RX_PAYLOAD, data, dataLength);
	
	// Clear data received flag.
	uint8_t statusReg = readRegister(STATUS_NRF);
	if (statusReg & _BV(RX_DR))
	{
		writeRegister(STATUS_NRF, statusReg | _BV(RX_DR));
	}
}
///////////TO DO: CHECK WHILE(1) LOOP, can't escape
uint8_t NRFLite::send(uint8_t toRadioId, void *data, uint8_t length, SendType sendType)
{
	prepForTx(toRadioId, sendType);

	// Clear any previously asserted TX success or max retries flags.
	uint8_t statusReg = readRegister(STATUS_NRF);
	if (statusReg & _BV(TX_DS) || statusReg & _BV(MAX_RT))
	{
		writeRegister(STATUS_NRF, statusReg | _BV(TX_DS) | _BV(MAX_RT));
	}
	
	// Add data to the TX FIFO buffer, with or without an ACK request.
	if (sendType == NO_ACK) { spiTransfer(WRITE_OPERATION, W_TX_PAYLOAD_NO_ACK, data, length); }
	else                    { spiTransfer(WRITE_OPERATION, W_TX_PAYLOAD       , data, length); }

	// Start transmission.
	// If we have separate pins for CE and CSN, CE will be LOW and we must pulse it to start transmission.
	// If we use the same pin for CE and CSN, CE will already be HIGH and transmission will have started
	// when data was loaded into the TX FIFO.
	uint8_t txPulseIsNeeded = _cePin != _csnPin;
	if (txPulseIsNeeded)
	{
		GPIO_Write(_cePin_port,_cePin, HIGH);
		delay_us(CE_TRANSMISSION_MICROS);
		GPIO_Write(_cePin_port,_cePin, LOW);
	}
	
	// Wait for transmission to succeed or fail.
	//while (1)
	//{
		delay_us(_transmissionRetryWaitMicros);
		statusReg = readRegister(STATUS_NRF);
		
		if (statusReg & _BV(TX_DS))
		{
			writeRegister(STATUS_NRF, statusReg | _BV(TX_DS));  // Clear TX success flag.
			return 1;                                           // Return success.
			//break;
		}
		else if (statusReg & _BV(MAX_RT))
		{
			spiTransfer(WRITE_OPERATION, FLUSH_TX, NULL, 0);    // Clear TX FIFO buffer.
			writeRegister(STATUS_NRF, statusReg | _BV(MAX_RT)); // Clear flag which indicates max retries has been reached.
			return 0;                                           // Return failure.
			//break;
		}
	//}
	return 1;
}

void NRFLite::startSend(uint8_t toRadioId, void *data, uint8_t length, SendType sendType)
{
	prepForTx(toRadioId, sendType);
	
	// Add data to the TX FIFO buffer, with or without an ACK request.
	if (sendType == NO_ACK) { spiTransfer(WRITE_OPERATION, W_TX_PAYLOAD_NO_ACK, data, length); }
	else                    { spiTransfer(WRITE_OPERATION, W_TX_PAYLOAD       , data, length); }
	
	// Start transmission.
	uint8_t txPulseIsNeeded = _cePin != _csnPin;
	if (txPulseIsNeeded)
	{
		GPIO_Write(_cePin_port,_cePin, HIGH);
		delay_us(CE_TRANSMISSION_MICROS);
		GPIO_Write(_cePin_port,_cePin, LOW);
	}
}

void NRFLite::whatHappened(uint8_t &txOk, uint8_t &txFail, uint8_t &rxReady)
{
	uint8_t statusReg = readRegister(STATUS_NRF);
	
	txOk = statusReg & _BV(TX_DS);
	txFail = statusReg & _BV(MAX_RT);
	rxReady = statusReg & _BV(RX_DR);
	
	// When we need to see interrupt flags, we disable the logic here which clears them.
	// Programs that have an interrupt handler for the radio's IRQ pin will use 'whatHappened'
	// and if we don't disable this logic, it's not possible for us to check these flags.
	if (_resetInterruptFlags)
	{
		writeRegister(STATUS_NRF, statusReg | _BV(TX_DS) | _BV(MAX_RT) | _BV(RX_DR));
	}
}

void NRFLite::powerDown()
{
	// If we have separate CE and CSN pins, we can gracefully turn off Tx or Rx operation.
	if (_cePin != _csnPin) { GPIO_Write(_cePin_port,_cePin, LOW); }
	
	// Turn off the radio.
	writeRegister(CONFIG, readRegister(CONFIG) & ~_BV(PWR_UP));
}
/*
void NRFLite::printDetails()
{
	printRegister("CONFIG", readRegister(CONFIG));
	printRegister("EN_AA", readRegister(EN_AA));
	printRegister("EN_RXADDR", readRegister(EN_RXADDR));
	printRegister("SETUP_AW", readRegister(SETUP_AW));
	printRegister("SETUP_RETR", readRegister(SETUP_RETR));
	printRegister("RF_CH", readRegister(RF_CH));
	printRegister("RF_SETUP", readRegister(RF_SETUP));
	printRegister("STATUS", readRegister(STATUS_NRF));
	printRegister("OBSERVE_TX", readRegister(OBSERVE_TX));
	printRegister("RX_PW_P0", readRegister(RX_PW_P0));
	printRegister("RX_PW_P1", readRegister(RX_PW_P1));
	printRegister("FIFO_STATUS", readRegister(FIFO_STATUS));
	printRegister("DYNPD", readRegister(DYNPD));
	printRegister("FEATURE", readRegister(FEATURE));
	
	uint8_t data[5];
	
	String msg = "TX_ADDR ";
	readRegister(TX_ADDR, &data, 5);
	for (uint8_t i = 0; i < 4; i++) { msg += data[i]; msg += ','; }
	msg += data[4];
	
	msg += "\nRX_ADDR_P0 ";
	readRegister(RX_ADDR_P0, &data, 5);
	for (uint8_t i = 0; i < 4; i++) { msg += data[i]; msg += ','; }
	msg += data[4];

	msg += "\nRX_ADDR_P1 ";
	readRegister(RX_ADDR_P1, &data, 5);
	for (uint8_t i = 0; i < 4; i++) { msg += data[i]; msg += ','; }
	msg += data[4];

	debugln(msg);
}
*/

/////////////////////
// Private methods //
/////////////////////

uint8_t NRFLite::getPipeOfFirstRxPacket()
{
	// The pipe number is bits 3, 2, and 1.  So B1110 masks them and we shift right by 1 to get the pipe number.
	// 000-101 = Data Pipe Number
	//     110 = Not Used
	//     111 = RX FIFO Empty
	return (readRegister(STATUS_NRF) & 0B1110) >> 1;
}

uint8_t NRFLite::getRxPacketLength()
{
	// Read the length of the first data packet sitting in the RX FIFO buffer.
	uint8_t dataLength;
	spiTransfer(READ_OPERATION, R_RX_PL_WID, &dataLength, 1);

	// Verify the data length is valid (0 - 32 bytes).
	if (dataLength > 32)
	{
		spiTransfer(WRITE_OPERATION, FLUSH_RX, NULL, 0); // Clear invalid data in the RX FIFO buffer.
		writeRegister(STATUS_NRF, readRegister(STATUS_NRF) | _BV(TX_DS) | _BV(MAX_RT) | _BV(RX_DR));
		return 0;
	}
	else
	{
		return dataLength;
	}
}

uint8_t NRFLite::prepForRx(uint8_t radioId, Bitrates bitrate, uint8_t channel)
{
	_resetInterruptFlags = 1;

	delay_ms(OFF_TO_POWERDOWN_MILLIS);

	// Valid channel range is 2400 - 2525 MHz, in 1 MHz increments.
	if (channel > 125) { channel = 125; }
	writeRegister(RF_CH, channel);

	// Transmission speed, retry times, and output power setup.
	// For 2 Mbps or 1 Mbps operation, a 500 uS retry time is necessary to support the max ACK packet size.
	// For 250 Kbps operation, a 1500 uS retry time is necessary.
	if (bitrate == BITRATE2MBPS)
	{
		writeRegister(RF_SETUP, 0B00001110);   // 2 Mbps, 0 dBm output power
		writeRegister(SETUP_RETR, 0B00011111); // 0001 =  500 uS between retries, 1111 = 15 retries
		_maxHasDataIntervalMicros = 600;
		_transmissionRetryWaitMicros = 600;
	}
	else if (bitrate == BITRATE1MBPS)
	{
		writeRegister(RF_SETUP, 0B00000110);   // 1 Mbps, 0 dBm output power
		writeRegister(SETUP_RETR, 0B00011111); // 0001 =  500 uS between retries, 1111 = 15 retries
		_maxHasDataIntervalMicros = 1200;
		_transmissionRetryWaitMicros = 1000;
	}
	else
	{
		writeRegister(RF_SETUP, 0B00100110);   // 250 Kbps, 0 dBm output power
		writeRegister(SETUP_RETR, 0B01011111); // 0101 = 1500 uS between retries, 1111 = 15 retries
		_maxHasDataIntervalMicros = 8000;
		_transmissionRetryWaitMicros = 1500;
	}

	// Assign this radio's address to RX pipe 1.  When another radio sends us data, this is the address
	// it will use.  We use RX pipe 1 to store our address since the address in RX pipe 0 is reserved
	// for use with auto-acknowledgment packets.
	uint8_t address[5] = { 1, 2, 3, 4, radioId };
	writeRegister(RX_ADDR_P1, &address, 5);

	// Enable dynamically sized packets on the 2 RX pipes we use, 0 and 1.
	// RX pipe address 1 is used to for normal packets from radios that send us data.
	// RX pipe address 0 is used to for auto-acknowledgment packets from radios we transmit to.
	writeRegister(DYNPD, _BV(DPL_P0) | _BV(DPL_P1));

	// Enable dynamically sized payloads, ACK payloads, and TX support with or without an ACK request.
	writeRegister(FEATURE, _BV(EN_DPL) | _BV(EN_ACK_PAY) | _BV(EN_DYN_ACK));

	// Ensure RX FIFO and TX FIFO buffers are empty.  Each buffer can hold 3 packets.
	spiTransfer(WRITE_OPERATION, FLUSH_RX, NULL, 0);
	spiTransfer(WRITE_OPERATION, FLUSH_TX, NULL, 0);

	// Clear any interrupts.
	uint8_t statusReg = readRegister(STATUS_NRF);
	writeRegister(STATUS_NRF, statusReg | _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT));

	// Power on the radio and start listening, delaying to allow startup to complete.
	uint8_t newConfigReg = _BV(PWR_UP) | _BV(PRIM_RX) | _BV(EN_CRC);
	writeRegister(CONFIG, newConfigReg);
	GPIO_Write(_cePin_port,_cePin, HIGH);
	delay_us(POWERDOWN_TO_RXTX_MODE_MICROS);

	uint8_t configRegisterWasSet = readRegister(CONFIG) == newConfigReg;
	return configRegisterWasSet;
}

void NRFLite::prepForTx(uint8_t toRadioId, SendType sendType)
{
	// TX pipe address sets the destination radio for the data.
	// RX pipe 0 is special and needs the same address in order to receive ACK packets from the destination radio.
	uint8_t address[5] = { 1, 2, 3, 4, toRadioId };
	writeRegister(TX_ADDR, &address, 5);
	writeRegister(RX_ADDR_P0, &address, 5);

	// Ensure radio is powered on and ready for TX operation.
	uint8_t originalConfigReg = readRegister(CONFIG);
	uint8_t newConfigReg = originalConfigReg & (~_BV(PRIM_RX)) | _BV(PWR_UP);
	if (originalConfigReg != newConfigReg)
	{
		// In case the radio was in RX mode (powered on and listening), we'll put the radio into
		// Standby-I mode by setting CE LOW.  The radio cannot transition directly from RX to TX,
		// it must go through Standby-I first.
		if ((originalConfigReg & _BV(PRIM_RX)) && (originalConfigReg & _BV(PWR_UP)))
		{
			if (GPIO_Read(_cePin_port,_cePin) == HIGH) { GPIO_Write(_cePin_port,_cePin, LOW); }
		}
		
		writeRegister(CONFIG, newConfigReg);
		delay_us(POWERDOWN_TO_RXTX_MODE_MICROS);
	}
	
	// If RX FIFO buffer is full and we require an ACK, clear it so we can receive the ACK response.
	uint8_t fifoReg = readRegister(FIFO_STATUS);
	if (fifoReg & _BV(RX_FULL) && sendType == REQUIRE_ACK)
	{
		spiTransfer(WRITE_OPERATION, FLUSH_RX, NULL, 0);
	}
	
	// If TX FIFO buffer is full, we'll send a packet.
	if (fifoReg & _BV(FIFO_FULL))
	{
		// Disable interrupt flag reset logic in 'whatHappened' so we can react to the flags here.
		_resetInterruptFlags = 0;
		uint8_t statusReg = readRegister(STATUS_NRF);
		
		// While the TX FIFO buffer is full...
		while (statusReg & _BV(TX_FULL))
		{
			// Try sending a packet.
			GPIO_Write(_cePin_port,_cePin, HIGH);
			delay_us(CE_TRANSMISSION_MICROS);
			GPIO_Write(_cePin_port,_cePin, LOW);
			
			delay_us(_transmissionRetryWaitMicros);
			statusReg = readRegister(STATUS_NRF);
			
			if (statusReg & _BV(TX_DS))
			{
				writeRegister(STATUS_NRF, statusReg | _BV(TX_DS));   // Clear TX success flag.
			}
			else if (statusReg & _BV(MAX_RT))
			{
				spiTransfer(WRITE_OPERATION, FLUSH_TX, NULL, 0); // Clear TX FIFO buffer.
				writeRegister(STATUS_NRF, statusReg | _BV(MAX_RT));  // Clear flag which indicates max retries has been reached.
			}
		}
		
		_resetInterruptFlags = 1;
	}
}

uint8_t NRFLite::readRegister(uint8_t regName)
{
	uint8_t data;
	readRegister(regName, &data, 1);
	return data;
}

void NRFLite::readRegister(uint8_t regName, void *data, uint8_t length)
{
	spiTransfer(READ_OPERATION, (R_REGISTER | (REGISTER_MASK & regName)), data, length);
}

void NRFLite::writeRegister(uint8_t regName, uint8_t data)
{
	writeRegister(regName, &data, 1);
}

void NRFLite::writeRegister(uint8_t regName, void *data, uint8_t length)
{
	spiTransfer(WRITE_OPERATION, (W_REGISTER | (REGISTER_MASK & regName)), data, length);
}

void NRFLite::spiTransfer(SpiTransferType transferType, uint8_t regName, void* data, uint8_t length)
{
	uint8_t* intData = reinterpret_cast<uint8_t*>(data);

	if (_useTwoPinspiTransfer)
	{
		GPIO_Write(_csnPin_port,_csnPin, LOW);              // Signal radio it should begin listening to the SPI bus.
		delay_us(CSN_DISCHARGE_MICROS); // Allow capacitor on CSN pin to discharge.
		//noInterrupts();                          // Timing is critical so interrupts are disabled during the bit-bang transfer.
		cli();
		twoPinTransfer(regName);
		for (uint8_t i = 0; i < length; ++i) {
			uint8_t newData = twoPinTransfer(intData[i]);
			if (transferType == READ_OPERATION) { intData[i] = newData; }
		}
		//interrupts();
		sei();
		GPIO_Write(_csnPin_port,_csnPin, HIGH);             // Stop radio from listening to the SPI bus.
		delay_us(CSN_DISCHARGE_MICROS); // Allow capacitor on CSN pin to recharge.
	}
	else
	{
		GPIO_Write(_csnPin_port,_csnPin, LOW);  // Signal radio it should begin listening to the SPI bus.
		#if defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
		// ATtiny transfer with USI.
		usiTransfer(regName);
		for (uint8_t i = 0; i < length; ++i) {
			uint8_t newData = usiTransfer(intData[i]);
			if (transferType == READ_OPERATION) { intData[i] = newData; }
		}
		#else
		
		// Transfer with the Arduino SPI library.
		//SPI.beginTransaction(SPISettings(NRF_SPICLOCK, MSBFIRST, SPI_MODE0));
		SPI_transfer(regName);
		for (uint8_t i = 0; i < length; ++i) {
			uint8_t newData = SPI_transfer(intData[i]);
			if (transferType == READ_OPERATION) { intData[i] = newData; }
		}
		//SPI.endTransaction();
		#endif
		GPIO_Write(_csnPin_port,_csnPin, HIGH); // Stop radio from listening to the SPI bus.
	}
}
/*
we dont need for now
uint8_t NRFLite::usiTransfer(uint8_t data)
{
	#if defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
	
	USIDR = data;
	USISR = _BV(USIOIF);
	
	while ((USISR & _BV(USIOIF)) == 0)
	{
		USICR = _BV(USIWM0) | _BV(USICS1) | _BV(USICLK) | _BV(USITC);
	}
	
	return USIDR;
	
	#endif
}
*/
uint8_t NRFLite::twoPinTransfer(uint8_t data)
{
	uint8_t byteFromRadio=0;
	uint8_t bits = 8;
	
	do
	{
		byteFromRadio <<= 1; // Shift the byte we are building to the left.
		
		if (*_momi_PIN & _momi_MASK) { byteFromRadio++; } // Read bit from radio on MOMI pin.  If HIGH, set bit position 0 of our byte to 1.
		*_momi_DDR |= _momi_MASK;                         // Change MOMI to be an OUTPUT pin.
		
		if (data & 0x80) { *_momi_PORT |=  _momi_MASK; }  // Set MOMI HIGH if bit position 7 of the byte we are sending is 1.

		*_sck_PORT |= _sck_MASK;  // Set SCK HIGH to transfer the bit to the radio.  CSN will remain LOW while the capacitor begins charging.
		*_sck_PORT &= ~_sck_MASK; // Set SCK LOW.  CSN will have remained LOW due to the capacitor.
		
		*_momi_PORT &= ~_momi_MASK; // Set MOMI LOW.
		*_momi_DDR &= ~_momi_MASK;  // Change MOMI back to an INPUT.  Since we previously ensured it was LOW, its PULLUP resistor will never
		// be enabled which would prevent MOMI from fully reaching a LOW state.

		data <<= 1; // Shift the byte we are sending to the left.
	}
	while (--bits);
	
	return byteFromRadio;
}
/*
void NRFLite::printRegister(char name[], uint8_t reg)
{
	String msg = name;
	msg += " ";

	uint8_t i = 8;
	do
	{
		msg += bitRead(reg, --i);
	}
	while (i);

	debugln(msg);
}
*/