// this code provides the interface to the LCD module
// it also provides the putch function that enables support for printf
// it implements the functions shown in the prototypes below

// (2024/02/24)
// modified to be as flat as possible since latest MCC driver uses too much stack


//#include <xc.h>
//#include "mcc_generated_files/system/clock.h"
//#include <stdbool.h>
#include <stdio.h>
//#include <stdint.h>
//#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/system/system.h"

// prototypes
void initLCD(void);
void clearLCD(void);
void homeLCD(void);
void putcharLCD(void);
void static inline setaddrLCD(unsigned char);
void setline0LCD(void);
void setline1LCD(void);
void static inline sendcmd(unsigned char);
void sendchar( unsigned char);
void static inline writeLCD( unsigned char);
bool I2C1_Write(uint16_t, uint8_t*, size_t);

unsigned char LCDaddr=0;    // global address variable for line control

static	uint16_t address = (0x4e/2);
static	size_t length = 1;
static uint8_t data = 0;

//void static inline writeLCD(unsigned char byte)
//{
//static	uint16_t address = (0x4e / 2);
//static	uint16_t address = (0x4e/2);
//static	size_t length = 1;
//static	volatile uint8_t data = 0;

//data=(byte|0b1100);
//if( ! I2C1_Write(address, &data, length)) {};
//data=(byte|0b1000);
//if( ! I2C1_Write(address, &data, length)) {};

/* we need a function like this:
void I2C_WriteNBytes(i2c_address_t address, uint8_t* data, size_t len)
{
    while(!I2C_Open(address)); // sit here until we get the bus..
    I2C_SetBuffer(data,len);
    I2C_SetAddressNackCallback(NULL,NULL); //NACK polling?
    I2C_MasterWrite();
    while(I2C_BUSY == I2C_Close()); // sit here until finished.
}
*/

/*
 DOCTORS HATE HIM!!
 with this one simple trick, free up 10% of your program memory space
 */
void transmit_data(uint8_t DATA) {
    data=(DATA);
    if( ! I2C1_Write(address, &data, length)) {};
    __delay_ms(1);
}
// ----------------------------------------------------------- //

void initLCD(void)
{

// first get the bus into 4-bit mode
//data=(0x30);
//writeLCD(0x30);
transmit_data(0x30|0b1100);

transmit_data(0x30|0b1000);

//data=(0x30);
//writeLCD(0x30);
transmit_data(0x30|0b1100);

transmit_data(0x30|0b1000);

//data=(0x30);
//writeLCD(0x30);
transmit_data(0x30|0b1100);

transmit_data(0x30|0b1000);

//data=(0x20);
//writeLCD(0x20);
transmit_data(0x20|0b1100);

transmit_data(0x20|0b1000);


// now init the LCD
//    sendcmd(0b00101000);    // 001 function set DL=0(data length=4) N=1(2 lines) F=0(5x8 font)
transmit_data(0b00100000|0b1100);

transmit_data(0b00100000|0b1000);

transmit_data(0b10000000|0b1100);

transmit_data(0b10000000|0b1000);


//    sendcmd(0b00000001);
transmit_data(0b00000000|0b1100);

transmit_data(0b00000000|0b1000);

transmit_data(0b00010000|0b1100);

transmit_data(0b00010000|0b1000);


//    sendcmd(0b00001111);    // 00001 display on-off Display=1(on) Cursor=1(on) Blink=1(on)
transmit_data(0b00000000|0b1100);

transmit_data(0b00000000|0b1000);

transmit_data(0b11110000|0b1100);

transmit_data(0b11110000|0b1000);


//    sendcmd(0b00000110);    // 000001 entry mode set i/d=1(increment) S=0 (shift off)
transmit_data(0b00000000|0b1100);

transmit_data(0b00000000|0b1000);

transmit_data(0b01100000|0b1100);

transmit_data(0b01100000|0b1000);

// move this to main.c
/*
    setline0LCD();
    printf("LCD initialized");
    __delay_ms(1000);
    clearLCD();
    setline1LCD();
    printf("LCD initialized");
    __delay_ms(1000);
    clearLCD();
    setline0LCD();    
*/
}


//void static inline sendcmd(unsigned char cmd)
//{
//unsigned char val;
//val=(cmd&0xf0);
//writeLCD(val);

//val=(cmd&0xf)<<4;
//writeLCD(val);
//}


void clearLCD(void) {
//    sendcmd(0b00000001);     // 00000001 clear display
transmit_data(0b00000000|0b1100);
transmit_data(0b00000000|0b1000);

transmit_data(0b00010000|0b1100);
transmit_data(0b00010000|0b1000);

}

void homeLCD(void) {
//    sendcmd(0b00000010);    // 0000001x return home
transmit_data(0b00000000|0b1100);
transmit_data(0b00000000|0b1000);

transmit_data(0b00100000|0b1100);
transmit_data(0b00100000|0b1000);

    __delay_ms(2);          // needs more than 1.5 ms
}

void setline0LCD(void) {
//    setaddrLCD(0);  address=(0x00 | 0x80) write the 1000 then write the 0000
transmit_data(0b10000000|0b1100);
transmit_data(0b10000000|0b1000);

transmit_data(0b00000000|0b1100);
transmit_data(0b00000000|0b1000);

    LCDaddr=0;
}

void setline1LCD(void) {
//    setaddrLCD(0x40);  address=(0x40 | 0x80) write the 1100 then write the 0000
transmit_data(0b11000000|0b1100);
transmit_data(0b11000000|0b1000);

transmit_data(0b00000000|0b1100);
transmit_data(0b00000000|0b1000);

    LCDaddr=0x40;
}

//void static inline setaddrLCD(unsigned char address) {
//    unsigned char addr;
//    addr = address | 0x80;
//    sendcmd(addr);
//}

void putch(char character) {
char val;
// this putch allows using the printf function
//setaddrLCD(LCDaddr);    //put the cursor at the position for the next character

val=(character&0xf0);
val=val|0b0001;     //upper 4 bit of char and the RS bit=1
//writeLCD(val);
transmit_data(val|0b1100);
transmit_data(val|0b1000);

val=(unsigned char)(((character&0xf)<<4)|0b0001); //lower 4 bits with the RS=1

//writeLCD(val);
transmit_data(val|0b1100);
transmit_data(val|0b1000);


LCDaddr++;  // keep track of cursor position
if (LCDaddr == 0x10) {
    LCDaddr=0x40;   // wrap to next line
    }
if (LCDaddr == 0x50) {
    LCDaddr=0;      //wrap from second line back to first line
    }
}
