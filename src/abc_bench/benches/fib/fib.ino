#include <stdint.h>

uint16_t fib(uint16_t x)
{
    if(x <= 1) return x;
    return fib(x - 1) + fib(x - 2);
}

volatile uint16_t x;

int main()
{
    asm volatile("break\n");
    x = fib(20);
    asm volatile("break\n");
}
