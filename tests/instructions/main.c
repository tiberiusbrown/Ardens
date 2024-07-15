#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

static PROGMEM const unsigned crcs [] = {
	#include "correct.h"
};

extern void instrs( void );
extern void instrs_end( void );
extern unsigned test_instr( unsigned );

#define TESTDDR DDRB
#define TESTPORT PORTB

#include <stdio.h>

int my_putc(char c, FILE* f) 
{
    UEDATX = c;
    return c;
}

int main( void )
{
    //fdevopen(&my_putc, 0);
    
	cli();
	
	TESTDDR = 0;
	
	// Be sure that port B acts like an 8-bit RAM location (used in test)
	{
		uint8_t b = 0;
		do
		{
			TESTPORT = b;
			if ( TESTPORT != b )
			{
				UEDATX = 'F';
				return 0;
			}
		}
		while ( --b );
	}
	
	// without this linker strips table from asm code
	static volatile char c = 123;
	c = 0;
	
	char failed = 0;
	unsigned addr = (unsigned) &instrs;
	const unsigned* p = crcs;
	while ( addr < (unsigned) &instrs_end )
	{
		unsigned crc = test_instr( addr );
		unsigned correct = pgm_read_word( p++ );
		//printf( "0x%04X,", crc );
		if ( crc != correct )
		{
            UEDATX = 'F';
			//printf( " // +0x%03x mismatch; table shows 0x%04X", addr*2, correct );
			failed = 1;
		}
		//printf( "\n" );
		
		addr += 2;
		if ( pgm_read_word( addr*2+2 ) == 0 )
			// prev instr was a skip which uses four slots, so skip the last two
			addr += 2;
	}
	
	if ( !failed )
		UEDATX = 'P'; //printf( "// Passed\n" );
	else
		UEDATX = 'F'; //printf( "// Failed\n" );
	
	return 0;
}
