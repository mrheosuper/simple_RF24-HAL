/*
 * bit_macro.h
 *
 * Created: 06-Mar-19 10:31:33 AM
 *  Author: bacph
 */ 


#ifndef BIT_MACRO_H_
#define BIT_MACRO_H_
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#define bit(value) ((0x1UL) << value)
#endif /* BIT_MACRO_H_ */