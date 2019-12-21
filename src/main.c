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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "tm4c1294ncpdt.h"
#include "startup_clear.h"
#include "direction_map.h"
#include "number_map.h"
#include "term_map.h"

#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"

#define DISP_WIDTH 480
#define DISP_HIGHT 272
#define DISP_FULL (DISP_HIGHT * DISP_WIDTH)

#define SONE GPIO_PIN_0
#define STWO GPIO_PIN_1
#define SONE_DIR 'V'
#define STWO_DIR 'R'
#define DEFAULT_DIR 'P'
#define POINTS_ANALOG_X_MAX 414 // Maximal number of points in the x-achsis for the analog Area
#define TOGGLE5 (GPIO_PORTC_AHB_DATA_R ^= GPIO_PIN_5)
#define TOGGLE4 (GPIO_PORTC_AHB_DATA_R ^= GPIO_PIN_4)

//           ---System---
void pin_init(void);
void wait(int loops);
//           ---Touch---
void touch_write(uint8_t value);
unsigned int touch_read();
//           ---Display---
void startUp_display(void);
void draw_komma(void);
void draw_term(uint8_t mode);
void draw_direction(uint8_t direction);
void draw_number(uint8_t number, uint8_t mode);
void write_command(uint8_t command);
void write_data(uint8_t data);
void initialise_ssd1963(void);
void window_set(unsigned int start_x, unsigned int end_x, unsigned int start_y, unsigned int end_y);
void draw_line(unsigned int start_x, unsigned int start_y, unsigned int length, unsigned int width);
void clear_display(void);
void clear_pixel(void);
//           ---RPM---
void rpm_init(void);
void rpm_sone_handler(void);
void rpm_stwo_handler(void);
void rpm_time_handler(void);
void rpm_speed(void);
void calculate(uint32_t counter);
void drawIt(uint8_t mode);

volatile uint8_t S = 0;				 // Mutex for gspeed write
volatile uint8_t O = 0;				 // Mutex for gspeed read
volatile uint8_t S2 = 0;			 // Mutex for gcount
volatile uint32_t gcount = 0;		 // Global counter
volatile uint8_t gdir = DEFAULT_DIR; // Global direction
volatile uint32_t gspeed = 0;		 // Global speed
int main(void)
{
	IntMasterDisable();
	unsigned long sysclock_read_out;
	sysclock_read_out = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
											SYSCTL_OSC_MAIN |
											SYSCTL_USE_PLL |
											SYSCTL_CFG_VCO_480),
										   10000000); //Set clock to 100MHz
	SysTickPeriodSet(sysclock_read_out / 2);		  // Set period for systick to 10ms
	SysTickIntRegister(rpm_time_handler);			  // Set handler for systick

	SysTickEnable();	// Enable systicks
	SysTickIntEnable(); // Enable systickinterrupts

	pin_init();			  // Initialize pins for display
	initialise_ssd1963(); // Initialize display
	clear_display();	  // Clear display
	startUp_display();	// Displays startup frame
	rpm_init();			  // Initialize interrupt pins and handler

	IntMasterEnable();
	while (1)
	{
		drawIt(MODE_TERM_KMH); // BESTIMMEN WIE LANGE DIE FUNKTION BRAUCHT
							   //wait(10000);
	}
}
void pin_init(void)
{
	SYSCTL_RCGCGPIO_R = 0x2C08; // Enable clock Port D, L, M
	while ((SYSCTL_PRGPIO_R & 0x2C08) == 0)
		; // GPIO Clock ready?

	// Port Initialization
	GPIO_PORTD_AHB_DEN_R = 0x1F; // PortD digital enable
	GPIO_PORTL_DEN_R = 0x1F;	 // PortL digital enable
	GPIO_PORTM_DEN_R = 0xFF;	 // PortM digital enable

	GPIO_PORTD_AHB_DIR_R = 0x0D; // PortD Input/Output
	GPIO_PORTL_DIR_R = 0x1F;	 // PortL Input/Output
	GPIO_PORTM_DIR_R = 0xFF;	 // PortM Input/Output

	GPIO_PORTD_AHB_DATA_R &= 0xF7; // Clk = 0
	GPIO_PORTL_DATA_R &= 0xF7;	 // Clk = 0
	GPIO_PORTM_DATA_R &= 0xF7;	 // Clk = 0
}

void rpm_init(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);		//for speed calculation
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);		//for debug
	while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOC)) //short wait for clock
	{
	}
	GPIOPinTypeGPIOOutput(GPIO_PORTC_BASE, GPIO_PIN_5 | GPIO_PIN_4); // for debug Toggle

	//GPIO Interrrupt init
	GPIOPinTypeGPIOInput(GPIO_PORTP_BASE, GPIO_PIN_0 | GPIO_PIN_1); // for Speed calculation
	GPIOIntTypeSet(GPIO_PORTP_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_RISING_EDGE);
	IntRegister(INT_GPIOP0, rpm_sone_handler); //register Interrupt handler
	IntRegister(INT_GPIOP1, rpm_stwo_handler); //register Interrupt handler

	GPIOIntClear(GPIO_PORTP_BASE, GPIO_PIN_0);						 //clean Interrupt flag
	GPIOIntClear(GPIO_PORTP_BASE, GPIO_PIN_1);						 //clean interrupt flag
	IntPrioritySet(INT_GPIOP0, 0);									 //Set high priority
	IntPrioritySet(INT_GPIOP1, 0);									 //Set high priority
	GPIOIntEnable(GPIO_PORTP_BASE, GPIO_INT_PIN_0 | GPIO_INT_PIN_1); //Enable GPIOs as interrupts
	IntEnable(INT_GPIOP0);											 // Enable Interrupts
	IntEnable(INT_GPIOP1);											 // Enable Interrupts
}

void rpm_sone_handler(void)
{
	IntMasterDisable();
	TOGGLE5;
	GPIOIntClear(GPIO_PORTP_BASE, SONE);	 //clear interrupt-flag
	if (!GPIOPinRead(GPIO_PORTP_BASE, STWO)) //checking other pin
	{
		if (gdir == SONE_DIR) //same direction
		{
			if (S2 == 0)	 //gcount is enabled
				gcount += 1; //count up
		}
		else //direction changed
		{
			gdir = SONE_DIR; //set new dir
			if (S2 == 0)
				gcount = 1; //count up
		}
	}
	IntMasterEnable();
}
void rpm_stwo_handler(void)
{
	TOGGLE4;
	IntMasterDisable();
	GPIOIntClear(GPIO_PORTP_BASE, STWO);
	if (!GPIOPinRead(GPIO_PORTP_BASE, SONE)) //checking other pin
	{
		if (gdir == STWO_DIR) //same direction
		{
			if (S2 == 0)	 //gcount is enabled
				gcount += 1; //count up
		}
		else //direction changed
		{
			gdir = STWO_DIR; //set new dir
			if (S2 == 0)
				gcount = 1; //count up
		}
	}
	IntMasterEnable();
}

void rpm_time_handler(void)
{
	TOGGLE4;
	S2 = 1;
	uint64_t lcounter = gcount;
	gcount = 0;
	S2 = 0;
	S = 1;
	if (O == 0)
	{
		calculate(lcounter);
	}
	S = 0; // Mutex for gespeed write
}
void calculate(uint32_t counter)
{
	gspeed = counter * 720; // 720 = 3,6  * 100 * 2
}

void drawIt(uint8_t mode)
{
	if (gspeed > 100000)
		return;
	O = 1;
	while (S != 0)
		;
	uint32_t analog = (uint32_t)gspeed / 100; //to avoid floatingpoint operations gspeed is 100 times higher
	uint32_t lspeed = gspeed;
	uint8_t hunderter, zehner, einer, nulleiner, nullnulleiner = 0; //to seperate numbers
	hunderter = lspeed / 10000;
	lspeed = gspeed % 10000;
	zehner = lspeed / 1000;
	lspeed = gspeed % 1000;
	einer = lspeed / 100;
	lspeed = gspeed % 100;
	nulleiner = lspeed / 10;
	lspeed = gspeed % 10;
	nullnulleiner = lspeed;
	O = 0;

	if (analog >= POINTS_ANALOG_X_MAX) //stop from overwriting Area
		analog = POINTS_ANALOG_X_MAX;
	draw_number(hunderter, MODE_NUM_1);		//draw number
	draw_number(zehner, MODE_NUM_2);		//draw number
	draw_number(einer, MODE_NUM_3);			//draw number
	draw_komma();							//draw komma
	draw_number(nulleiner, MODE_NUM_4);		//draw number
	draw_number(nullnulleiner, MODE_NUM_5); //draw number
	draw_term(mode);						// draw the termmode
	draw_direction(gdir);					// draw the direction
	draw_line(32, 36, analog, 95);			// Box to cover the analog space
}

void wait(int loops)
{
	int tmp;
	for (tmp = 0; tmp < loops; tmp++)
		; // Sekundentakt:= loops = 1600000
}

// Touch

void touch_write(uint8_t value)
{
	uint8_t i = 0x08; // 8 bit command
	uint8_t x, DI;
	GPIO_PORTD_AHB_DATA_R &= 0xFB; // CS=0

	while (i > 0)
	{
		DI = (value >> 7);
		if (DI == 0)
		{
			GPIO_PORTD_AHB_DATA_R &= 0xfe;
		} // out bit = 0
		else
		{
			GPIO_PORTD_AHB_DATA_R |= 0x01;
		}			 // out bit = 1
		value <<= 1; // next value

		GPIO_PORTD_AHB_DATA_R |= 0x08; // Clk = 1
		for (x = 0; x < 10; x++)
			;
		GPIO_PORTD_AHB_DATA_R &= 0xf7; // Clk = 0
		for (x = 0; x < 10; x++)
			;

		i--;
	}
}
unsigned int touch_read(void)
{
	uint8_t i = 12; // 12 Bit ADC
	unsigned int x, value = 0x00;

	while (i > 0)
	{
		value <<= 1;
		GPIO_PORTD_AHB_DATA_R |= 0x08; // Clk = 1
		for (x = 0; x < 10; x++)
			;
		GPIO_PORTD_AHB_DATA_R &= 0xf7; // Clk = 0
		for (x = 0; x < 10; x++)
			;
		value |= ((GPIO_PORTD_AHB_DATA_R >> 1) & 0x01); // read value

		i--;
	}
	GPIO_PORTD_AHB_DATA_R |= 0x04; // CS = 1
	return value;
}

// Display

void clear_display(void)
{
	int x;

	window_set(0, DISP_WIDTH, 0, DISP_HIGHT); // set position
	write_command(0x2C);					  // write pixel command

	for (x = 0; x < DISP_FULL; x++) // set all pixels
	{
		write_data(0x84); // red  	132
		write_data(0x97); // green	151
		write_data(0xb0); // blue		176
	}
}
void startUp_display(void)
{
	int x, y = 0;
	window_set(0, DISP_WIDTH, 0, DISP_HIGHT); // set position
	write_command(0x2C);					  // write pixel command

	for (y = 0; y < DISP_HIGHT; y++)
	{
		for (x = 0; x < DISP_WIDTH + 1; x++)
		{
			uint32_t offset = (x + (y * DISP_WIDTH)) * 4;
			write_data(startup_disp_map[offset + 2]); // red
			write_data(startup_disp_map[offset + 1]); // green
			write_data(startup_disp_map[offset + 0]); // blue
		}
	}
}
void draw_komma(void)
{
	unsigned int start_x = KOMMA_START_X;
	unsigned int start_y = KOMMA_START_Y;
	unsigned int width = KOMMA_WIDTH;
	unsigned int hight = KOMMA_HIGHT;
	int x, y;

	unsigned int end_x = start_x + width - 1;
	unsigned int end_y = start_y + hight - 1;

	window_set(start_x, end_x, start_y, end_y);
	write_command(0x2C);

	for (y = 0; y < hight; y++)
	{
		for (x = 0; x < width; x++)
		{
			uint32_t offset = (x + (y * width)) * 4;
			write_data(komma_map[offset + 2]); // red
			write_data(komma_map[offset + 1]); // green
			write_data(komma_map[offset + 0]); // blue
		}
	}
}
void draw_term(uint8_t mode)
{
	unsigned int start_x = TERM_START_X;
	unsigned int start_y = TERM_START_Y;
	unsigned int width = TERM_WIDTH;
	unsigned int hight = TERM_HIGHT;
	int x, y;

	unsigned int end_x = start_x + width - 1;
	unsigned int end_y = start_y + hight - 1;

	window_set(start_x, end_x, start_y, end_y);
	write_command(0x2C);
	switch (mode)
	{
	case MODE_TERM_KM:
		for (y = 0; y < hight; y++)
		{
			for (x = 0; x < width; x++)
			{
				uint32_t offset = (x + (y * width)) * 4;
				write_data(km_map[offset + 2]); // red
				write_data(km_map[offset + 1]); // green
				write_data(km_map[offset + 0]); // blue
			}
		}
		break;
	case MODE_TERM_KMH:
		for (y = 0; y < hight; y++)
		{
			for (x = 0; x < width; x++)
			{
				uint32_t offset = (x + (y * width)) * 4;
				write_data(km_h_map[offset + 2]); // red
				write_data(km_h_map[offset + 1]); // green
				write_data(km_h_map[offset + 0]); // blue
			}
		}
		break;
	default:
		return;
	}
}
void draw_direction(uint8_t direction)
{
	unsigned int start_x = DIR_START_X;
	unsigned int start_y = DIR_START_Y;
	unsigned int width = DIR_WIDTH;
	unsigned int hight = DIR_HIGTH;
	int x, y;

	unsigned int end_x = start_x + width - 1;
	unsigned int end_y = start_y + hight - 1;

	window_set(start_x, end_x, start_y, end_y);
	write_command(0x2C);

	switch (direction)
	{
	case 'V':
		for (y = 0; y < hight; y++)
		{
			for (x = 0; x < width; x++)
			{
				uint32_t offset = (x + (y * width)) * 4;
				write_data(V_map[offset + 2]); // red
				write_data(V_map[offset + 1]); // green
				write_data(V_map[offset + 0]); // blue
			}
		}
		break;
	case 'R':
		for (y = 0; y < hight; y++)
		{
			for (x = 0; x < width; x++)
			{
				uint32_t offset = (x + (y * width)) * 4;
				write_data(R_map[offset + 2]); // red
				write_data(R_map[offset + 1]); // green
				write_data(R_map[offset + 0]); // blue
			}
		}
		break;
	default:
		return;
	}
}
void draw_number(uint8_t number, uint8_t mode)
{
	if (number > 9)
		return;
	unsigned int start_x;
	switch (mode)
	{
	case MODE_NUM_1:
		start_x = NUM_START_X_1;
		break;
	case MODE_NUM_2:
		start_x = NUM_START_X_2;
		break;
	case MODE_NUM_3:
		start_x = NUM_START_X_3;
		break;
	case MODE_NUM_4:
		start_x = NUM_START_X_4;
		break;
	case MODE_NUM_5:
		start_x = NUM_START_X_5;
		break;
	default:
		return;
	}
	unsigned int start_y = NUM_START_Y;
	unsigned int width = NUM_WIDTH;
	unsigned int hight = NUM_HIGTH;
	int x, y;

	unsigned int end_x = start_x + width - 1;
	unsigned int end_y = start_y + hight - 1;

	window_set(start_x, end_x, start_y, end_y);
	write_command(0x2C);

	for (y = 0; y < hight; y++)
	{
		for (x = 0; x < width; x++)
		{
			uint32_t offset = (x + (y * width)) * 4;
			write_data(number_map[number][offset + 2]); // red
			write_data(number_map[number][offset + 1]); // green
			write_data(number_map[number][offset + 0]); // blue
		}
	}
}
void clear_pixel(void)
{
	write_data(0x84); // red
	write_data(0x97); // green
	write_data(0xb0); // blue
}

void initialise_ssd1963(void)
{

	GPIO_PORTL_DATA_R &= ~0x10; // Hardware reset
	wait(1600);					// Wait more than 100us -> 1ms

	write_command(0x01); // Software reset
	wait(16000);		 // Wait more than 5ms -> 10ms

	write_command(0xE2); // Set PLL Freq=100MHz
	write_data(0x1D);
	write_data(0x02);
	write_data(0x04);

	write_command(0xE0); // Start PLL
	write_data(0x01);
	wait(1600); // Wait more than 100us -> 1ms

	write_command(0xE0); //Lock PLL
	write_data(0x03);
	wait(1600); // Wait more than 100us -> 1ms

	write_command(0x01); //Software reset
	wait(16000);		 // Wait more than 5ms -> 10ms

	write_command(0xE6); // Set LCD Pixel Clock 9MHz;
	write_data(0x01);
	write_data(0x70);
	write_data(0xA3);

	write_command(0xB0); // Set LCD Panel mode
	write_data(0x20);	// TFT panel 24bit...
	write_data(0x00);	// TFT mode
	write_data(0x01);	// Horizontal size 480-1
	write_data(0xDF);	// Horizontal size 480-1
	write_data(0x01);	// Vertical size 272-1
	write_data(0x0F);	// Vertical size 272-1
	write_data(0x00);	// even/odd line RGB

	write_command(0xB4); // set Horizontal period
	write_data(0x02);	// set HT total pixel=531
	write_data(0x13);
	write_data(0x00); // Set Horizontal sync pule start pos=43
	write_data(0x2B);
	write_data(0x0A); // set horiz.sync pulse with =10
	write_data(0x00); // set horiz.sync pulse start pos=8
	write_data(0x08);
	write_data(0x00);

	write_command(0xB6); // set Vertical period
	write_data(0x01);	// set VT lines = 288
	write_data(0x20);
	write_data(0x00); // Set VPS = 12
	write_data(0x0C);
	write_data(0x0A); // set vert.sync pulse with = 10
	write_data(0x00); // set vert.sync pulse start = 4
	write_data(0x04);

	write_command(0x36); // Flip Dysplay - necessary to match with Touch Display
	write_data(0x03);

	write_command(0xF0); // Set pixel data format = 8bit
	write_data(0x00);

	write_command(0x29); // Set display on
}

void draw_line(unsigned int start_x, unsigned int start_y, unsigned int length, unsigned int width)
{
	uint8_t red = 230;
	uint8_t green = 2;
	uint8_t blue = 0;
	int x, y;

	unsigned int end_x = start_x + POINTS_ANALOG_X_MAX;

	window_set(start_x, end_x, start_y, end_y);
	write_command(0x2C);

	for (y = 0; y < width; y++)
	{
		for (x = 0; x < POINTS_ANALOG_X_MAX; x++)
		{
			if (x >= length)
			{
				clear_pixel();
			}
			else
			{
				write_data(red);   // red
				write_data(green); // green
				write_data(blue);  // blue
			}
		}
	}
}

void write_command(uint8_t command)
{
	GPIO_PORTL_DATA_R = 0x1F;	// Initial state
	GPIO_PORTL_DATA_R &= ~0x08;  // Chip select
	GPIO_PORTL_DATA_R &= ~0x04;  // Command mode select
	GPIO_PORTL_DATA_R &= ~0x02;  // Write state
	GPIO_PORTM_DATA_R = command; // Write command byte
	GPIO_PORTL_DATA_R |= 0x02;   // No write state
	GPIO_PORTL_DATA_R |= 0x08;   // No chip select
}
void write_data(uint8_t data)
{
	GPIO_PORTL_DATA_R = 0x1F;   // Initial state
	GPIO_PORTL_DATA_R &= ~0x08; // Chip select
	GPIO_PORTL_DATA_R |= 0x04;  // Data mode select
	GPIO_PORTL_DATA_R &= ~0x02; // Write state
	GPIO_PORTM_DATA_R = data;   // Write data byte
	GPIO_PORTL_DATA_R |= 0x02;  // No write state
	GPIO_PORTL_DATA_R |= 0x08;  // No chip select
}
void window_set(unsigned int start_x, unsigned int end_x, unsigned int start_y, unsigned int end_y)
{
	write_command(0x2A);		// SET page address
	write_data((start_x) >> 8); // SET start page adress (HB)
	write_data(start_x);		// SET start page adress (LB)
	write_data((end_x) >> 8);   // SET end page adress
	write_data(end_x);			// SET end page adress

	write_command(0x2B);		// SET page address
	write_data((start_y) >> 8); // SET start column adress (HB)
	write_data(start_y);		// SET start column adress (LB)
	write_data((end_y) >> 8);   // SET end column adress
	write_data(end_y);			// SET end column adress
}
