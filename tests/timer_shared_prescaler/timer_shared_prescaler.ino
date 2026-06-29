#include <Arduboy2.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#ifndef PSRSYNC
#define PSRSYNC 0
#endif

#ifndef TSM
#define TSM 7
#endif

Arduboy2 arduboy;

static constexpr uint8_t RESULT_VALUE_CAPACITY = 10;

struct result_t
{
  const char* label;
  uint8_t count;
  uint16_t value[RESULT_VALUE_CAPACITY];
  uint16_t flags;
};

static result_t results[8];
static uint8_t result_count;

static result_t& next_result(const char* label, uint8_t count)
{
  result_t& r = results[result_count++];
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
  Serial.print(r.label);
  Serial.print(',');
  Serial.print(r.count);
  for(uint8_t i = 0; i < r.count; ++i)
  {
    Serial.print(',');
    Serial.print(r.value[i]);
  }
  Serial.print(',');
  Serial.println(r.flags);
}

static void emit_results()
{
  Serial.println(F("ARDENS_TIMER_SHARED_PRESCALER,1"));
  Serial.print(F("meta,f_cpu,"));
  Serial.println(F_CPU);
  Serial.print(F("meta,gtccr_tsm_psrsync,"));
  Serial.println(uint8_t(_BV(TSM) | _BV(PSRSYNC)));

  for(uint8_t i = 0; i < result_count; ++i)
    emit_result(results[i]);

  Serial.println(F("END"));
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

static void stop_sync_timers()
{
  GTCCR = 0;
  stop_timer0();
  stop_timer1();
  stop_timer3();
}

static void measure_tsm_hold_release()
{
  result_t& r = next_result("tsm_hold_release_div8", 6);
  uint8_t hold0, run0;
  uint16_t hold1, hold3, run1, run3;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[gtccr], r18\n\t"
    "sts %[timsk0], r18\n\t"
    "sts %[timsk1], r18\n\t"
    "sts %[timsk3], r18\n\t"
    "sts %[tccr0a], r18\n\t"
    "sts %[tccr0b], r18\n\t"
    "sts %[tccr1a], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr1c], r18\n\t"
    "sts %[tccr3a], r18\n\t"
    "sts %[tccr3b], r18\n\t"
    "sts %[tccr3c], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "sts %[tifr1], r18\n\t"
    "sts %[tifr3], r18\n\t"
    "ldi r18, 129\n\t"
    "sts %[gtccr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt0], r18\n\t"
    "sts %[tcnt1h], r18\n\t"
    "sts %[tcnt1l], r18\n\t"
    "sts %[tcnt3h], r18\n\t"
    "sts %[tcnt3l], r18\n\t"
    "ldi r18, 2\n\t"
    "sts %[tccr0b], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr3b], r18\n\t"
    ".rept 96\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[hold0], %[tcnt0]\n\t"
    "lds %A[hold1], %[tcnt1l]\n\t"
    "lds %B[hold1], %[tcnt1h]\n\t"
    "lds %A[hold3], %[tcnt3l]\n\t"
    "lds %B[hold3], %[tcnt3h]\n\t"
    "ldi r18, 0\n\t"
    "sts %[gtccr], r18\n\t"
    ".rept 56\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[run0], %[tcnt0]\n\t"
    "lds %A[run1], %[tcnt1l]\n\t"
    "lds %B[run1], %[tcnt1h]\n\t"
    "lds %A[run3], %[tcnt3l]\n\t"
    "lds %B[run3], %[tcnt3h]\n\t"
    : [hold0] "=&r" (hold0), [hold1] "=&r" (hold1), [hold3] "=&r" (hold3),
      [run0] "=&r" (run0), [run1] "=&r" (run1), [run3] "=&r" (run3)
    : [gtccr] "n" (_SFR_MEM_ADDR(GTCCR)),
      [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [timsk3] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tifr3] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tccr3a] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccr3b] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccr3c] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcnt0] "n" (_SFR_MEM_ADDR(TCNT0)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [tcnt3l] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnt3h] "n" (_SFR_MEM_ADDR(TCNT3H))
    : "r18", "memory");

  r.value[0] = hold0;
  r.value[1] = hold1;
  r.value[2] = hold3;
  r.value[3] = run0;
  r.value[4] = run1;
  r.value[5] = run3;
}

static void measure_staggered_start_div64()
{
  result_t& r = next_result("staggered_start_div64", 8);
  uint8_t t0a, t0b, t0c;
  uint16_t t1a, t1b, t3b, t1c, t3c;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[gtccr], r18\n\t"
    "sts %[timsk0], r18\n\t"
    "sts %[timsk1], r18\n\t"
    "sts %[timsk3], r18\n\t"
    "sts %[tccr0a], r18\n\t"
    "sts %[tccr0b], r18\n\t"
    "sts %[tccr1a], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr1c], r18\n\t"
    "sts %[tccr3a], r18\n\t"
    "sts %[tccr3b], r18\n\t"
    "sts %[tccr3c], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "sts %[tifr1], r18\n\t"
    "sts %[tifr3], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt0], r18\n\t"
    "sts %[tcnt1h], r18\n\t"
    "sts %[tcnt1l], r18\n\t"
    "sts %[tcnt3h], r18\n\t"
    "sts %[tcnt3l], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[gtccr], r18\n\t"
    "ldi r18, 3\n\t"
    "sts %[tccr0b], r18\n\t"
    ".rept 50\n\t"
    "nop\n\t"
    ".endr\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 12\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0a], %[tcnt0]\n\t"
    "lds %A[t1a], %[tcnt1l]\n\t"
    "lds %B[t1a], %[tcnt1h]\n\t"
    "sts %[tccr3b], r18\n\t"
    ".rept 48\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0b], %[tcnt0]\n\t"
    "lds %A[t1b], %[tcnt1l]\n\t"
    "lds %B[t1b], %[tcnt1h]\n\t"
    "lds %A[t3b], %[tcnt3l]\n\t"
    "lds %B[t3b], %[tcnt3h]\n\t"
    ".rept 64\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0c], %[tcnt0]\n\t"
    "lds %A[t1c], %[tcnt1l]\n\t"
    "lds %B[t1c], %[tcnt1h]\n\t"
    "lds %A[t3c], %[tcnt3l]\n\t"
    "lds %B[t3c], %[tcnt3h]\n\t"
    : [t0a] "=&r" (t0a), [t1a] "=&r" (t1a),
      [t0b] "=&r" (t0b), [t1b] "=&r" (t1b), [t3b] "=&r" (t3b),
      [t0c] "=&r" (t0c), [t1c] "=&r" (t1c), [t3c] "=&r" (t3c)
    : [gtccr] "n" (_SFR_MEM_ADDR(GTCCR)),
      [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [timsk3] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tifr3] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tccr3a] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccr3b] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccr3c] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcnt0] "n" (_SFR_MEM_ADDR(TCNT0)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [tcnt3l] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnt3h] "n" (_SFR_MEM_ADDR(TCNT3H))
    : "r18", "memory");

  r.value[0] = t0a;
  r.value[1] = t1a;
  r.value[2] = t0b;
  r.value[3] = t1b;
  r.value[4] = t3b;
  r.value[5] = t0c;
  r.value[6] = t1c;
  r.value[7] = t3c;
}

static void measure_psrsync_while_running()
{
  result_t& r = next_result("psrsync_while_running", 9);
  uint8_t t0a, t0b, t0c;
  uint16_t t1a, t3a, t1b, t3b, t1c, t3c;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[gtccr], r18\n\t"
    "sts %[timsk0], r18\n\t"
    "sts %[timsk1], r18\n\t"
    "sts %[timsk3], r18\n\t"
    "sts %[tccr0a], r18\n\t"
    "sts %[tccr0b], r18\n\t"
    "sts %[tccr1a], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr1c], r18\n\t"
    "sts %[tccr3a], r18\n\t"
    "sts %[tccr3b], r18\n\t"
    "sts %[tccr3c], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "sts %[tifr1], r18\n\t"
    "sts %[tifr3], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt0], r18\n\t"
    "sts %[tcnt1h], r18\n\t"
    "sts %[tcnt1l], r18\n\t"
    "sts %[tcnt3h], r18\n\t"
    "sts %[tcnt3l], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[gtccr], r18\n\t"
    "ldi r18, 3\n\t"
    "sts %[tccr0b], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr3b], r18\n\t"
    ".rept 96\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0a], %[tcnt0]\n\t"
    "lds %A[t1a], %[tcnt1l]\n\t"
    "lds %B[t1a], %[tcnt1h]\n\t"
    "lds %A[t3a], %[tcnt3l]\n\t"
    "lds %B[t3a], %[tcnt3h]\n\t"
    "ldi r18, 1\n\t"
    "sts %[gtccr], r18\n\t"
    ".rept 32\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0b], %[tcnt0]\n\t"
    "lds %A[t1b], %[tcnt1l]\n\t"
    "lds %B[t1b], %[tcnt1h]\n\t"
    "lds %A[t3b], %[tcnt3l]\n\t"
    "lds %B[t3b], %[tcnt3h]\n\t"
    ".rept 40\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0c], %[tcnt0]\n\t"
    "lds %A[t1c], %[tcnt1l]\n\t"
    "lds %B[t1c], %[tcnt1h]\n\t"
    "lds %A[t3c], %[tcnt3l]\n\t"
    "lds %B[t3c], %[tcnt3h]\n\t"
    : [t0a] "=&r" (t0a), [t1a] "=&r" (t1a), [t3a] "=&r" (t3a),
      [t0b] "=&r" (t0b), [t1b] "=&r" (t1b), [t3b] "=&r" (t3b),
      [t0c] "=&r" (t0c), [t1c] "=&r" (t1c), [t3c] "=&r" (t3c)
    : [gtccr] "n" (_SFR_MEM_ADDR(GTCCR)),
      [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [timsk3] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tifr3] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tccr3a] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccr3b] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccr3c] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcnt0] "n" (_SFR_MEM_ADDR(TCNT0)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [tcnt3l] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnt3h] "n" (_SFR_MEM_ADDR(TCNT3H))
    : "r18", "memory");

  r.value[0] = t0a;
  r.value[1] = t1a;
  r.value[2] = t3a;
  r.value[3] = t0b;
  r.value[4] = t1b;
  r.value[5] = t3b;
  r.value[6] = t0c;
  r.value[7] = t1c;
  r.value[8] = t3c;
}

static void measure_mixed_taps_tsm()
{
  result_t& r = next_result("mixed_taps_tsm", 9);
  uint8_t t0a, t0b, t0c;
  uint16_t t1a, t3a, t1b, t3b, t1c, t3c;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[gtccr], r18\n\t"
    "sts %[timsk0], r18\n\t"
    "sts %[timsk1], r18\n\t"
    "sts %[timsk3], r18\n\t"
    "sts %[tccr0a], r18\n\t"
    "sts %[tccr0b], r18\n\t"
    "sts %[tccr1a], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr1c], r18\n\t"
    "sts %[tccr3a], r18\n\t"
    "sts %[tccr3b], r18\n\t"
    "sts %[tccr3c], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "sts %[tifr1], r18\n\t"
    "sts %[tifr3], r18\n\t"
    "ldi r18, 129\n\t"
    "sts %[gtccr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt0], r18\n\t"
    "sts %[tcnt1h], r18\n\t"
    "sts %[tcnt1l], r18\n\t"
    "sts %[tcnt3h], r18\n\t"
    "sts %[tcnt3l], r18\n\t"
    "ldi r18, 2\n\t"
    "sts %[tccr0b], r18\n\t"
    "ldi r18, 3\n\t"
    "sts %[tccr1b], r18\n\t"
    "ldi r18, 4\n\t"
    "sts %[tccr3b], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[gtccr], r18\n\t"
    ".rept 300\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0a], %[tcnt0]\n\t"
    "lds %A[t1a], %[tcnt1l]\n\t"
    "lds %B[t1a], %[tcnt1h]\n\t"
    "lds %A[t3a], %[tcnt3l]\n\t"
    "lds %B[t3a], %[tcnt3h]\n\t"
    ".rept 128\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0b], %[tcnt0]\n\t"
    "lds %A[t1b], %[tcnt1l]\n\t"
    "lds %B[t1b], %[tcnt1h]\n\t"
    "lds %A[t3b], %[tcnt3l]\n\t"
    "lds %B[t3b], %[tcnt3h]\n\t"
    ".rept 128\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0c], %[tcnt0]\n\t"
    "lds %A[t1c], %[tcnt1l]\n\t"
    "lds %B[t1c], %[tcnt1h]\n\t"
    "lds %A[t3c], %[tcnt3l]\n\t"
    "lds %B[t3c], %[tcnt3h]\n\t"
    : [t0a] "=&r" (t0a), [t1a] "=&r" (t1a), [t3a] "=&r" (t3a),
      [t0b] "=&r" (t0b), [t1b] "=&r" (t1b), [t3b] "=&r" (t3b),
      [t0c] "=&r" (t0c), [t1c] "=&r" (t1c), [t3c] "=&r" (t3c)
    : [gtccr] "n" (_SFR_MEM_ADDR(GTCCR)),
      [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [timsk3] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tifr3] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tccr3a] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccr3b] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccr3c] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcnt0] "n" (_SFR_MEM_ADDR(TCNT0)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [tcnt3l] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnt3h] "n" (_SFR_MEM_ADDR(TCNT3H))
    : "r18", "memory");

  r.value[0] = t0a;
  r.value[1] = t1a;
  r.value[2] = t3a;
  r.value[3] = t0b;
  r.value[4] = t1b;
  r.value[5] = t3b;
  r.value[6] = t0c;
  r.value[7] = t1c;
  r.value[8] = t3c;
}

static void measure_prescaler_free_run()
{
  result_t& r = next_result("prescaler_free_run", 6);
  uint16_t t1a, t3a, t1b, t3b;
  uint8_t t0b, gtccr_end;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[gtccr], r18\n\t"
    "sts %[timsk0], r18\n\t"
    "sts %[timsk1], r18\n\t"
    "sts %[timsk3], r18\n\t"
    "sts %[tccr0a], r18\n\t"
    "sts %[tccr0b], r18\n\t"
    "sts %[tccr1a], r18\n\t"
    "sts %[tccr1b], r18\n\t"
    "sts %[tccr1c], r18\n\t"
    "sts %[tccr3a], r18\n\t"
    "sts %[tccr3b], r18\n\t"
    "sts %[tccr3c], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr0], r18\n\t"
    "sts %[tifr1], r18\n\t"
    "sts %[tifr3], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt0], r18\n\t"
    "sts %[tcnt1h], r18\n\t"
    "sts %[tcnt1l], r18\n\t"
    "sts %[tcnt3h], r18\n\t"
    "sts %[tcnt3l], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[gtccr], r18\n\t"
    ".rept 50\n\t"
    "nop\n\t"
    ".endr\n\t"
    "ldi r18, 3\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[t1a], %[tcnt1l]\n\t"
    "lds %B[t1a], %[tcnt1h]\n\t"
    "ldi r18, 0\n\t"
    "sts %[tccr1b], r18\n\t"
    ".rept 43\n\t"
    "nop\n\t"
    ".endr\n\t"
    "ldi r18, 3\n\t"
    "sts %[tccr3b], r18\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[t3a], %[tcnt3l]\n\t"
    "lds %B[t3a], %[tcnt3h]\n\t"
    "sts %[tccr0b], r18\n\t"
    ".rept 64\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[t0b], %[tcnt0]\n\t"
    "lds %A[t1b], %[tcnt1l]\n\t"
    "lds %B[t1b], %[tcnt1h]\n\t"
    "lds %A[t3b], %[tcnt3l]\n\t"
    "lds %B[t3b], %[tcnt3h]\n\t"
    "lds %[gtccr_end], %[gtccr]\n\t"
    : [t1a] "=&r" (t1a), [t3a] "=&r" (t3a),
      [t0b] "=&r" (t0b), [t1b] "=&r" (t1b), [t3b] "=&r" (t3b),
      [gtccr_end] "=&r" (gtccr_end)
    : [gtccr] "n" (_SFR_MEM_ADDR(GTCCR)),
      [timsk0] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [timsk1] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [timsk3] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr0] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tifr1] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tifr3] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccr0a] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccr0b] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tccr1a] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccr1b] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccr1c] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tccr3a] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccr3b] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccr3c] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcnt0] "n" (_SFR_MEM_ADDR(TCNT0)),
      [tcnt1l] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnt1h] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [tcnt3l] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnt3h] "n" (_SFR_MEM_ADDR(TCNT3H))
    : "r18", "memory");

  r.value[0] = t1a;
  r.value[1] = t3a;
  r.value[2] = t0b;
  r.value[3] = t1b;
  r.value[4] = t3b;
  r.value[5] = gtccr_end;
}

static void run_measurements()
{
  uint8_t const sreg = SREG;
  cli();

  PRR0 &= ~(_BV(PRTIM0) | _BV(PRTIM1));
  PRR1 &= ~_BV(PRTIM3);

  stop_sync_timers();

  measure_tsm_hold_release();
  measure_staggered_start_div64();
  measure_psrsync_while_running();
  measure_mixed_taps_tsm();
  measure_prescaler_free_run();

  stop_sync_timers();
  SREG = sreg;
}

void setup()
{
  arduboy.boot();
  wait_for_serial();
  run_measurements();
  emit_results();
}

void loop()
{
  if(!arduboy.nextFrame())
    return;

  arduboy.clear();
  arduboy.setCursor(0, 0);
  arduboy.print(F("shared prescaler"));
  arduboy.setCursor(0, 10);
  arduboy.print(F("serial emitted"));
  arduboy.display();
}
