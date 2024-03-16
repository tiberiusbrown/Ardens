#include <Arduboy2.h>

Arduboy2 a;

void setup() {
  a.boot();
  uint8_t sreg = SREG;
  uint16_t t0, t1, t2, t3, t4, t5, t6, t7;
  cli();
  GTCCR = 0x01;
  TCCR3A = 0x00;
  TCCR3B = 0x01;
  TCCR3C = 0x00;
  TCNT3 = 0;
  for(int i = 0; i < 48; ++i)
    asm volatile("nop\n");
  TCCR1A = 0x00;
  TCCR1B = 0x02;
  TCCR1C = 0x00;
  TCNT1 = 0;
  for(int i = 0; i < 48; ++i)
    asm volatile("nop\n");
  t0 = TCNT3;
  t1 = TCNT1;
  t2 = TCNT3;
  t3 = TCNT1;
  TCNT3 = 500;
  TCNT1 = 500;
  t4 = TCNT3;
  t5 = TCNT1;
  t6 = TCNT3;
  asm volatile("nop\n");
  asm volatile("nop\n");
  t7 = TCNT1;
  SREG = sreg;

  if(
    t0 == 494 &&
    t1 == 31 &&
    t2 == 502 &&
    t3 == 32 &&
    t4 == 505 &&
    t5 == 500 &&
    t6 == 513 &&
    t7 == 502)
    UEDATX = 'P';
  else
    UEDATX = 'F';

  a.print(t0);
  a.print('\n');
  a.print(t1);
  a.print('\n');
  a.print(t2);
  a.print('\n');
  a.print(t3);
  a.print('\n');
  a.print(t4);
  a.print('\n');
  a.print(t5);
  a.print('\n');
  a.print(t6);
  a.print('\n');
  a.print(t7);
  a.display(CLEAR_BUFFER);
}

void loop() {
  a.idle();
}
