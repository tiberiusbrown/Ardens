#include "absim.hpp"

namespace absim
{

std::array<int_vector_info_t, 43> const INT_VECTOR_INFO =
{ {
    { "RESET", "Reset Vector" },
    { "INT0", "External Interrupt Request 0" },
    { "INT1", "External Interrupt Request 1" },
    { "INT2", "External Interrupt Request 2" },
    { "INT3", "External Interrupt Request 3" },
    { nullptr, nullptr },
    { nullptr, nullptr },
    { "INT6", "External Interrupt Request 6" },
    { nullptr, nullptr },
    { "PCINT0", "Pin Change Interrupt Request 0" },
    { "USB_GEN", "USB General Interrupt Request" },
    { "USB_COM", "USB Endpoint/Pipe Interrupt Communication Request" },
    { "WDT", "Watchdog Time-out Interrupt" },
    { nullptr, nullptr },
    { nullptr, nullptr },
    { nullptr, nullptr },
    { "TIMER1_CAPT", "Timer/Counter1 Capture Event" },
    { "TIMER1_COMPA", "Timer/Counter1 Compare Match A" },
    { "TIMER1_COMPB", "Timer/Counter1 Compare Match B" },
    { "TIMER1_COMPC", "Timer/Counter1 Compare Match C" },
    { "TIMER1_OVF", "Timer/Counter1 Overflow" },
    { "TIMER0_COMPA", "Timer/Counter0 Compare Match A" },
    { "TIMER0_COMPB", "Timer/Counter0 Compare Match B" },
    { "TIMER0_OVF", "Timer/Counter0 Overflow" },
    { "SPI_STC", "SPI Serial Transfer Complete" },
    { "USART1_RX", "USART1, Rx Complete" },
    { "USART1_UDRE", "USART1 Data register Empty" },
    { "USART1_TX", "USART1, Tx Complete" },
    { "ANALOG_COMP", "Analog Comparator" },
    { "ADC", "ADC Conversion Complete" },
    { "EE_READY", "EEPROM Ready" },
    { "TIMER3_CAPT", "Timer/Counter3 Capture Event" },
    { "TIMER3_COMPA", "Timer/Counter3 Compare Match A" },
    { "TIMER3_COMPB", "Timer/Counter3 Compare Match B" },
    { "TIMER3_COMPC", "Timer/Counter3 Compare Match C" },
    { "TIMER3_OVF", "Timer/Counter3 Overflow" },
    { "TWI", "2-wire Serial Interface        " },
    { "SPM_READY", "Store Program Memory Read" },
    { "TIMER4_COMPA", "Timer/Counter4 Compare Match A" },
    { "TIMER4_COMPB", "Timer/Counter4 Compare Match B" },
    { "TIMER4_COMPD", "Timer/Counter4 Compare Match D" },
    { "TIMER4_OVF", "Timer/Counter4 Overflow" },
    { "TIMER4_FPF", "Timer/Counter4 Fault Protection Interrupt" },
} };

} // namespace absim
