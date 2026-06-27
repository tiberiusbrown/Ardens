#include <Arduboy2.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#ifndef PRTIM4
#define PRTIM4 4
#endif

Arduboy2 arduboy;

struct result_t
{
  const char* label;
  uint8_t count;
  uint16_t value[8];
  uint16_t flags;
};

static result_t results[16];
static uint8_t result_count;
static uint8_t pllcsr_after_wait;
static uint8_t pllfrq_after_wait;

static result_t& next_result(const char* label)
{
  result_t& r = results[result_count++];
  r.label = label;
  r.count = 8;
  r.flags = 0;
  for(uint8_t i = 0; i < 8; ++i)
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
  Serial.println(F("ARDENS_TIMER_CYCLE_ACCURACY,1"));
  Serial.print(F("meta,f_cpu,"));
  Serial.println(F_CPU);
  Serial.print(F("meta,pllcsr,"));
  Serial.println(pllcsr_after_wait);
  Serial.print(F("meta,pllfrq,"));
  Serial.println(pllfrq_after_wait);

  for(uint8_t i = 0; i < result_count; ++i)
    emit_result(results[i]);

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
}

static void measure_t0_normal()
{
  result_t& r = next_result("t0_normal");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 37\n\t"
    "sts %[ocra], r18\n\t"
    "ldi r18, 19\n\t"
    "sts %[ocrb], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 48\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    "ldi r18, 250\n\t"
    "sts %[tcnt], r18\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT0)),
      [ocra] "n" (_SFR_MEM_ADDR(OCR0A)),
      [ocrb] "n" (_SFR_MEM_ADDR(OCR0B))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t0_ctc()
{
  result_t& r = next_result("t0_ctc_top");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 17\n\t"
    "sts %[ocra], r18\n\t"
    "ldi r18, 13\n\t"
    "sts %[ocrb], r18\n\t"
    "ldi r18, 12\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 2\n\t"
    "sts %[tccra], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 8\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT0)),
      [ocra] "n" (_SFR_MEM_ADDR(OCR0A)),
      [ocrb] "n" (_SFR_MEM_ADDR(OCR0B))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t0_phase()
{
  result_t& r = next_result("t0_phase_ocra");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 9\n\t"
    "sts %[ocra], r18\n\t"
    "ldi r18, 4\n\t"
    "sts %[ocrb], r18\n\t"
    "ldi r18, 7\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccra], r18\n\t"
    "ldi r18, 9\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR0)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT0)),
      [ocra] "n" (_SFR_MEM_ADDR(OCR0A)),
      [ocrb] "n" (_SFR_MEM_ADDR(OCR0B))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t0_div8()
{
  result_t& r = next_result("t0_normal_div8");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[gtccr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 2\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 64\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK0)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR0)),
      [gtccr] "n" (_SFR_MEM_ADDR(GTCCR)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR0A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR0B)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT0))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t1_normal()
{
  result_t& r = next_result("t1_normal");
  uint16_t v0, v1, v2, v3, v4, v5, v6, v7;
  uint8_t f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnth], r18\n\t"
    "sts %[tcntl], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 48\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[v0], %[tcntl]\n\t"
    "lds %B[v0], %[tcnth]\n\t"
    "lds %A[v1], %[tcntl]\n\t"
    "lds %B[v1], %[tcnth]\n\t"
    "lds %A[v2], %[tcntl]\n\t"
    "lds %B[v2], %[tcnth]\n\t"
    "lds %A[v3], %[tcntl]\n\t"
    "lds %B[v3], %[tcnth]\n\t"
    "ldi r18, 1\n\t"
    "sts %[tcnth], r18\n\t"
    "ldi r18, 244\n\t"
    "sts %[tcntl], r18\n\t"
    "lds %A[v4], %[tcntl]\n\t"
    "lds %B[v4], %[tcnth]\n\t"
    "lds %A[v5], %[tcntl]\n\t"
    "lds %B[v5], %[tcnth]\n\t"
    "lds %A[v6], %[tcntl]\n\t"
    "lds %B[v6], %[tcnth]\n\t"
    "lds %A[v7], %[tcntl]\n\t"
    "lds %B[v7], %[tcnth]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tcntl] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnth] "n" (_SFR_MEM_ADDR(TCNT1H))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t1_ctc()
{
  result_t& r = next_result("t1_ctc_ocra");
  uint16_t v0, v1, v2, v3, v4, v5, v6, v7;
  uint8_t f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocrah], r18\n\t"
    "ldi r18, 31\n\t"
    "sts %[ocral], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocrbh], r18\n\t"
    "ldi r18, 19\n\t"
    "sts %[ocrbl], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocrch], r18\n\t"
    "ldi r18, 23\n\t"
    "sts %[ocrcl], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnth], r18\n\t"
    "ldi r18, 26\n\t"
    "sts %[tcntl], r18\n\t"
    "ldi r18, 9\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 4\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[v0], %[tcntl]\n\t"
    "lds %B[v0], %[tcnth]\n\t"
    "lds %A[v1], %[tcntl]\n\t"
    "lds %B[v1], %[tcnth]\n\t"
    "lds %A[v2], %[tcntl]\n\t"
    "lds %B[v2], %[tcnth]\n\t"
    "lds %A[v3], %[tcntl]\n\t"
    "lds %B[v3], %[tcnth]\n\t"
    "lds %A[v4], %[tcntl]\n\t"
    "lds %B[v4], %[tcnth]\n\t"
    "lds %A[v5], %[tcntl]\n\t"
    "lds %B[v5], %[tcnth]\n\t"
    "lds %A[v6], %[tcntl]\n\t"
    "lds %B[v6], %[tcnth]\n\t"
    "lds %A[v7], %[tcntl]\n\t"
    "lds %B[v7], %[tcnth]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tcntl] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnth] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [ocral] "n" (_SFR_MEM_ADDR(OCR1AL)),
      [ocrah] "n" (_SFR_MEM_ADDR(OCR1AH)),
      [ocrbl] "n" (_SFR_MEM_ADDR(OCR1BL)),
      [ocrbh] "n" (_SFR_MEM_ADDR(OCR1BH)),
      [ocrcl] "n" (_SFR_MEM_ADDR(OCR1CL)),
      [ocrch] "n" (_SFR_MEM_ADDR(OCR1CH))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t1_phase_icr()
{
  result_t& r = next_result("t1_phase_icr");
  uint16_t v0, v1, v2, v3, v4, v5, v6, v7;
  uint8_t f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[icrh], r18\n\t"
    "ldi r18, 15\n\t"
    "sts %[icrl], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocrah], r18\n\t"
    "ldi r18, 7\n\t"
    "sts %[ocral], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnth], r18\n\t"
    "ldi r18, 12\n\t"
    "sts %[tcntl], r18\n\t"
    "ldi r18, 17\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[v0], %[tcntl]\n\t"
    "lds %B[v0], %[tcnth]\n\t"
    "lds %A[v1], %[tcntl]\n\t"
    "lds %B[v1], %[tcnth]\n\t"
    "lds %A[v2], %[tcntl]\n\t"
    "lds %B[v2], %[tcnth]\n\t"
    "lds %A[v3], %[tcntl]\n\t"
    "lds %B[v3], %[tcnth]\n\t"
    "lds %A[v4], %[tcntl]\n\t"
    "lds %B[v4], %[tcnth]\n\t"
    "lds %A[v5], %[tcntl]\n\t"
    "lds %B[v5], %[tcnth]\n\t"
    "lds %A[v6], %[tcntl]\n\t"
    "lds %B[v6], %[tcnth]\n\t"
    "lds %A[v7], %[tcntl]\n\t"
    "lds %B[v7], %[tcnth]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tcntl] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnth] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [icrl] "n" (_SFR_MEM_ADDR(ICR1L)),
      [icrh] "n" (_SFR_MEM_ADDR(ICR1H)),
      [ocral] "n" (_SFR_MEM_ADDR(OCR1AL)),
      [ocrah] "n" (_SFR_MEM_ADDR(OCR1AH))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t1_fast_pwm_icr()
{
  result_t& r = next_result("t1_fast_icr");
  uint16_t v0, v1, v2, v3, v4, v5, v6, v7;
  uint8_t f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[icrh], r18\n\t"
    "ldi r18, 21\n\t"
    "sts %[icrl], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocrah], r18\n\t"
    "ldi r18, 11\n\t"
    "sts %[ocral], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnth], r18\n\t"
    "ldi r18, 18\n\t"
    "sts %[tcntl], r18\n\t"
    "ldi r18, 2\n\t"
    "sts %[tccra], r18\n\t"
    "ldi r18, 25\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[v0], %[tcntl]\n\t"
    "lds %B[v0], %[tcnth]\n\t"
    "lds %A[v1], %[tcntl]\n\t"
    "lds %B[v1], %[tcnth]\n\t"
    "lds %A[v2], %[tcntl]\n\t"
    "lds %B[v2], %[tcnth]\n\t"
    "lds %A[v3], %[tcntl]\n\t"
    "lds %B[v3], %[tcnth]\n\t"
    "lds %A[v4], %[tcntl]\n\t"
    "lds %B[v4], %[tcnth]\n\t"
    "lds %A[v5], %[tcntl]\n\t"
    "lds %B[v5], %[tcnth]\n\t"
    "lds %A[v6], %[tcntl]\n\t"
    "lds %B[v6], %[tcnth]\n\t"
    "lds %A[v7], %[tcntl]\n\t"
    "lds %B[v7], %[tcnth]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK1)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR1)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR1A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR1B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR1C)),
      [tcntl] "n" (_SFR_MEM_ADDR(TCNT1L)),
      [tcnth] "n" (_SFR_MEM_ADDR(TCNT1H)),
      [icrl] "n" (_SFR_MEM_ADDR(ICR1L)),
      [icrh] "n" (_SFR_MEM_ADDR(ICR1H)),
      [ocral] "n" (_SFR_MEM_ADDR(OCR1AL)),
      [ocrah] "n" (_SFR_MEM_ADDR(OCR1AH))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t3_normal()
{
  result_t& r = next_result("t3_normal");
  uint16_t v0, v1, v2, v3, v4, v5, v6, v7;
  uint8_t f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnth], r18\n\t"
    "sts %[tcntl], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 48\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[v0], %[tcntl]\n\t"
    "lds %B[v0], %[tcnth]\n\t"
    "lds %A[v1], %[tcntl]\n\t"
    "lds %B[v1], %[tcnth]\n\t"
    "lds %A[v2], %[tcntl]\n\t"
    "lds %B[v2], %[tcnth]\n\t"
    "lds %A[v3], %[tcntl]\n\t"
    "lds %B[v3], %[tcnth]\n\t"
    "ldi r18, 1\n\t"
    "sts %[tcnth], r18\n\t"
    "ldi r18, 244\n\t"
    "sts %[tcntl], r18\n\t"
    "lds %A[v4], %[tcntl]\n\t"
    "lds %B[v4], %[tcnth]\n\t"
    "lds %A[v5], %[tcntl]\n\t"
    "lds %B[v5], %[tcnth]\n\t"
    "lds %A[v6], %[tcntl]\n\t"
    "lds %B[v6], %[tcnth]\n\t"
    "lds %A[v7], %[tcntl]\n\t"
    "lds %B[v7], %[tcnth]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcntl] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnth] "n" (_SFR_MEM_ADDR(TCNT3H))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t3_compare_toggle()
{
  result_t& r = next_result("t3_ctc_toggle_c6");
  uint16_t v0, v1, v2, v3;
  uint8_t p0, p1, p2, p3, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "lds r18, %[ddrc]\n\t"
    "ori r18, 64\n\t"
    "sts %[ddrc], r18\n\t"
    "lds r18, %[portc]\n\t"
    "andi r18, 191\n\t"
    "sts %[portc], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[ocrah], r18\n\t"
    "ldi r18, 17\n\t"
    "sts %[ocral], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tcnth], r18\n\t"
    "ldi r18, 14\n\t"
    "sts %[tcntl], r18\n\t"
    "ldi r18, 64\n\t"
    "sts %[tccra], r18\n\t"
    "ldi r18, 9\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %A[v0], %[tcntl]\n\t"
    "lds %B[v0], %[tcnth]\n\t"
    "lds %[p0], %[portc]\n\t"
    "lds %A[v1], %[tcntl]\n\t"
    "lds %B[v1], %[tcnth]\n\t"
    "lds %[p1], %[portc]\n\t"
    "lds %A[v2], %[tcntl]\n\t"
    "lds %B[v2], %[tcnth]\n\t"
    "lds %[p2], %[portc]\n\t"
    "lds %A[v3], %[tcntl]\n\t"
    "lds %B[v3], %[tcnth]\n\t"
    "lds %[p3], %[portc]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [p0] "=&r" (p0), [p1] "=&r" (p1), [p2] "=&r" (p2), [p3] "=&r" (p3),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK3)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR3)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR3A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR3B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR3C)),
      [tcntl] "n" (_SFR_MEM_ADDR(TCNT3L)),
      [tcnth] "n" (_SFR_MEM_ADDR(TCNT3H)),
      [ocral] "n" (_SFR_MEM_ADDR(OCR3AL)),
      [ocrah] "n" (_SFR_MEM_ADDR(OCR3AH)),
      [ddrc] "n" (_SFR_MEM_ADDR(DDRC)),
      [portc] "n" (_SFR_MEM_ADDR(PORTC))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = p0;
  r.value[2] = v1;
  r.value[3] = p1;
  r.value[4] = v2;
  r.value[5] = p2;
  r.value[6] = v3;
  r.value[7] = p3;
  r.flags = f & 0x0f;
}

static void measure_t4_normal()
{
  result_t& r = next_result("t4_normal");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "sts %[tccrd], r18\n\t"
    "sts %[tccre], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 64\n\t"
    "sts %[tccrb], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[ocrc], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 129\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 48\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    "ldi r18, 250\n\t"
    "sts %[tcnt], r18\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccrd] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccre] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocrc] "n" (_SFR_MEM_ADDR(OCR4C))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t4_fast_top()
{
  result_t& r = next_result("t4_fast_ocrc");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "sts %[tccrd], r18\n\t"
    "sts %[tccre], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 9\n\t"
    "sts %[ocra], r18\n\t"
    "ldi r18, 13\n\t"
    "sts %[ocrb], r18\n\t"
    "ldi r18, 31\n\t"
    "sts %[ocrc], r18\n\t"
    "ldi r18, 17\n\t"
    "sts %[ocrd], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 26\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 129\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccrd] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccre] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocra] "n" (_SFR_MEM_ADDR(OCR4A)),
      [ocrb] "n" (_SFR_MEM_ADDR(OCR4B)),
      [ocrc] "n" (_SFR_MEM_ADDR(OCR4C)),
      [ocrd] "n" (_SFR_MEM_ADDR(OCR4D))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t4_phase_top()
{
  result_t& r = next_result("t4_phase_ocrc");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "sts %[tccrd], r18\n\t"
    "sts %[tccre], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 7\n\t"
    "sts %[ocra], r18\n\t"
    "ldi r18, 15\n\t"
    "sts %[ocrc], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "ldi r18, 12\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 1\n\t"
    "sts %[tccrd], r18\n\t"
    "ldi r18, 129\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 2\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccrd] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccre] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocra] "n" (_SFR_MEM_ADDR(OCR4A)),
      [ocrc] "n" (_SFR_MEM_ADDR(OCR4C))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void measure_t4_div8()
{
  result_t& r = next_result("t4_normal_div8");
  uint8_t v0, v1, v2, v3, v4, v5, v6, v7, f;

  asm volatile(
    "ldi r18, 0\n\t"
    "sts %[timsk], r18\n\t"
    "sts %[tccra], r18\n\t"
    "sts %[tccrb], r18\n\t"
    "sts %[tccrc], r18\n\t"
    "sts %[tccrd], r18\n\t"
    "sts %[tccre], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[tifr], r18\n\t"
    "ldi r18, 255\n\t"
    "sts %[ocrc], r18\n\t"
    "ldi r18, 0\n\t"
    "sts %[tc4h], r18\n\t"
    "sts %[tcnt], r18\n\t"
    "ldi r18, 132\n\t"
    "sts %[tccrb], r18\n\t"
    ".rept 64\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v0], %[tcnt]\n\t"
    "lds %[v1], %[tcnt]\n\t"
    "lds %[v2], %[tcnt]\n\t"
    "lds %[v3], %[tcnt]\n\t"
    ".rept 16\n\t"
    "nop\n\t"
    ".endr\n\t"
    "lds %[v4], %[tcnt]\n\t"
    "lds %[v5], %[tcnt]\n\t"
    "lds %[v6], %[tcnt]\n\t"
    "lds %[v7], %[tcnt]\n\t"
    "lds %[f], %[tifr]\n\t"
    : [v0] "=&r" (v0), [v1] "=&r" (v1), [v2] "=&r" (v2), [v3] "=&r" (v3),
      [v4] "=&r" (v4), [v5] "=&r" (v5), [v6] "=&r" (v6), [v7] "=&r" (v7),
      [f] "=&r" (f)
    : [timsk] "n" (_SFR_MEM_ADDR(TIMSK4)),
      [tifr] "n" (_SFR_MEM_ADDR(TIFR4)),
      [tccra] "n" (_SFR_MEM_ADDR(TCCR4A)),
      [tccrb] "n" (_SFR_MEM_ADDR(TCCR4B)),
      [tccrc] "n" (_SFR_MEM_ADDR(TCCR4C)),
      [tccrd] "n" (_SFR_MEM_ADDR(TCCR4D)),
      [tccre] "n" (_SFR_MEM_ADDR(TCCR4E)),
      [tcnt] "n" (_SFR_MEM_ADDR(TCNT4L)),
      [tc4h] "n" (_SFR_MEM_ADDR(TC4H)),
      [ocrc] "n" (_SFR_MEM_ADDR(OCR4C))
    : "r18", "memory");

  r.value[0] = v0;
  r.value[1] = v1;
  r.value[2] = v2;
  r.value[3] = v3;
  r.value[4] = v4;
  r.value[5] = v5;
  r.value[6] = v6;
  r.value[7] = v7;
  r.flags = f;
}

static void run_measurements()
{
  uint8_t const sreg = SREG;
  cli();

  PRR0 &= ~(_BV(PRTIM0) | _BV(PRTIM1));
  PRR1 &= ~(_BV(PRTIM3) | _BV(PRTIM4));

  stop_all_timers();

  measure_t0_normal();
  measure_t0_ctc();
  measure_t0_phase();
  measure_t0_div8();

  measure_t1_normal();
  measure_t1_ctc();
  measure_t1_phase_icr();
  measure_t1_fast_pwm_icr();

  measure_t3_normal();
  measure_t3_compare_toggle();

  measure_t4_normal();
  measure_t4_fast_top();
  measure_t4_phase_top();
  measure_t4_div8();

  stop_all_timers();
  SREG = sreg;
}

void setup()
{
  arduboy.boot();
  wait_for_serial();
  wait_for_pll_lock();
  run_measurements();
  emit_results();
}

void loop()
{
}
