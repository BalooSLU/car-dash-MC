// Wartefunktion
// Sekundentakt mit loops = 1600000

#include "tm4c1294ncpdt.h"
#include "stdio.h"

void wait(int loops){
	int tmp;
	for(tmp=0;tmp<loops;tmp++);
}

void main (void)
{
	// Port  Clock Gating Control
			SYSCTL_RCGCGPIO_R |= 0x800;	    				// Port M clock ini
			while ((SYSCTL_PRGPIO_R & 0x00000800) ==0);		// Port M ready ?

		 // Set direction
			GPIO_PORTM_DIR_R = 0xFF;

		 // Digital enable
			GPIO_PORTM_DEN_R = 0xFF;

		 // Ports on/off
			while(1){
			GPIO_PORTM_DATA_R=0xFF;
			wait(1600000);
			GPIO_PORTM_DATA_R =0x00;
			wait(1600000);
			printf("MP-Labor\n");
			}
}
