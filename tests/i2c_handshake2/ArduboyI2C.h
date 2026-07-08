/*
MIT License

Copyright (c) 2024-2026 sub1inear

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/** @file
 * \brief
 * An I2C library for Arduboy multiplayer games.
 */
#pragma once
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/twi.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#ifndef I2C_FREQUENCY
/** \brief
 * The initial I2C frequency in Hz.
 * \details
 * Defaults to 100000 Hz, with a maximum of 400000 Hz. \n
 * Standard frequencies: \n
 * 100000 Hz - Standard Mode \n
 * 400000 Hz - Fast Mode
 */
#define I2C_FREQUENCY 100000
#elif I2C_FREQUENCY > 400000
#error "I2C_FREQUENCY is too high."
#endif

#ifndef I2C_BUFFER_SIZE
/** \brief
 * The size of the buffer used for writes/target (slave) operations.
 * \details
 * Defaults to 32. If more than 32 bytes are needed for writes/target (slave) operations, increase. If more RAM is needed, decrease.
 * Maximum is 255.
 */
#define I2C_BUFFER_SIZE 32
#elif I2C_BUFFER_SIZE > 255
#error "I2C_BUFFER_SIZE is too big."
#endif

#ifndef I2C_CHECK_BUS_BUSY_CHECKS
/** \brief
 * The amount of times the bus is checked before continuing with a read/write operation.
 * \details
 * Defaults to 16, with a maximum of 255. Fixes design flaw where TWI hardware does not check if the bus has become busy during a stop interrupt,
 * so if multiple targets (slaves) receive the stop interrupt right before
 * they become the controller (master) and send a start, they all will think the bus is free and clobber each other.
 * Increase if the game ever freezes.
 * More information: https://www.robotroom.com/Atmel-AVR-TWI-I2C-Multi-Master-Problem.html
 */
#define I2C_CHECK_BUS_BUSY_CHECKS 16
#elif I2C_CHECK_BUS_BUSY_CHECKS > 255
#error "I2C_CHECK_BUS_BUSY_CHECKS is too big."
#endif

#ifndef I2C_CHECK_CABLE_FLIPPED_CHECKS
/** \brief
 * The total number of checks to perform when checking for a flipped cable.
 * \details
 * Defaults to 128, with a maximum of 255. Increase for a more accurate detection at the cost of a longer detection time.
 */
#define I2C_CHECK_CABLE_FLIPPED_CHECKS 128
#elif I2C_CHECK_CABLE_FLIPPED_CHECKS > 255
#error "I2C_CHECK_CABLE_FLIPPED_CHECKS is too big."
#endif

#ifndef I2C_CHECK_CABLE_FLIPPED_DEBOUNCE_TIMEOUT
/** \brief
 * The amount of time in milliseconds to debounce cable changes when checking for a flipped cable.
 * \details
 * Defaults to 1000 ms, with a maximum of 32767 ms. Increase for more stable detection at the cost of a longer wait time when flipping the cable.
 */
#define I2C_CHECK_CABLE_FLIPPED_DEBOUNCE_TIMEOUT 1000
#elif I2C_CHECK_CABLE_FLIPPED_DEBOUNCE_TIMEOUT > 32767
#error "I2C_CHECK_CABLE_FLIPPED_DEBOUNCE_TIMEOUT is too big."
#endif

#ifndef I2C_USE_HANDSHAKE
/** \brief
 * Whether or not to enable handshake functionality.
 * \details
 * Defaults to 1.
 * Set to 0 if you do not use the built-in handshake (e.g. you use a custom handshake or no handshake at all) to save memory.
 */
#define I2C_USE_HANDSHAKE 1
#endif

#ifndef I2C_USE_CHECK_BUS_BUSY
/** \brief
 * Whether or not to enable bus busy checking functionality.
 * \details
 * Defaults to 1.
 * Set to 0 if you do not need to check if the bus is busy (e.g. you only have one controller (master)).
 */
#define I2C_USE_CHECK_BUS_BUSY 1
#endif

#ifndef I2C_USE_CHECK_CABLE_FLIPPED
/** \brief
 * Whether or not to enable flipped cable detection functionality.
 * \details
 * Defaults to 1.
 * Set to 0 if you do not need to detect flipped cables (e.g. you are only targeting the Arduboy Mini) to save memory.
 */
#define I2C_USE_CHECK_CABLE_FLIPPED 1
#endif

#ifndef I2C_SDA_BIT
/** \brief
 * The bit of the pin on which the SDA line is connected.
 * \details
 * Defaults to PIND1.
 */
#define I2C_SDA_BIT PIND1
#endif

#ifndef I2C_SCL_BIT
/** \brief
 * The bit of the pin on which the SCL line is connected.
 * \details
 * Defaults to PIND0.
 */
#define I2C_SCL_BIT PIND0
#endif

#ifndef I2C_PIN
/** \brief
 * The pin on which the SDA and SCL lines is connected.
 * \details
 * Defaults to PIND.
 * \note
 * There can be only one pin for the SDA and SCL lines to increase optimization.
 */
#define I2C_PIN PIND
#endif

#ifndef I2C_PORT
/** \brief
 * The port on which the SDA and SCL lines is connected.
 * \details
 * Defaults to PORTD.
 * \note
 * There can be only one port for the SDA and SCL lines to increase optimization.
 */
#define I2C_PORT PORTD
#endif

#ifndef I2C_DDR
/** \brief
 * The data direction register for the SDA and SCL lines.
 * \details
 * Defaults to DDRD.
 * \note
 * There can be only one data direction register for the SDA and SCL lines to increase optimization.
 */
#define I2C_DDR DDRD
#endif

/** \brief
 * The address used for general calls.
 * \details
 * General calls are sent to this address and are received by every device on the bus.
 */
#define I2C_GENERAL_CALL 0x00

/** \brief
 * Error code returned by I2C::error(), meaning success.
 */
#define TW_SUCCESS 0xFF

/** \brief
 * Error code returned by I2C::handshake, meaning a handshake has already been completed by the number of players specified.
 */
#define I2C_HANDSHAKE_FULL 0xFE

/** \brief
 * The maximum amount of ids available to a device.
 * \details
 * I2C uses a 7-bit addressing scheme with 128 available unique addresses.
 * However, addresses 0-7 and 120-127 are reserved by the standard and should not be used.
 * This leaves 112 unique addresses, and by extension, ids, for a device to use.
 */
#define I2C_MAX_IDS 112

/** \brief
 * I2C library version.
 * \details
 * For a given version x.y.z, the library version will be in the form xxxyyzz with no leading zeros on x.
 */
#define I2C_LIB_VER 30000

/**
 * Provides all I2C functionality.
 */
class I2C {
public:
    /** \brief
     * Initializes I2C hardware.
     * \details
     * This function powers on, initializes, and sets up the clock on the TWI hardware.
     * Must be called after the arduboy hardware is initialized as `arduboy.boot()` disables the I2C (TWI) hardware.
     */
    static void begin();

    /** \brief
     * Set the address of the device and whether to enable or disable general calls for it.
     * \param address The 7-bit address which to respond to.
     * Addresses 0-7 and 120-127 are reserved by the standard and should not be used.
     * \param generalCall Whether to enable or disable general calls. Defaults to true.
     * \note
     * General calls are a way for a device to broadcast data to every other device without addressing them individually.
     * They are sent by sending a write to address I2C_GENERAL_CALL. If they are disabled, the device will not respond to them.
     * These two functionalities are combined for efficiency, as together they make up the TWAR register.
     */
    static void setAddress(uint8_t address, bool generalCall = true);

    /** \brief
     * Attempts to become the bus controller (master) and sends data over I2C to the specified address.
     * \param address The 7-bit address which to send the data. To send a general call, use address I2C_GENERAL_CALL.
     * Addresses 1-7 and 120-127 are reserved by the standard and should not be used.
     * \param buffer A pointer to the data to send.
     * \param size The amount of data in bytes to send. This cannot be zero.
     * \param wait Whether or not to wait for the write to complete. If this is false, it will proceed with interrupts.
     * \details
     * \note
     * Sending general calls will only function if the `generalCall` argument of `setAddress` is true on every other device.
     * \note
     * Internally, this function uses a buffer to enable asynchronous writes. The buffer size is controlled by the macro `I2C_BUFFER_SIZE`
     * and defaults to 32. If the program needs to send more than 32 bytes at a time, `I2C_BUFFER_SIZE`
     * must be defined before including to be larger.
     * \see transmit() read()
     */
    static void write(uint8_t address, const void *buffer, uint8_t size, bool wait);

    /** \brief
     * Attempts to become the bus controller (master) and sends an object over I2C to the specified address.
     * \tparam T The type of the object to write.
     * \param address The 7-bit address which to send the data. To send a general call, use address I2C_GENERAL_CALL.
     * Addresses 1-7 and 120-127 are reserved by the standard and should not be used.
     * \param object A reference to the object to send.
     * \param wait Whether or not to wait for the write to complete. If this is false, it will proceed with interrupts.
     * \note
     * Sending general calls will only function if the `generalCall` argument of `setAddress` is true on every other device.
     * \note
     * Interally, this function uses a buffer to enable asynchronous writes. The buffer size is controlled by the macro `I2C_BUFFER_SIZE`
     * and defaults to 32. If the program needs to send more than 32 bytes at a time, `I2C_BUFFER_SIZE`
     * must be defined before including to be larger.
     * \see transmit() read()
     */
    template<typename T>
    static void write(uint8_t address, const T &object, bool wait) {
        static_assert(sizeof(T) <= I2C_BUFFER_SIZE, "Size of T must be less than or equal to I2C_BUFFER_SIZE.");
        I2C::write(address, (const void *)&object, sizeof(T), wait);
    }

    /*
     * This function is deleted to prevent accidental use with pointers.
     */
    template <typename T>
    static void write(uint8_t address, T *object, bool wait) = delete;

    /** \brief
     * Attempts to become the bus controller (master) and reads data over I2C from the specified address.
     * \param address The 7-bit address which to receive the data from.
     * Addresses 0-7 and 120-127 are reserved by the standard and should not be used.
     * \param buffer A pointer to the buffer in which to store the data.
     * \param size The maximum amount of bytes to receive. This cannot be 0 or 255.
     * \details
     * \note
     * Unlike the `write` function, this function is bufferless and is not limited to 32 bytes.
     * \see write()
     */
    static void read(uint8_t address, void *buffer, uint8_t size);

    /** \brief
     * Attempts to become the bus controller (master) and reads an object over I2C from the specified address.
     * \tparam T The type of the object to read.
     * \param address The 7-bit address which to receive the data from.
     * Addresses 0-7 and 120-127 are reserved by the standard and should not be used.
     * \param object A reference to the object in which to store the data.
     * \details
     * Types with sizes greater than or equal to 255 should not be used with this function.
     * \note
     * Unlike the `write` function, this function is bufferless and is not limited to 32 bytes.
     * \see write()
     */
    template<typename T>
    static void read(uint8_t address, T &object) {
        static_assert(sizeof(T) < 255, "Size of T must be less than 255.");
        I2C::read(address, (void *)&object, sizeof(T));
    }

    /*
     * This function is deleted to prevent accidental use with pointers.
     */
    template <typename T>
    static void read(uint8_t address, T *object) = delete;

    /** \brief
     * Replies back to the controller (master).
     * \param buffer A pointer to the data to reply with.
     * \param size The amount of the data in bytes to reply with.
     * \details
     * This function is intended to be called inside the onRequest callback.
     * It fills the reply buffer with data to then be send one byte at a time.
     * It may be called multiple times to accumulate data.
     * \note
     * Internally, this function uses a buffer. The buffer size is controlled by the macro `I2C_BUFFER_SIZE`
     * and defaults to 32. If the program needs to send more than 32 bytes at a time, `I2C_BUFFER_SIZE`
     * must be defined before including to be larger.
     * \see write() onRequest()
     */
    static void reply(const void *buffer, uint8_t size);

    /** \brief
     * Replies back to the controller (master).
     * \tparam T The type of the object to reply with.
     * \param object A reference to the object to reply with.
     * \details
     * This function is intended to be called inside the onRequest callback.
     * It fills the reply buffer with data to then be send one byte at a time.
     * It may be called multiple times to accumulate data.
     * \note
     * Internally, this function uses a buffer. The buffer size is controlled by the macro `I2C_BUFFER_SIZE`
     * and defaults to 32. If the program needs to send more than 32 bytes at a time, `I2C_BUFFER_SIZE`
     * must be defined before including to be larger.
     * \see write() onRequest()
     */
    template <typename T>
    static void reply(const T &object) {
        static_assert(sizeof(T) <= I2C_BUFFER_SIZE, "Size of T must be less than or equal to I2C_BUFFER_SIZE.");
        I2C::reply((const void *)&object, sizeof(T));
    }

    /*
     * This function is deleted to prevent accidental use with pointers.
     */
    template <typename T>
    static void reply(T *object) = delete;

    /** \brief
     * Sets up/disables the callback to be called when data is requested from the device's address (a read).
     * \param function The function to be called when data is requested, or `nullptr` to disable.
     * \details
     * Example Callback and Usage:
     * \code{.cpp}
     * void dataRequest() {
     *   I2C::reply(players[id]);
     * }
     * ...
     * void setup() {
     *   ...
     *   I2C::onRequest(dataRequest);
     * }
     * \endcode
     * \note
     * Interrupts are disabled during this callback.
     * Any functions called within it should not rely on interrupts (i.e. no `Serial`, `delay`, `millis`, etc.).
     * To respond to the controller (master), use `reply` instead of `write`.
     * \see onReceive() reply() read()
     */
    static void onRequest(void (*function)());

    /** \brief
     * Sets up/disables the callback to be called when data is sent to the device's address (a write)
     * \param function The function to be called when data is received, or `nullptr` to disable.
     * \details
     * Example Callback and Usage:
     * \code{.cpp}
     * void dataReceive(const uint8_t *buffer, uint8_t size) {
     *   uint8_t newId = buffer[0];
     *   players[newId].x = buffer[1];
     *   players[newId].y = buffer[2];
     * }
     * ...
     * void setup() {
     *   ...
     *   I2C::onReceive(dataReceive);
     * }
     * \endcode
     * \see onRequest() reply() read()
     */
    static void onReceive(void (*function)(const uint8_t *buffer, uint8_t size));

    /** \brief
     * Gets the hardware error which happened in a previous read or write.
     * \return A byte indicating the error. TW_SUCCESS means no error has occurred.
     * The full list of error codes are available in the avr utils\twi.h.
     */
    static uint8_t error();

    /** \brief
     * Checks if the I2C cable is flipped, calling a function if it is and waiting for it to be flipped back.
     * \param function The function to be called if the cable is flipped.
     * \details
     * This function works by seeing which line behaves more like a clock (equal high and low) over a sampling period.
     * It is by no means perfect, but it should suffice.
     * This is only needed on the FX-C, as the Arduboy Mini does not have a way to flip the cable.
     * This method must be used with I2C::handshake.
     * Example Usage:
     * \code{.cpp}
     * I2C::checkCableFlipped([] {
     *     arduboy.clear();
     *     arduboy.print(F("Please flip the cable\non this device."));
     *     arduboy.display();
     * });
     * uint8_t id = I2C::handshake(2);
     * \endcode
     * \note
     * In order to work with this function, custom handshaking functions must send data at a regular interval.
     * Sending 0b00000000 is recommended as it will increase the chance of detection.
     */
    static void checkCableFlipped(void (*function)());
    /** \brief
     * Checks if an emulator without I2C support is being used to run the code.
     * \return True if an emulator without I2C support has been detected and false if it has not
     */
    static bool checkEmulator();

    /** \brief
     * Gets the address from a provided id.
     * \param id An id between 0 and I2C_MAX_IDS - 1.
     * \return The address corresponding to that id.
     * \details
     * This function is provided to standardize addresses for each id. It is used by I2C::handshake.
     */
    static uint8_t idToAddress(uint8_t id);

    /** \brief
     * Handshakes with other devices and returns a unique id once complete.
     * \param numPlayers The amount of players to wait for before completing the handshake. Must be between 1 and I2C_MAX_IDS.
     * \return A unique id for this device.
     * \details
     * This function may be called only once; further calls will not work.
     * This function will wait until every single player has joined.
     * \note
     * The onReceive() callback will be overriden by this function.
     */
    static uint8_t handshake(uint8_t numPlayers);
};

#ifdef I2C_IMPLEMENTATION
/** \brief
 * Not officially part of the library.
 */
namespace i2c_detail {
#if I2C_USE_HANDSHAKE
volatile uint8_t handshakeState;

void handshakeOnRequest() {
    handshakeState++;
}
#endif // #if I2C_USE_HANDSHAKE

struct i2c_data_t {
    void (*onRequestFunction)() = nullptr;
    void (*onReceiveFunction)(const uint8_t *buffer, uint8_t size) = nullptr;

    volatile uint8_t *rxBuffer;
    uint8_t twiBuffer[I2C_BUFFER_SIZE];
    volatile uint8_t bufferIdx;
    volatile uint8_t bufferSize;

    volatile uint8_t active;
    volatile uint8_t slaRW;
    volatile uint8_t error;

} data;

#if I2C_USE_CHECK_BUS_BUSY
bool checkBusBusy() {
    uint8_t busyChecks = I2C_CHECK_BUS_BUSY_CHECKS;
    while (busyChecks) {
        if ((I2C_PIN & _BV(I2C_SDA_BIT)) && (I2C_PIN & _BV(I2C_SCL_BIT))) {
            busyChecks--;
        } else {
            i2c_detail::data.error = TW_MT_ARB_LOST; // same as TW_MR_ARB_LOST
            i2c_detail::data.active = false;
            return true;
        }
    }
    return false;
}
#endif // #if I2C_USE_CHECK_BUS_BUSY

#if I2C_USE_CHECK_CABLE_FLIPPED
bool checkCableFlippedCore(bool disconnectFlip = false) {
    // count frequency of each state of the SDA and SCL lines
    uint8_t sdaHigh = 0;
    uint8_t sclHigh = 0;
    for (uint8_t i = 0; i < I2C_CHECK_CABLE_FLIPPED_CHECKS; i++) {
        // probably should buffer the pin reads
        // but saves an instruction not to
        // and will only be off by a very small amount
        if (I2C_PIN & _BV(I2C_SDA_BIT)) { sdaHigh++; }
        if (I2C_PIN & _BV(I2C_SCL_BIT)) { sclHigh++; }
        // half-period delay (otherwise way too fast to detect changes)
        // double will be optimized away into constant cycle loop
        _delay_us(1000000.0 / I2C_FREQUENCY / 2.0);
    }
    // if the cable disconnected (both lines high) for the entire check
    // and disconnectFlip is true, return true (flipped)
    // otherwise will return false in same case
    // (sdaScore = halfChecks, sclScore = halfChecks, sdaScore < sclScore is false)
    if (disconnectFlip &&
        sdaHigh == I2C_CHECK_CABLE_FLIPPED_CHECKS &&
        sclHigh == I2C_CHECK_CABLE_FLIPPED_CHECKS) {
        return true;
    }
    // score each line based on its distance from half the checks being high
    constexpr uint8_t halfChecks = I2C_CHECK_CABLE_FLIPPED_CHECKS / 2;
    uint8_t sdaScore = (uint8_t)abs((int8_t)(sdaHigh - halfChecks));
    uint8_t sclScore = (uint8_t)abs((int8_t)(sclHigh - halfChecks));
    // less score means more like a clock
    // so flipped if sdaScore is less than sclScore
    return sdaScore < sclScore;
}
// optimizes for debounce in checkCableFlipped (only needs uint16_t)
uint16_t millisShort() {
    return (uint16_t)millis();
}
#endif // #if I2C_USE_CHECK_CABLE_FLIPPED

void readWriteStart(uint8_t address, bool readWrite) {
    while (i2c_detail::data.active) {}
    i2c_detail::data.active = true;

    i2c_detail::data.error = TW_SUCCESS;
    i2c_detail::data.slaRW = address << 1 | readWrite;
    i2c_detail::data.bufferIdx = 0;
}
}

void I2C::begin() {
    power_twi_enable();
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);

    TWBR = (F_CPU / I2C_FREQUENCY - 16) / 2;

    // clear prescaler bits
    TWSR = 0;

    // exit to bootloader/restart will not clear TWAR,
    // so we clear it here to prevent any issues with handshakes.
    TWAR = 0;

    // enable internal pullups
    I2C_DDR &= ~(_BV(I2C_SDA_BIT) | _BV(I2C_SCL_BIT));
    I2C_PORT |= _BV(I2C_SDA_BIT) | _BV(I2C_SCL_BIT);
}

void I2C::setAddress(uint8_t address, bool generalCall) {
    TWAR = address << 1 | generalCall;
}

void I2C::write(uint8_t address, const void *buffer, uint8_t size, bool wait) {
    i2c_detail::readWriteStart(address, TW_WRITE);

    memcpy(i2c_detail::data.twiBuffer, buffer, size);

    i2c_detail::data.bufferSize = size;

#if I2C_USE_CHECK_BUS_BUSY
    if (i2c_detail::checkBusBusy()) { return; }
#endif // #if I2C_USE_CHECK_BUS_BUSY

    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
    if (wait) {
        while (i2c_detail::data.active) {}
    }
}

void I2C::read(uint8_t address, void *buffer, uint8_t size) {
    i2c_detail::readWriteStart(address, TW_READ);

    i2c_detail::data.rxBuffer = (uint8_t *)buffer;
    i2c_detail::data.bufferSize = size - 1;

#if I2C_USE_CHECK_BUS_BUSY
    if (i2c_detail::checkBusBusy()) { return; }
#endif // #if I2C_USE_CHECK_BUS_BUSY

    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
    while (i2c_detail::data.active) {}
}

void I2C::reply(const void *buffer, uint8_t size) {
    // cache to avoid volatile access
    uint8_t bufferSize = i2c_detail::data.bufferSize;
    memcpy(i2c_detail::data.twiBuffer + bufferSize, buffer, size);
    i2c_detail::data.bufferSize = bufferSize + size;
}

void I2C::onRequest(void (*function)()) {
    i2c_detail::data.onRequestFunction = function;
}

void I2C::onReceive(void (*function)(const uint8_t *buffer, uint8_t size)) {
    i2c_detail::data.onReceiveFunction = function;
}

uint8_t I2C::error() {
    return i2c_detail::data.error;
}

#if I2C_USE_CHECK_CABLE_FLIPPED
void I2C::checkCableFlipped(void (*function)()) {
    // wait to finish ongoing operations
    while (i2c_detail::data.active) { }
    TWCR = 0; // disable TWI

    if (i2c_detail::checkCableFlippedCore()) {
        // inform the user of the flipped cable
        function();
        // wait for cable to be flipped back
        // debounce cable changes
        uint16_t start = i2c_detail::millisShort();
        while (true) {
            if (i2c_detail::checkCableFlippedCore(true)) {
                start = i2c_detail::millisShort();
            } else if (i2c_detail::millisShort() - start >
                       I2C_CHECK_CABLE_FLIPPED_DEBOUNCE_TIMEOUT) {
                break;
            }
        }
    }

    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT); // re-enable TWI
}
#endif // #if I2C_USE_CHECK_CABLE_FLIPPED

bool I2C::checkEmulator() {
    // TWWC is set when TWDR is written to without TWINT being set
    // not done in emulator
    TWDR = 0;
    return !(TWCR & _BV(TWWC));
}

uint8_t I2C::idToAddress(uint8_t id) {
    return 0x8 + id;
}

#if I2C_USE_HANDSHAKE
uint8_t I2C::handshake(uint8_t numPlayers) {
    for (int8_t i = numPlayers - 1; i >= 0; ) {
        uint8_t dummy;
        uint8_t address = I2C::idToAddress(i);

        I2C::read(address, dummy);

        switch (I2C::error()) {
        case TW_MR_SLA_NACK:
            I2C::onRequest(i2c_detail::handshakeOnRequest);
            I2C::setAddress(address);

            // handshakeState is the number of times the callback has been called.
            // when the callback has been called i times, the final Arduboy has joined.
            // cable flipped detection relies on clock detection,
            // so we send 0b00000000 to have SDA never change
            // while detecting it.

#if I2C_USE_CHECK_CABLE_FLIPPED
            dummy = 0b00000000;
            while (i2c_detail::handshakeState < i) {
                I2C::write(I2C_GENERAL_CALL, dummy, true);
            }
#else
            while (i2c_detail::handshakeState < i) { }
#endif // #if I2C_USE_CHECK_CABLE_FLIPPED

            return i;
        case TW_SUCCESS:
            i--;
            break;
        }
    }
    return I2C_HANDSHAKE_FULL;
}

#endif // #if I2C_USE_HANDSHAKE

ISR(TWI_vect, ISR_NAKED) {
    asm volatile (
R"(
; --------------------- defines ----------------------- ;
.equ TWPTR, 0xB9
.equ TWCR, 3
.equ TWSR, 0
.equ TWDR, 2

.equ TWIE, 0
.equ TWEN, 2
.equ TWWC, 3
.equ TWSTO, 4
.equ TWSTA, 5
.equ TWEA, 6
.equ TWINT, 7

.equ REPLY_ACK, (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWEA)
.equ REPLY_NACK, (1 << TWINT) | (1 << TWEN) | (1 << TWIE)
.equ STOP, (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWSTO) | (1 << TWEA)

; -------------------- registers ---------------------- ;
; r18     - TWSR (never used after function call)
; r19     - general use
; r26 (X) - general use
; r27 (X) - general use
; r28 (Y) - data pointer
; r29 (Y) - data pointer
; r30 (Z) - TW register pointer
; r31 (Z) - TW register pointer
; --------------------- prologue ---------------------- ;
push r18
in r18, __SREG__
push r18
; save and restore call-clobbered registers
; target (slave) could call function pointer
push r19
push r20
push r21
push r22
push r23
push r24
push r25
push r26
push r27
push r28
push r29
push r30
push r31
; save and restore tmp and zero registers (could be used in function calls)
push __tmp_reg__
push __zero_reg__
clr __zero_reg__
; ----------------------------------------------------- ;
; set up Y pointer (data)
ldi r28, lo8(%[data])
ldi r29, hi8(%[data])

; set up Z pointer (TW registers)
ldi r30, TWPTR
clr r31

; switch (TWSR)
ldd r18, Z + TWSR ; no mask needed because prescaler bits are cleared

cpi r18, 0x08
breq TW_START

; MT_MR
cpi r18, 0x18
breq TW_MT_SLA_ACK
cpi r18, 0x28
breq TW_MT_DATA_ACK
cpi r18, 0x38
breq TW_MT_ARB_LOST ; same as TW_MR_ARB_LOST
cpi r18, 0x40
breq TW_MR_SLA_ACK
cpi r18, 0x50
breq TW_MR_DATA_ACK
cpi r18, 0x58
breq TW_MR_DATA_NACK

; 64 instruction limit on branches
rjmp SR_ST

TW_START:
    ; TWDR = i2c_detail::data.slaRW;
    ldd r26, Y + %[slaRW]
    std Z + TWDR, r26
    ; TWCR = REPLY_NACK;
    ldi r26, REPLY_NACK
    std Z + TWCR, r26
    ; return;
    rjmp pop_reti

TW_MT_SLA_ACK:
TW_MT_DATA_ACK:
    ; if (i2c_detail::data.bufferIdx >= i2c_detail::data.bufferSize) { stop(); return; }
    ldd r26, Y + %[bufferIdx]
    ldd r27, Y + %[bufferSize]
    cp r26, r27

    brlt 1f ; 64 instruction limit on branches
    rjmp stop_reti
    1:

    ; TWDR = i2c_detail::data.twiBuffer[i2c_detail::data.bufferIdx++];
    inc r26
    std Y + %[bufferIdx], r26

    ; Use SUBI and SBCI as (non-existant) ADDI and (non-existant) ADCI
    ; bufferIdx is already incremented so decrement to compensate

    clr r27
    subi r26, lo8(-(%[twiBuffer] - 1))
    sbci r27, hi8(-(%[twiBuffer] - 1))
    ld r26, X
    std Z + TWDR, r26

    ; TWCR = REPLY_NACK;
    ldi r26, REPLY_NACK
    std Z + TWCR, r26
    ; return;
    rjmp pop_reti

TW_MT_ARB_LOST:
    ; TWCR = REPLY_ACK;
    ldi r26, REPLY_ACK
    std Z + TWCR, r26
    ; i2c_detail::data.error = TW_MT_ARB_LOST;
    ldi r26, 0x38
    std Y + %[error], r26
    ; active = false;
    ; return;
    rjmp active_false_reti
; ----------------------------------------------------- ;

TW_MR_DATA_NACK:
TW_MR_DATA_ACK:
    ; i2c_detail::data.rxBuffer[i2c_detail::data.bufferIdx++] = TWDR;
    ldd r19, Y + %[bufferIdx]
    inc r19
    std Y + %[bufferIdx], r19
    dec r19

    ldd r26, Y + %[rxBuffer]
    ldd r27, Y + %[rxBuffer] + 1

    add r26, r19
    adc r27, __zero_reg__

    ldd r19, Z + TWDR
    st X, r19

    ; if (TWSR == TW_MR_DATA_NACK) { stop(); return; }
    ; r18 holds TWSR
    cpi r18, 0x58
    brne 1f ; 64 instruction limit on branches
    rjmp stop_reti
    1:
; ------------------ fallthrough ---------------------- ;
TW_MR_SLA_ACK:
    ; if (i2c_detail::data.bufferIdx < i2c_detail::data.bufferSize) {
    ;    TWCR = REPLY_ACK;
    ; } else {
    ;    TWCR = REPLY_NACK;
    ; }
    ; return;

    ldd r26, Y + %[bufferIdx]
    ldd r27, Y + %[bufferSize]
    cp r26, r27
    ldi r26, REPLY_ACK
    brlt 1f
    ldi r26, REPLY_NACK
    1:
    std Z + TWCR, r26
    rjmp pop_reti
; ----------------------------------------------------- ;
SR_ST:
cpi r18, 0x60
breq TW_SR_SLA_ACK
cpi r18, 0x68
breq TW_SR_ARB_LOST_SLA_ACK
cpi r18, 0x70
breq TW_SR_GCALL_ACK
cpi r18, 0x78
breq TW_SR_ARB_LOST_GCALL_ACK
cpi r18, 0x80
breq TW_SR_DATA_ACK
cpi r18, 0x90
breq TW_SR_GCALL_DATA_ACK
cpi r18, 0xA0
breq TW_SR_STOP
cpi r18, 0xA8
breq TW_ST_SLA_ACK
cpi r18, 0xB0
breq TW_ST_ARB_LOST_SLA_ACK
cpi r18, 0xB8
breq TW_ST_DATA_ACK
cpi r18, 0xC0
breq TW_ST_DATA_NACK
cpi r18, 0xC8
breq TW_ST_LAST_DATA

rjmp default

TW_SR_SLA_ACK:
TW_SR_ARB_LOST_SLA_ACK:
TW_SR_GCALL_ACK:
TW_SR_ARB_LOST_GCALL_ACK:
    ; i2c_detail::data.active = TWSR; (true)
    std Y + %[active], r18 ; r18 holds TWSR
    ; i2c_detail::data.bufferIdx = 0;
    std Y + %[bufferIdx], __zero_reg__
    ; TWCR = REPLY_ACK;
    ldi r26, REPLY_ACK
    std Z + TWCR, r26
    ; return;
    rjmp pop_reti

TW_SR_DATA_ACK:
TW_SR_GCALL_DATA_ACK:
    ; i2c_detail::data.twiBuffer[i2c_detail::data.bufferIdx++] = TWDR;
    ldd r26, Y + %[bufferIdx]
    inc r26
    std Y + %[bufferIdx], r26

    ; Use SUBI and SBCI as (non-existant) ADDI and (non-existant) ADCI
    ; bufferIdx is already incremented so decrement to compensate

    clr r27
    subi r26, lo8(-(%[twiBuffer] - 1))
    sbci r27, hi8(-(%[twiBuffer] - 1))
    ldd r19, Z + TWDR
    st X, r19

    ; TWCR = REPLY_ACK;
    ldi r26, REPLY_ACK
    std Z + TWCR, r26
    ; return;
    rjmp pop_reti
TW_SR_STOP:
    ; TWCR = REPLY_ACK;
    ldi r26, REPLY_ACK
    std Z + TWCR, r26

    ; if (i2c_detail::data.onReceiveFunction) {
    ;     i2c_detail::data.onReceiveFunction(i2c_detail::data.twiBuffer, i2c_detail::data.bufferIdx);
    ; }
    ldd r22, Y + %[bufferIdx]
    ldi r24, lo8(%[twiBuffer])
    ldi r25, hi8(%[twiBuffer])

    ldd r30, Y + %[onReceiveFunction]
    ldd r31, Y + %[onReceiveFunction] + 1

    cp r30, __zero_reg__
    cpc r31, __zero_reg__
    breq active_false_reti

    icall
    ; i2c_detail::data.active = false;
    ; return;
    rjmp active_false_reti;

; ----------------------------------------------------- ;
TW_ST_ARB_LOST_SLA_ACK:
TW_ST_SLA_ACK:
    ; i2c_detail::data.active = TWSR; (true)
    std Y + %[active], r18
    ; i2c_detail::data.bufferIdx = 0;
    std Y + %[bufferIdx], __zero_reg__
    ; i2c_detail::data.bufferSize = 0;
    std Y + %[bufferSize], __zero_reg__

    ; if (i2c_detail::data.onRequestFunction) {
    ;     i2c_detail::data.onRequestFunction();
    ; }
    ; i2c_detail::data.onRequestFunction();
    ldd r30, Y + %[onRequestFunction]
    ldd r31, Y + %[onRequestFunction] + 1

    cp r30, __zero_reg__
    cpc r31, __zero_reg__
    breq 1f
    icall

    1:
    ; if (i2c_detail::data.bufferSize == 0) {
    ;     TWDR = 0;
    ;     TWCR = REPLY_NACK;
    ;     return;
    ; }
    ldd r18, Y + %[bufferSize]
    tst r18
    brne 2f
    std Z + TWCR, __zero_reg__
    ldi r26, REPLY_NACK
    std Z + TWCR, r26
    rjmp pop_reti
    2:

    ; restore Z pointer
    ldi r30, TWPTR
    clr r31
; ------------------ fallthrough ---------------------- ;
TW_ST_DATA_ACK:
    ; TWDR = i2c_detail::data.twiBuffer[i2c_detail::data.bufferIdx++];
    ldd r26, Y + %[bufferIdx]
    inc r26
    std Y + %[bufferIdx], r26

    ; Use SUBI and SBCI as (non-existant) ADDI and (non-existant) ADCI
    ; bufferIdx is already incremented so decrement to compensate

    clr r27
    subi r26, lo8(-(%[twiBuffer] - 1))
    sbci r27, hi8(-(%[twiBuffer] - 1))
    ld r26, X
    std Z + TWDR, r26

    ; if (i2c_detail::data.bufferIdx < i2c_detail::data.bufferSize) {
    ;    TWCR = REPLY_ACK;
    ; } else {
    ;    TWCR = REPLY_NACK;
    ; }
    ; return;
    ; (reuse code in MR)
    rjmp TW_MR_SLA_ACK
TW_ST_DATA_NACK:
TW_ST_LAST_DATA:
    ; TWCR = REPLY_ACK;
    ldi r26, REPLY_ACK
    std Z + TWCR, r26
    ; i2c_detail::data.active = false;
    ; return;
    rjmp active_false_reti
; ----------------------------------------------------- ;
default:
    ; i2c_detail::data.error = TWSR;
    std Y + %[error], r18

    stop_reti:

    ; TWCR = STOP;
    ldi r26, STOP
    std Z + TWCR, r26

    ; while (TWCR & _BV(TWSTO)) {}
    1:
    ldd r26, Z + TWCR
    sbrc r26, TWSTO ; skip if bit in register clear
    rjmp 1b

    active_false_reti:
    ; i2c_detail::data.active = false;
    std Y + %[active], __zero_reg__

; --------------------- epilogue ---------------------- ;
    pop_reti:
    pop __zero_reg__
    pop __tmp_reg__
    pop r31
    pop r30
    pop r29
    pop r28
    pop r27
    pop r26
    pop r25
    pop r24
    pop r23
    pop r22
    pop r21
    pop r20
    pop r19
    pop r18
    out __SREG__, r18
    pop r18
    reti
)"
        : // Output Operands
        [data]             "=m" (i2c_detail::data),
        [twiBuffer]        "=m" (i2c_detail::data.twiBuffer)
        : // Input Operands
        [error]             "i" (offsetof(i2c_detail::i2c_data_t, error)),
        [active]            "i" (offsetof(i2c_detail::i2c_data_t, active)),
        [bufferIdx]         "i" (offsetof(i2c_detail::i2c_data_t, bufferIdx)),
        [rxBuffer]          "i" (offsetof(i2c_detail::i2c_data_t, rxBuffer)),
        [onRequestFunction] "i" (offsetof(i2c_detail::i2c_data_t, onRequestFunction)),
        [onReceiveFunction] "i" (offsetof(i2c_detail::i2c_data_t, onReceiveFunction)),
        [bufferSize]        "i" (offsetof(i2c_detail::i2c_data_t, bufferSize)),
        [slaRW]             "i" (offsetof(i2c_detail::i2c_data_t, slaRW))
    );
}

// C++ ISR version for reference
#if 0
ISR(TWI_vect) {
    switch (TWSR) { // prescaler bits are cleared, no mask needed
    case TW_START:
        TWDR = i2c_detail::data.slaRW;
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
        break;
    // MT
    case TW_MT_SLA_ACK:
    case TW_MT_DATA_ACK:
        if (i2c_detail::data.bufferIdx < i2c_detail::data.bufferSize) {
            TWDR = i2c_detail::data.twiBuffer[i2c_detail::data.bufferIdx++];
            TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
        } else {
            TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO) | _BV(TWEA);
            while (TWCR & _BV(TWSTO)) {  }
            i2c_detail::data.active = false;
        }
        break;
    case TW_MT_ARB_LOST: // same as TW_MR_ARB_LOST
        i2c_detail::data.active = false;
        i2c_detail::data.error = TW_MT_ARB_LOST;
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
        break;
    // MR
    case TW_MR_DATA_ACK:
        i2c_detail::data.rxBuffer[i2c_detail::data.bufferIdx++] = TWDR;
        __attribute__((fallthrough));
    case TW_MR_SLA_ACK:
        if (i2c_detail::data.bufferIdx < i2c_detail::data.bufferSize) {
            TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
        } else {
            TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
        }
        break;
    case TW_MR_DATA_NACK:
        i2c_detail::data.rxBuffer[i2c_detail::data.bufferIdx++] = TWDR;
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO) | _BV(TWEA);
        while (TWCR & _BV(TWSTO)) {  }
        i2c_detail::data.active = false;
        break;
    // ST
    case TW_ST_SLA_ACK:
    case TW_ST_ARB_LOST_SLA_ACK:
        i2c_detail::data.active = true;
        i2c_detail::data.bufferIdx = 0;
        i2c_detail::data.bufferSize = 0;
        if (i2c_detail::data.onRequestFunction) {
            i2c_detail::data.onRequestFunction();
        }
        if (i2c_detail::data.bufferSize == 0) {
            i2c_detail::data.bufferSize = 1;
        }
        __attribute__((fallthrough));
    case TW_ST_DATA_ACK:
        TWDR = i2c_detail::data.twiBuffer[i2c_detail::data.bufferIdx++];
        if (i2c_detail::data.bufferIdx < i2c_detail::data.bufferSize) {
            TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
        } else {
            TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
        }
        break;
    case TW_ST_DATA_NACK:
    case TW_ST_LAST_DATA: // last interrupt cleared TWEA
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
        i2c_detail::data.active = false;
        break;
    // SR
    case TW_SR_SLA_ACK:
    case TW_SR_GCALL_ACK:
    case TW_SR_ARB_LOST_SLA_ACK:
    case TW_SR_ARB_LOST_GCALL_ACK:
        i2c_detail::data.bufferIdx = 0;
        i2c_detail::data.active = true;
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
        break;
    case TW_SR_GCALL_DATA_ACK:
    case TW_SR_DATA_ACK:
        i2c_detail::data.twiBuffer[i2c_detail::data.bufferIdx++] = TWDR;
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
        break;
    case TW_SR_STOP:
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
        if (i2c_detail::data.onReceiveFunction) {
            i2c_detail::data.onReceiveFunction(i2c_detail::data.twiBuffer, i2c_detail::data.bufferIdx);
        }
        i2c_detail::data.active = false;
        break;
    default:
        i2c_detail::data.error = TWSR;
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO) | _BV(TWEA);
        while (TWCR & _BV(TWSTO)) {  }
        i2c_detail::data.active = false;
        break;
    }
}
#endif // #if 0

#endif // #ifdef I2C_IMPLEMENTATION