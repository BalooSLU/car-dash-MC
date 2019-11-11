/*
* Car-Dash system
* 
* #git: https://github.com/BalooSLU/car-dash-MC
* #author: Simon Kesting
* #author: Martin Pyka
* #date: 18.10.19
* #file: main.c
*
* This project is a laboratory test from the university,
* where we recognize the speed of a motor. 
* The speed is then displayed on a graphic LCD display.
 */
//test

#include<stdio.h>
#include"tm4c1294ncpdt.h"

#define DISP_WIDTH 480
#define DISP_HIGHT 272
#define DISP_FULL (DISP_HIGHT * DISP_WIDTH)

void wait(int loops);
// Touch
void touch_write(unsigned char value);
unsigned int touch_read();
// Display
void write_command (unsigned char command);
void write_data (unsigned char data);
void initialise_ssd1963(void);
void window_set (unsigned int start_x, unsigned int end_x, unsigned int start_y, unsigned int end_y);
void draw_line (unsigned int start_x, unsigned int start_y, unsigned int length, unsigned int width, char red, char green, char blue);
void clear_display (void);

void main(void)
{
	int x, xpos, ypos;

	SYSCTL_RCGCGPIO_R = 0x2C08;								// Enable clock Port D, L, M
	while ((SYSCTL_PRGPIO_R & 0x2C08) ==0);					// GPIO Clock ready?

	// Port Initialization
	GPIO_PORTD_AHB_DEN_R = 0x1F;						// PortD digital enable
	GPIO_PORTL_DEN_R = 0x1F;							// PortL digital enable
	GPIO_PORTM_DEN_R = 0xFF;							// PortM digital enable
	GPIO_PORTP_DEN_R = 0x03;							// PortP digital enable

	GPIO_PORTD_AHB_DIR_R = 0x0D;						// PortD Input/Output
	GPIO_PORTL_DIR_R = 0x1F;							// PortL Input/Output
	GPIO_PORTM_DIR_R = 0xFF;							// PortM Input/Output
	GPIO_PORTM_DIR_R &= ~0x03;							// PortP Input/Output

	GPIO_PORTD_AHB_DATA_R &= 0xF7;							// Clk = 0
	GPIO_PORTL_DATA_R &= 0xF7;							// Clk = 0
	GPIO_PORTM_DATA_R &= 0xF7;							// Clk = 0
	GPIO_PORTP_DATA_R &= 0xF7;							// Clk = 0


	// B6 Example
	initialise_ssd1963();

	clear_display();

	draw_line(10, 10, 100, 100, 0x00, 0xFF, 0x00);

	window_set(240, 339, 136, 145);							// set position
	write_command(0x2C);									// write pixel command

	for (x = 0; x <= 1000; x++)								// set 8 pixels
	{
		write_data(0xff);	// red
		write_data(0x00);	// green
		write_data(0x00);	// blue
	}


	while (1)
	{
		touch_write (0xD0);									// Touch Command XPos read
		for (x=0; x<10; x++);								// Busy wait
		xpos = touch_read();								// xpos value read (0...4095)
		printf ("xpos = %5d     ",xpos);

		touch_write (0x90);									// Touch Command YPos read
		for (x=0; x<10; x++);								// Busy wait
		ypos = touch_read();								// ypos value read (0...4095)
		printf ("ypos = %5d\n    ",ypos);
	}
}

void clear_display (void)
{
	int x;

	window_set(0, DISP_WIDTH, 0, DISP_HIGHT);							// set position
	write_command(0x2C);												// write pixel command

	for (x = 0; x < DISP_FULL ; x++)						// set all pixels
	{
		write_data(0xff);	// red
		write_data(0xff);	// green
		write_data(0xff);	// blue
	}
}

void draw_line (unsigned int start_x, unsigned int start_y, unsigned int length, unsigned int width, char red, char green, char blue)
{
	int x;
	unsigned int number_pixel = length * width;

	unsigned int end_x = start_x + length - 1;
	unsigned int end_y = start_y + width - 1;

	window_set(start_x, end_x, start_y, end_y);
	write_command(0x2C);

	for (x = 0; x < number_pixel ; x++)						// set all pixels
		{
			write_data(red);	// red
			write_data(green);	// green
			write_data(blue);	// blue
		}

}


void wait(int loops)
{
	int tmp;
	for(tmp=0;tmp<loops;tmp++);		// Sekundentakt:= loops = 1600000
}

// Touch

void touch_write(unsigned char value)
{
	unsigned char i = 0x08;				// 8 bit command
	unsigned char x, DI;
	GPIO_PORTD_AHB_DATA_R &= 0xFB;		// CS=0

	while (i > 0)
	{
		DI = (value >> 7);
		if (DI == 0) {GPIO_PORTD_AHB_DATA_R &= 0xfe;}	// out bit = 0
		else {GPIO_PORTD_AHB_DATA_R |= 0x01;}			// out bit = 1
		value <<= 1;									// next value

		GPIO_PORTD_AHB_DATA_R |= 0x08;					// Clk = 1
		for (x=0; x<10; x++);
		GPIO_PORTD_AHB_DATA_R &= 0xf7;					// Clk = 0
		for (x=0; x<10; x++);

		i--;
	}
}


unsigned int touch_read()
{
	unsigned char i = 12;									// 12 Bit ADC
	unsigned int x, value = 0x00;

	while (i > 0)
	{
		value <<= 1;
		GPIO_PORTD_AHB_DATA_R |= 0x08;						// Clk = 1
		for (x=0; x<10; x++);
		GPIO_PORTD_AHB_DATA_R &= 0xf7;						// Clk = 0
		for (x=0; x<10; x++);
		value |= ((GPIO_PORTD_AHB_DATA_R >> 1) & 0x01);		// read value

		i--;
	}
	GPIO_PORTD_AHB_DATA_R |= 0x04;							// CS = 1
	return value;
}


// Display

void write_command (unsigned char command)
{
	GPIO_PORTL_DATA_R = 0x1F;			// Initial state
	GPIO_PORTL_DATA_R &= ~0x08;			// Chip select
	GPIO_PORTL_DATA_R &= ~0x04;			// Command mode select
	GPIO_PORTL_DATA_R &= ~0x02;			// Write state
	GPIO_PORTM_DATA_R = command;		// Write command byte
	GPIO_PORTL_DATA_R |= 0x02;			// No write state
	GPIO_PORTL_DATA_R |= 0x08;			// No chip select
}

void write_data (unsigned char data)
{
	GPIO_PORTL_DATA_R = 0x1F;			// Initial state
	GPIO_PORTL_DATA_R &= ~0x08;			// Chip select
	GPIO_PORTL_DATA_R |= 0x04;			// Data mode select
	GPIO_PORTL_DATA_R &= ~0x02;			// Write state
	GPIO_PORTM_DATA_R = data;			// Write data byte
	GPIO_PORTL_DATA_R |= 0x02;			// No write state
	GPIO_PORTL_DATA_R |= 0x08;			// No chip select
}

void initialise_ssd1963(void)
{

	GPIO_PORTL_DATA_R &= ~0x10;			// Hardware reset
	wait (1600);		// Wait more than 100us -> 1ms

	write_command(0x01);				// Software reset
	wait (16000);		// Wait more than 5ms -> 10ms

	write_command(0xE2);				// Set PLL Freq=100MHz
	write_data(0x1D);
	write_data(0x02);
	write_data(0x04);

	write_command(0xE0);				// Start PLL
	write_data(0x01);
	wait (1600);		// Wait more than 100us -> 1ms

	write_command(0xE0);	//Lock PLL
	write_data(0x03);
	wait(1600);			// Wait more than 100us -> 1ms

	write_command(0x01);	//Software reset
	wait(16000);		// Wait more than 5ms -> 10ms

	write_command(0xE6);	// Set LCD Pixel Clock 9MHz;
	write_data(0x01);
	write_data(0x70);
	write_data(0xA3);

	write_command(0xB0);	// Set LCD Panel mode
	write_data(0x20);		// TFT panel 24bit...
	write_data(0x00);		// TFT mode
	write_data(0x01);		// Horizontal size 480-1
	write_data(0xDF);		// Horizontal size 480-1
	write_data(0x01);		// Vertical size 272-1
	write_data(0x0F);		// Vertical size 272-1
	write_data(0x00);		// even/odd line RGB

	write_command(0xB4);	// set Horizontal period
	write_data(0x02);		// set HT total pixel=531
	write_data(0x13);		
	write_data(0x00);		// Set Horizontal sync pule start pos=43
	write_data(0x2B);		
	write_data(0x0A);		// set horiz.sync pulse with =10
	write_data(0x00);		// set horiz.sync pulse start pos=8
	write_data(0x08);		
	write_data(0x00);

	write_command(0xB6);	// set Vertical period
	write_data(0x01);		// set VT lines = 288
	write_data(0x20);
	write_data(0x00);		// Set VPS = 12
	write_data(0x0C);
	write_data(0x0A);		// set vert.sync pulse with = 10
	write_data(0x00);		// set vert.sync pulse start = 4
	write_data(0x04);

	write_command(0x36);	// Flip Dysplay - necessary to match with Touch Display
	write_data(0x03);

	write_command(0xF0);	// Set pixel data format = 8bit
	write_data(0x00);

	write_command(0x29);	// Set display on
}

void window_set (unsigned int start_x, unsigned int end_x, unsigned int start_y, unsigned int end_y)
{
	write_command(0x2A);		// SET page address
	write_data((start_x) >> 8);	// SET start page adress (HB)
	write_data(start_x);		// SET start page adress (LB)
	write_data((end_x) >> 8);		// SET end page adress
	write_data(end_x);			// SET end page adress

	write_command(0x2B);		// SET page address
	write_data((start_y) >> 8);	// SET start column adress (HB)
	write_data(start_y);		// SET start column adress (LB)
	write_data((end_y) >> 8);		// SET end column adress
	write_data(end_y);			// SET end column adress

}



