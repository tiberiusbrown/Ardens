#include <Arduboy2.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#ifndef PRTIM4
#define PRTIM4 4
#endif

#ifndef TLOCK4
#define TLOCK4 7
#endif

Arduboy2 arduboy;

static constexpr uint8_t RESULT_VALUE_CAPACITY = 18;

struct result_t
{
  const char* label;
  uint8_t count;
  uint16_t value[RESULT_VALUE_CAPACITY];
  uint16_t flags;
};

static result_t current_result;
static uint8_t pllcsr_after_wait;
static uint8_t pllfrq_after_wait;

static result_t& next_result(const char* label, uint8_t count)
{
  result_t& r = current_result;
  r.label = label;
  r.count = count;
  r.flags = 0;
  for(uint8_t i = 0; i < RESULT_VALUE_CAPACITY; ++i)
    r.value[i] = 0;
  return r;
}

static void wait_for_serial()
{
  Serial.begin(9600);
  uint32_t const start = millis();
  while(!Serial && millis() - start < 5000)
  {
  }
  delay(50);
}

static void emit_result(result_t const& r)
{
  uint8_t const count =
    r.count <= RESULT_VALUE_CAPACITY ? r.count : RESULT_VALUE_CAPACITY;
  Serial.print(r.label);
  Serial.print(',');
  Serial.print(unsigned(count));
  for(uint8_t i = 0; i < count; ++i)
  {
    Serial.print(',');
    Serial.print(unsigned(r.value[i]));
  }
  Serial.print(',');
  Serial.println(unsigned(r.flags));
}

static void emit_header()
{
  Serial.println(F("ARDENS_TIMER_EDGE_ACCURACY,1"));
  Serial.print(F("meta,f_cpu,"));
  Serial.println(F_CPU);
  Serial.print(F("meta,fixture_version,"));
  Serial.println(1);
  Serial.print(F("meta,pllcsr,"));
  Serial.println(pllcsr_after_wait);
}

static void emit_footer()
{
  Serial.println(F("END"));
}

static void wait_for_pll_lock()
{
  PLLCSR |= _BV(PLLE);
  uint32_t const start = millis();
  while(!(PLLCSR & _BV(PLOCK)) && millis() - start < 10)
  {
  }
  pllcsr_after_wait = PLLCSR;
  pllfrq_after_wait = PLLFRQ;
}

static void stop_timer0()
{
  TCCR0A = 0;
  TCCR0B = 0;
  TIMSK0 = 0;
  TIFR0 = 0xff;
}

static void stop_timer1()
{
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1C = 0;
  TIMSK1 = 0;
  TIFR1 = 0xff;
}

static void stop_timer3()
{
  TCCR3A = 0;
  TCCR3B = 0;
  TCCR3C = 0;
  TIMSK3 = 0;
  TIFR3 = 0xff;
}

static void stop_timer4()
{
  TCCR4A = 0;
  TCCR4B = 0;
  TCCR4C = 0;
  TCCR4D = 0;
  TCCR4E = 0;
  TIMSK4 = 0;
  TIFR4 = 0xff;
}

static void stop_all_timers()
{
  stop_timer0();
  stop_timer1();
  stop_timer3();
  stop_timer4();
  GTCCR = 0;
}

static void measure_t1_read_latch()
{
  result_t& r = next_result("t1_read_latch", 8);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk1], r18\n\t"
    "sts %[tccr1a], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr1c], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt1h], r18\n\t"
    "ldi r18, 250\n\t"
    "sts %[tcnt1l], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 4\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tcnt1l]\n\t"
    "lds r23, %[tcnt1h]\n\t"
    "lds r24, %[tcnt1l]\n\t"
    "lds r25, %[tcnt1h]\n\t"
    "lds r18, %[tifr1]\n\t"
    "ldi r19, 0\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r23\n\t"
    "st X+, r24\n\t"
    "st X+, r25\n\t"
    "st X+, r18\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H))
    : "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "memory");
}

static void measure_t1_write_latch()
{
  result_t& r = next_result("t1_write_latch", 8);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk1], r18\n\t"
    "sts %[tccr1a], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr1c], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt1h], r18\n\t"
    "ldi r18, 48\n\t"
    "sts %[tcnt1l], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "ldi r18, 2\n\t"
    "sts %[tcnt1h], r18\n\t"
    ".rept 4\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1h]\n\t"
    "ldi r18, 16\n\t"
    "sts %[tcnt1l], r18\n\t"
    "lds r21, %[tcnt1l]\n\t"
    "lds r22, %[tcnt1h]\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r23, %[tcnt1l]\n\t"
    "lds r24, %[tcnt1h]\n\t"
    "lds r25, %[tcnt1h]\n\t"
    "lds r16, %[tcnt1l]\n\t"
    "lds r17, %[tifr1]\n\t"
    "lds r18, %[tccr1b]\n\t"
    "ldi r19, 0\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r23\n\t"
    "st X+, r24\n\t"
    "st X+, r25\n\t"
    "st X+, r19\n\t"
    "st X+, r16\n\t"
    "st X+, r19\n\t"
    "st X+, r17\n\t"
    "st X+, r19\n\t"
    "st X+, r18\n\t"
    "st X+, r19\n\t"
    "st X+, r19\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H))
    : "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25",
      "memory");
}

static void measure_compare_block_t0_t1()
{
  result_t& r = next_result("compare_block_t0_t1", 12);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk0], r19\n\t"
    "sts %[timsk1], r19\n\t"
    "sts %[tccr0a], r19\n\t"
    "sts %[tccr0b], r19\n\t"
    "sts %[tccr1a], r19\n\t"
    "sts %[tccr1b], r19\n\t"
    "sts %[tccr1c], r19\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "ldi r18, 22\n\t"
    "sts %[ocr0a], r18\n\t"
    "ldi r18, 21\n\t"
    "sts %[tcnt0], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr0b], r18\n\t"
    ".rept 4\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt0]\n\t"
    "lds r21, %[tifr0]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "sts %[tccr0b], r19\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "ldi r18, 22\n\t"
    "sts %[ocr0a], r18\n\t"
    "ldi r18, 20\n\t"
    "sts %[tcnt0], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr0b], r18\n\t"
    ".rept 6\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt0]\n\t"
    "lds r21, %[tifr0]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "sts %[tccr1b], r19\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocr1ah], r18\n\t"
    "ldi r18, 22\n\t"
    "sts %[ocr1al], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt1h], r18\n\t"
    "ldi r18, 21\n\t"
    "sts %[tcnt1l], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 4\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "sts %[tccr1b], r19\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocr1ah], r18\n\t"
    "ldi r18, 22\n\t"
    "sts %[ocr1al], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt1h], r18\n\t"
    "ldi r18, 20\n\t"
    "sts %[tcnt1l], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 6\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "lds r20, %[ocr0a]\n\t"
    "lds r21, %[ocr1al]\n\t"
    "lds r22, %[tccr0b]\n\t"
    "lds r23, %[tccr1b]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tcnt0] "n" (_SFR_MEM_ADDR(TCNT0)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [ocr0a] "n" (_SFR_MEM_ADDR(OCR0A)),
      [ocr1al] "n" (_SFR_MEM_ADDR(OCR1AL)),
      [ocr1ah] "n" (_SFR_MEM_ADDR(OCR1AH))
    : "r18", "r19", "r20", "r21", "r22", "r23", "memory");
}

static void measure_top_bottom_flags_t1()
{
  result_t& r = next_result("top_bottom_flags_t1", 14);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk1], r19\n\t"
    "sts %[tccr1a], r19\n\t"
    "sts %[tccr1b], r19\n\t"
    "sts %[tccr1c], r19\n\t"
    "ldi r18, 0\n\t"
    "sts %[icr1h], r18\n\t"
    "ldi r18, 5\n\t"
    "sts %[icr1l], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocr1ah], r18\n\t"
    "ldi r18, 3\n\t"
    "sts %[ocr1al], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt1h], r18\n\t"
    "ldi r18, 4\n\t"
    "sts %[tcnt1l], r18\n\t"
    "ldi r18, 17\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 1\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 4\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "lds r20, %[icr1l]\n\t"
    "lds r21, %[ocr1al]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [ocr1al] "n" (_SFR_MEM_ADDR(OCR1AL)),
      [ocr1ah] "n" (_SFR_MEM_ADDR(OCR1AH)),
      [icr1l] "n" (_SFR_MEM_ADDR(ICR1L)),
      [icr1h] "n" (_SFR_MEM_ADDR(ICR1H))
    : "r18", "r19", "r20", "r21", "r22", "memory");
}

static void measure_double_buffer_t1()
{
  result_t& r = next_result("double_buffer_t1", 14);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk1], r19\n\t"
    "sts %[tccr1a], r19\n\t"
    "sts %[tccr1b], r19\n\t"
    "sts %[tccr1c], r19\n\t"
    "ldi r18, 0\n\t"
    "sts %[icr1h], r18\n\t"
    "ldi r18, 9\n\t"
    "sts %[icr1l], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocr1bh], r18\n\t"
    "ldi r18, 7\n\t"
    "sts %[ocr1bl], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt1h], r18\n\t"
    "ldi r18, 6\n\t"
    "sts %[tcnt1l], r18\n\t"
    "ldi r18, 2\n\t"
    "sts %[tccr1a], r18\n\t"
    "ldi r18, 25\n\t"
    "sts %[tccr1b], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocr1bh], r18\n\t"
    "ldi r18, 2\n\t"
    "sts %[ocr1bl], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 6\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 10\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    ".rept 4\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 12\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "lds r20, %[ocr1bl]\n\t"
    "lds r21, %[icr1l]\n\t"
    "lds r22, %[tccr1a]\n\t"
    "lds r23, %[tccr1b]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [ocr1bl] "n" (_SFR_MEM_ADDR(OCR1BL)),
      [ocr1bh] "n" (_SFR_MEM_ADDR(OCR1BH)),
      [icr1l] "n" (_SFR_MEM_ADDR(ICR1L)),
      [icr1h] "n" (_SFR_MEM_ADDR(ICR1H))
    : "r18", "r19", "r20", "r21", "r22", "r23", "memory");
}

static void measure_foc_t0_t3()
{
  result_t& r = next_result("foc_t0_t3", 14);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk0], r19\n\t"
    "sts %[timsk3], r19\n\t"
    "sts %[tccr0a], r19\n\t"
    "sts %[tccr0b], r19\n\t"
    "sts %[tccr3a], r19\n\t"
    "sts %[tccr3b], r19\n\t"
    "sts %[tccr3c], r19\n\t"
    "lds r18, %[ddrb]\n\t"
    "ori r18, 128\n\t"
    "sts %[ddrb], r18\n\t"
    "lds r18, %[portb]\n\t"
    "andi r18, 127\n\t"
    "sts %[portb], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "ldi r18, 64\n\t"
    "sts %[tccr0a], r18\n\t"
    "ldi r18, 128\n\t"
    "sts %[tccr0b], r18\n\t"
    "lds r20, %[pinb]\n\t"
    "andi r20, 128\n\t"
    "lds r21, %[portb]\n\t"
    "andi r21, 128\n\t"
    "lds r22, %[tifr0]\n\t"
    "lds r23, %[tccr0b]\n\t"
    "ldi r18, 128\n\t"
    "sts %[tccr0b], r18\n\t"
    "lds r24, %[pinb]\n\t"
    "andi r24, 128\n\t"
    "lds r25, %[portb]\n\t"
    "andi r25, 128\n\t"
    "lds r16, %[tifr0]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r24\n\t"
    "st X+, r19\n\t"
    "st X+, r25\n\t"
    "st X+, r19\n\t"
    "st X+, r16\n\t"
    "st X+, r19\n\t"
    "lds r18, %[ddrc]\n\t"
    "ori r18, 64\n\t"
    "sts %[ddrc], r18\n\t"
    "lds r18, %[portc]\n\t"
    "andi r18, 191\n\t"
    "sts %[portc], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr3], r18\n\t"
    "ldi r18, 64\n\t"
    "sts %[tccr3a], r18\n\t"
    "ldi r18, 128\n\t"
    "sts %[tccr3c], r18\n\t"
    "lds r20, %[pinc]\n\t"
    "andi r20, 64\n\t"
    "lds r21, %[portc]\n\t"
    "andi r21, 64\n\t"
    "lds r22, %[tifr3]\n\t"
    "lds r23, %[tccr3c]\n\t"
    "ldi r18, 128\n\t"
    "sts %[tccr3c], r18\n\t"
    "lds r24, %[pinc]\n\t"
    "andi r24, 64\n\t"
    "lds r25, %[portc]\n\t"
    "andi r25, 64\n\t"
    "lds r16, %[tifr3]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r24\n\t"
    "st X+, r19\n\t"
    "st X+, r25\n\t"
    "st X+, r19\n\t"
    "st X+, r16\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [timsk3] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tifr3] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tccr3a] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccr3b] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccr3c] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [ddrb] "n" (_SFR_MEM_ADDR(DDRB)),
      [portb] "n" (_SFR_MEM_ADDR(PORTB)),
      [pinb] "n" (_SFR_MEM_ADDR(PINB)),
      [ddrc] "n" (_SFR_MEM_ADDR(DDRC)),
      [portc] "n" (_SFR_MEM_ADDR(PORTC)),
      [pinc] "n" (_SFR_MEM_ADDR(PINC))
    : "r16", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25",
      "memory");
}

static void measure_oc_pin_t0_t3_t4()
{
  result_t& r = next_result("oc_pin_t0_t3_t4", 18);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk0], r19\n\t"
    "sts %[tccr0a], r19\n\t"
    "sts %[tccr0b], r19\n\t"
    "lds r18, %[ddrb]\n\t"
    "ori r18, 128\n\t"
    "sts %[ddrb], r18\n\t"
    "lds r18, %[portb]\n\t"
    "andi r18, 127\n\t"
    "sts %[portb], r18\n\t"
    "ldi r18, 4\n\t"
    "sts %[ocr0a], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tcnt0], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "ldi r18, 64\n\t"
    "sts %[tccr0a], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr0b], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt0]\n\t"
    "lds r21, %[pinb]\n\t"
    "andi r21, 128\n\t"
    "lds r22, %[tifr0]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 6\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt0]\n\t"
    "lds r21, %[pinb]\n\t"
    "andi r21, 128\n\t"
    "lds r22, %[tifr0]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tcnt0] "n" (_SFR_MEM_ADDR(TCNT0)),
      [ocr0a] "n" (_SFR_MEM_ADDR(OCR0A)),
      [ddrb] "n" (_SFR_MEM_ADDR(DDRB)),
      [portb] "n" (_SFR_MEM_ADDR(PORTB)),
      [pinb] "n" (_SFR_MEM_ADDR(PINB))
    : "r18", "r19", "r20", "r21", "r22", "memory");

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk3], r19\n\t"
    "sts %[tccr3a], r19\n\t"
    "sts %[tccr3b], r19\n\t"
    "sts %[tccr3c], r19\n\t"
    "lds r18, %[ddrc]\n\t"
    "ori r18, 64\n\t"
    "sts %[ddrc], r18\n\t"
    "lds r18, %[portc]\n\t"
    "andi r18, 191\n\t"
    "sts %[portc], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocr3ah], r18\n\t"
    "ldi r18, 4\n\t"
    "sts %[ocr3al], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt3h], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tcnt3l], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr3], r18\n\t"
    "ldi r18, 64\n\t"
    "sts %[tccr3a], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr3b], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt3l]\n\t"
    "lds r21, %[tcnt3h]\n\t"
    "lds r22, %[pinc]\n\t"
    "andi r22, 64\n\t"
    "lds r23, %[tifr3]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt3l]\n\t"
    "lds r21, %[tcnt3h]\n\t"
    "lds r22, %[pinc]\n\t"
    "andi r22, 64\n\t"
    "lds r23, %[tifr3]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk3] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr3] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccr3a] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccr3b] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccr3c] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcnt3l] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnt3h] "n" (_SFR_MEM_ADDR(TCNT3H)),
      [ocr3al] "n" (_SFR_MEM_ADDR(OCR3AL)),
      [ocr3ah] "n" (_SFR_MEM_ADDR(OCR3AH)),
      [ddrc] "n" (_SFR_MEM_ADDR(DDRC)),
      [portc] "n" (_SFR_MEM_ADDR(PORTC)),
      [pinc] "n" (_SFR_MEM_ADDR(PINC))
    : "r18", "r19", "r20", "r21", "r22", "r23", "memory");

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk4], r19\n\t"
    "sts %[tccr4a], r19\n\t"
    "sts %[tccr4b], r19\n\t"
    "sts %[tccr4c], r19\n\t"
    "sts %[tccr4d], r19\n\t"
    "sts %[tccr4e], r19\n\t"
    "lds r18, %[ddrc]\n\t"
    "ori r18, 128\n\t"
    "sts %[ddrc], r18\n\t"
    "lds r18, %[portc]\n\t"
    "andi r18, 127\n\t"
    "sts %[portc], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 31\n\t"
    "sts %[ocr4c], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 4\n\t"
    "sts %[ocr4a], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tcnt4l], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr4], r18\n\t"
    "ldi r18, 64\n\t"
    "sts %[tccr4a], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr4b], r18\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[pinc]\n\t"
    "andi r22, 128\n\t"
    "lds r23, %[tifr4]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    ".rept 12\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[pinc]\n\t"
    "andi r22, 128\n\t"
    "lds r23, %[tifr4]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk4] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr4] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccr4a] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccr4b] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccr4c] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccr4d] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccr4e] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt4l] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocr4a] "n" (_SFR_MEM_ADDR(OCR4A)),
      [ocr4c] "n" (_SFR_MEM_ADDR(OCR4C)),
      [ddrc] "n" (_SFR_MEM_ADDR(DDRC)),
      [portc] "n" (_SFR_MEM_ADDR(PORTC)),
      [pinc] "n" (_SFR_MEM_ADDR(PINC))
    : "r18", "r19", "r20", "r21", "r22", "r23", "memory");
}

static void measure_prr_freeze_t1_t4()
{
  result_t& r = next_result("prr_freeze_t1_t4", 12);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "lds r18, %[prr0]\n\t"
    "andi r18, %[clear_prtim1]\n\t"
    "sts %[prr0], r18\n\t"
    "lds r18, %[prr1]\n\t"
    "andi r18, %[clear_prtim4]\n\t"
    "sts %[prr1], r18\n\t"
    "sts %[timsk1], r19\n\t"
    "sts %[tccr1a], r19\n\t"
    "sts %[tccr1b], r19\n\t"
    "sts %[tccr1c], r19\n\t"
    "sts %[tcnt1h], r19\n\t"
    "sts %[tcnt1l], r19\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr1], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r18, %[prr0]\n\t"
    "ori r18, %[prtim1]\n\t"
    "sts %[prr0], r18\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    ".rept 48\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "lds r18, %[prr0]\n\t"
    "andi r18, %[clear_prtim1]\n\t"
    "sts %[prr0], r18\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt1l]\n\t"
    "lds r21, %[tcnt1h]\n\t"
    "lds r22, %[tifr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "sts %[timsk4], r19\n\t"
    "sts %[tccr4a], r19\n\t"
    "sts %[tccr4b], r19\n\t"
    "sts %[tccr4c], r19\n\t"
    "sts %[tccr4d], r19\n\t"
    "sts %[tccr4e], r19\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[ocr4c], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "sts %[tcnt4l], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr4], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr4b], r18\n\t"
    ".rept 24\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r18, %[prr1]\n\t"
    "ori r18, %[prtim4]\n\t"
    "sts %[prr1], r18\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    ".rept 64\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[tifr4]\n\t"
    "lds r18, %[prr1]\n\t"
    "andi r18, %[clear_prtim4]\n\t"
    "sts %[prr1], r18\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 24\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[tifr4]\n\t"
    "lds r23, %[prr0]\n\t"
    "lds r24, %[prr1]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r24\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [prr0] "n" (_SFR_MEM_ADDR(PRR0)),
      [prr1] "n" (_SFR_MEM_ADDR(PRR1)),
      [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [timsk4] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tifr4] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tccr4a] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccr4b] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccr4c] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccr4d] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccr4e] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [tcnt4l] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocr4c] "n" (_SFR_MEM_ADDR(OCR4C)),
      [prtim1] "M" (_BV(PRTIM1)),
      [prtim4] "M" (_BV(PRTIM4)),
      [clear_prtim1] "M" (0xff ^ _BV(PRTIM1)),
      [clear_prtim4] "M" (0xff ^ _BV(PRTIM4))
    : "r18", "r19", "r20", "r21", "r22", "r23", "r24", "memory");
}

static void measure_t4_tcnt_write_sync()
{
  result_t& r = next_result("t4_tcnt_write_sync", 12);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk4], r19\n\t"
    "sts %[tccr4a], r19\n\t"
    "sts %[tccr4b], r19\n\t"
    "sts %[tccr4c], r19\n\t"
    "sts %[tccr4d], r19\n\t"
    "sts %[tccr4e], r19\n\t"
    "sts %[tc4h], r19\n\t"
    "ldi r18, 255\n\t"
    "sts %[ocr4c], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "sts %[tcnt4l], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr4], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr4b], r18\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "ldi r18, 1\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 32\n\t"
    "sts %[tcnt4l], r18\n\t"
    "lds r22, %[tcnt4l]\n\t"
    "lds r23, %[tc4h]\n\t"
    "andi r23, 7\n\t"
    "lds r24, %[tcnt4l]\n\t"
    "lds r25, %[tc4h]\n\t"
    "andi r25, 7\n\t"
    ".rept 12\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r16, %[tcnt4l]\n\t"
    "lds r17, %[tc4h]\n\t"
    "andi r17, 7\n\t"
    "lds r18, %[tifr4]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r23\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r24\n\t"
    "st X+, r25\n\t"
    "st X+, r24\n\t"
    "st X+, r19\n\t"
    "st X+, r25\n\t"
    "st X+, r19\n\t"
    "st X+, r16\n\t"
    "st X+, r17\n\t"
    "st X+, r18\n\t"
    "st X+, r19\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[tccr4b]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk4] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr4] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccr4a] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccr4b] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccr4c] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccr4d] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccr4e] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt4l] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocr4c] "n" (_SFR_MEM_ADDR(OCR4C))
    : "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25",
      "memory");
}

static void measure_t4_tc4h_latch()
{
  result_t& r = next_result("t4_tc4h_latch", 10);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk4], r19\n\t"
    "sts %[tccr4a], r19\n\t"
    "sts %[tccr4b], r19\n\t"
    "sts %[tccr4c], r19\n\t"
    "sts %[tccr4d], r19\n\t"
    "sts %[tccr4e], r19\n\t"
    "ldi r18, 1\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[ocr4c], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 254\n\t"
    "sts %[tcnt4l], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr4], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr4b], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[tcnt4l]\n\t"
    "lds r23, %[tc4h]\n\t"
    "andi r23, 7\n\t"
    "lds r24, %[tcnt4l]\n\t"
    "lds r25, %[tc4h]\n\t"
    "andi r25, 7\n\t"
    "lds r16, %[tifr4]\n\t"
    "lds r17, %[ocr4c]\n\t"
    "lds r18, %[tccr4b]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r23\n\t"
    "st X+, r24\n\t"
    "st X+, r25\n\t"
    "st X+, r16\n\t"
    "st X+, r19\n\t"
    "st X+, r17\n\t"
    "st X+, r19\n\t"
    "st X+, r18\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk4] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr4] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccr4a] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccr4b] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccr4c] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccr4d] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccr4e] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt4l] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocr4c] "n" (_SFR_MEM_ADDR(OCR4C))
    : "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25",
      "memory");
}

static void measure_t4_psr_tlock_buffer()
{
  result_t& r = next_result("t4_psr_tlock_buffer", 16);
  uint16_t* out = r.value;

  asm volatile(
    "ldi r19, 0\n\t"
    "sts %[timsk4], r19\n\t"
    "sts %[tccr4a], r19\n\t"
    "sts %[tccr4b], r19\n\t"
    "sts %[tccr4c], r19\n\t"
    "sts %[tccr4d], r19\n\t"
    "sts %[tccr4e], r19\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 15\n\t"
    "sts %[ocr4c], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 4\n\t"
    "sts %[ocr4a], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 10\n\t"
    "sts %[tcnt4l], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr4], r18\n\t"
    "ldi r18, 130\n\t"
    "sts %[tccr4a], r18\n\t"
    "ldi r18, 128\n\t"
    "sts %[tccr4e], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccr4b], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 10\n\t"
    "sts %[ocr4a], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 19\n\t"
    "sts %[ocr4c], r18\n\t"
    ".rept 10\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[tifr4]\n\t"
    "lds r23, %[ocr4a]\n\t"
    "lds r24, %[ocr4c]\n\t"
    "ldi r18, 0\n\t"
    "sts %[tccr4e], r18\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r24\n\t"
    "st X+, r19\n\t"
    ".rept 20\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[tifr4]\n\t"
    "ldi r18, %[psr4_cs1]\n\t"
    "sts %[tccr4b], r18\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    ".rept 12\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds r20, %[tcnt4l]\n\t"
    "lds r21, %[tc4h]\n\t"
    "andi r21, 7\n\t"
    "lds r22, %[tifr4]\n\t"
    "lds r23, %[tccr4b]\n\t"
    "lds r24, %[tccr4e]\n\t"
    "lds r25, %[tccr4a]\n\t"
    "st X+, r20\n\t"
    "st X+, r21\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r24\n\t"
    "st X+, r19\n\t"
    "st X+, r25\n\t"
    "st X+, r19\n\t"
    "lds r20, %[ocr4a]\n\t"
    "lds r21, %[ocr4c]\n\t"
    "lds r22, %[tc4h]\n\t"
    "andi r22, 7\n\t"
    "lds r23, %[tcnt4l]\n\t"
    "lds r24, %[pllfrq]\n\t"
    "st X+, r20\n\t"
    "st X+, r19\n\t"
    "st X+, r21\n\t"
    "st X+, r19\n\t"
    "st X+, r22\n\t"
    "st X+, r19\n\t"
    "st X+, r23\n\t"
    "st X+, r19\n\t"
    "st X+, r24\n\t"
    "st X+, r19\n\t"
    : "+x" (out)
    : [timsk4] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr4] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccr4a] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccr4b] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccr4c] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccr4d] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccr4e] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt4l] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocr4a] "n" (_SFR_MEM_ADDR(OCR4A)),
      [ocr4c] "n" (_SFR_MEM_ADDR(OCR4C)),
      [pllfrq] "n" (_SFR_MEM_ADDR(PLLFRQ)),
      [psr4_cs1] "M" (uint8_t(_BV(PSR4) | 1))
    : "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "memory");
}

static void run_measurement(void (*measure)())
{
  uint8_t const sreg = SREG;
  cli();

  PRR0 &= ~(_BV(PRTIM0) | _BV(PRTIM1));
  PRR1 &= ~(_BV(PRTIM3) | _BV(PRTIM4));

  stop_all_timers();

  measure();

  stop_all_timers();
  SREG = sreg;

  emit_result(current_result);
}

static void run_measurements()
{
  run_measurement(measure_t1_read_latch);
  run_measurement(measure_t1_write_latch);
  run_measurement(measure_compare_block_t0_t1);
  run_measurement(measure_top_bottom_flags_t1);
  run_measurement(measure_double_buffer_t1);
  run_measurement(measure_foc_t0_t3);
  run_measurement(measure_oc_pin_t0_t3_t4);
  run_measurement(measure_prr_freeze_t1_t4);
  run_measurement(measure_t4_tcnt_write_sync);
  run_measurement(measure_t4_tc4h_latch);
  run_measurement(measure_t4_psr_tlock_buffer);
}

void setup()
{
  arduboy.boot();
  wait_for_serial();
  wait_for_pll_lock();
  emit_header();
  run_measurements();
  emit_footer();
}

void loop()
{
  if(!arduboy.nextFrame())
    return;

  arduboy.clear();
  arduboy.setCursor(0, 0);
  arduboy.print(F("timer edge"));
  arduboy.setCursor(0, 10);
  arduboy.print(F("serial emitted"));
  arduboy.display();
}
