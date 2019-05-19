#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

int counter;

int main(void) {
	/* start GDBstub :) */
	extern void stub_test(void); stub_test();

	/* enable interrupts (on the CPU) */
	init_interrupts();

	/* Initialize peripherals */
	display_init( RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE );
	console_init();
	controller_init();

	console_set_render_mode(RENDER_MANUAL);

	while(1)
	{
		console_clear();

		printf( "Are you ready to debug?\n" );
		printf( "Counting: %d\n", counter++ );

		console_render();
	}
}
