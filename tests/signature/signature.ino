#include <Arduboy2.h>

#include <avr/boot.h>

Arduboy2 a;

void setup() {
  a.boot();
  uint8_t sreg = SREG;
  uint16_t t0, t1, t2, t3;

  cli();
  t0 = boot_signature_byte_get(0);
  t1 = boot_signature_byte_get(2);
  t2 = boot_signature_byte_get(4);
  t3 = boot_signature_byte_get(1);
  sei();

  if(
    t0 == 0x1e &&
    t1 == 0x95 &&
    t2 == 0x87)
    UEDATX = 'P';
  else
    UEDATX = 'F';

  a.print(t0, HEX);
  a.print(' ');
  a.print(t1, HEX);
  a.print(' ');
  a.print(t2, HEX);
  a.print('\n');
  a.print(t3, HEX);
  a.display(CLEAR_BUFFER);
}

void loop() {
  a.idle();
}
