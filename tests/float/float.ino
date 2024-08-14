#include <Arduboy2.h>

#include <stdint.h>

volatile float x;
volatile float y;

Arduboy2 a;

void setup() {
  a.boot();

  x = 1.f;

  y = x * 40.f;

  if(y == 40.f)
    UEDATX = 'P';
  else
    UEDATX = 'F';

  a.print(x);
  a.print('\n');
  a.print(y);
  a.display(CLEAR_BUFFER);
}

void loop() {
  a.idle();
}
