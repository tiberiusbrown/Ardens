;===============================================================================
;                                                             licenced under MIT
;                              ** Cathy 3K **
;
;  An optimized reversed Caterina bootloader with added Arduboy features in 3K
;
;                   Assembly optimalisation and Arduboy features
;                         by Mr.Blinky Oct.2017 - Oct.2023
;
;             m s t r <d0t> b l i n k y <at> g m a i l <d0t> c o m
;
; This bootloader will require the boot size fuses to be set  to:
;   BOOTSZ1 = 0 BOOTSZ0 = 1 (2K-byte/1K-word)
;
;  Main features:
;
;  - Bootloader size is under 3K alllowing 1K more space for Applications
;
;  - Built in menu to select and burn programs from external serial flash memory
;
;  - 100% compatible with Arduino IDE / AVRdude uploading
;
;  - No bootloader time out
;
;  - Power on + Button Down launces bootloader instead of programmed sketch
;
;  - A USB icon is displayed in bootloader mode when no serial flash is
;    available or has not been initialized yet (Not displayed when a RTC is used).
;
;  - Identifies itself as serial programmer 'ARDUBOY' with software version 2.1
;
;  - Added command to write to OLED display  (ignored when a RTC is used)
;
;  - Added command to read button states (Not available when a RTC is used)
;
;  - Added command to control LEDs and button input (Not available when a RTC is used)
;
;  - Added command to read and write to serial flash memory
;
;  - Sketch self flashing support through vector at 0x7FFC
;
;  - Software bootloader area protection to protect from accidental overwrites
;
;  - closing com port @ 1200 baud while in bootloader mode will restart bootloader
;    (speeds up Arduino IDE uploading while in bootloader mode)
;
;  - add optional powerdown support
;
;  - add Arduboy mini support
;
;  - add DS3231 / RV 3028 RTC support
;
;  the following obselete commands where removed:
;
;  - Flash Low byte, flash high byte and Page write commands are removed. These
;    commands are not used because flash writing is done using the write block
;    command.
;
;  - Read flash word command. Again it is not used because all flash reading is
;    done using the read block command. A single word can be read as a 2 byte
;    block if required.
;
;  - 'D' Write single EEPROM byte. Command not used. write EEPROM block command
;    is used instead. A single EEPROM byte can be written as a one byte block.
;
;  - 'd' Read single EEPROM byte. Command not used. read EEPROM block command is
;    used instead. A single EEPROM byte can be read as a ne byte block.
;
;  - 'e' Erase Chip. Not needed (and not used) to upload program. This command
;    only erased the program and not the full chip. That can only be done by
;    a ISP programmer.
;
;  the following non mandatory commands are removed (AVRdude works fine without them)
;
;  - 'p' programmer type.
;    Issuing this command will respond with a '?' instead of a 'S'
;
;  - 'a' auto address increment inquiry.
;    Issuing this command will respond with a '?' instead of a 'Y'
;
;  command 't' Request supported device list now returns a null list of devices.

;-------------------------------------------------------------------------------
;
;             LUFA Library
;     Copyright (C) Dean Camera, 2011.
;
;  dean [at] fourwalledcubicle [dot] com
;           www.lufa-lib.org
;
;  Permission to use, copy, modify, distribute, and sell this
;  software and its documentation for any purpose is hereby granted
;  without fee, provided that the above copyright notice appear in
;  all copies and that both that the copyright notice and this
;  permission notice and warranty disclaimer appear in supporting
;  documentation, and that the name of the author not be used in
;  advertising or publicity pertaining to distribution of the
;  software without specific, written prior permission.
;
;  The author disclaim all warranties with regard to this
;  software, including all implied warranties of merchantability
;  and fitness.  In no event shall the author be liable for any
;  special, indirect or consequential damages or any damages
;  whatsoever resulting from loss of use, data or profits, whether
;  in an action of contract, negligence or other tortious action,
;  arising out of or in connection with the use or performance of
;  this software.
;
;===============================================================================
;Adjustable timings

#define TX_RX_LED_PULSE_PERIOD  100     //; uint8 rx/tx pulse period in millisecs

;-------------------------------------------------------------------------------
;Externally supplied defines (commmand line/makefile)

; #define ARDUBOY_DEVKIT    //;configures hardware for official  Arduboy DevKit

; #define ARDUBOY_PROMICRO  //;For Arduboy clones using a Pro Micro 5V using
;                             ;alternate pins for OLED CS, RGB Green and 2nd
;                             ;speaker pin (speaker is not initialized though)

; #define OLED_SH1106       //;for Arduboy clones using SH1106 OLED display only

; #define LCD_ST7565        //;for Arduboy clones using ST7565 LCD displays with
;                           //;RGB backlight and Power LED

; #define CART_CS_SDA       //;When using PORTD1/SDA for flash chip select
;                           //;instead of PORTD2/RX

;#define SUPPORT_POWERDOWN  //;Pressing reset on loader screen will power down the MCU

;#define RUN_APP_ON_POWERON //;Start last flashed game instead of entering bootloader mode

;I2C Real time clocks:

; #define DS3231            //; use Dallas-Maxim DS3231 Real Time Clock

; #define RV3028            //; use Micro Crystal RV-3028-C7 Real Time Clock

;the DEVICE_VID and DEVICE_PID will determine for which board the build will be
;made. (Arduino Leonardo, Arduino Micro, Arduino Esplora, SparkFun ProMicro)

;USB device and vendor IDs
; #define DEVICE_VID                0x2341  //; Arduino LLC
; #define DEVICE_PID                0x0036  //; Leonardo Bootloader

;===============================================================================
;boot magic

#define BOOT_SIGNATURE          0xDCFB  //;LUFA signature for Arduino IDE to use
                                          ;RAMEND-1 to store magic boot key (not applicable for this bootloader)
#define BOOTLOADER_VERSION_MAJOR '2'
#define BOOTLOADER_VERSION_MINOR '1'

#define BOOT_START_ADDR         0x7400
#define BOOT_END_ADDR           0x7FFF

;display resolution
#define WIDTH                   128
#define HEIGHT                  64
#define HSCROLL_STEP            8

;boot logo positioning
#define BOOTLOGO_WIDTH          16
#define BOOTLOGO_HEIGHT         (24 / 8)
#define BOOT_LOGO_X             56
#define BOOT_LOGO_Y             (16 / 8)
#define BOOT_LOGO_OFFSET        BOOT_LOGO_X + BOOT_LOGO_Y * WIDTH

;progress bar positioning
#define PROGRESSBAR_POS         (DisplayBuffer + 3 * WIDTH + 29)
#define PROGRESSBAR_STEPS       64
#define PROGRESSBAR_START       (DisplayBuffer + 3 * WIDTH + 29 + 3)
#define PROGRESSBAR_END         (DisplayBuffer + 3 * WIDTH + 29 + 3 + PROGRESSBAR_STEPS)

;datetime positioning
#define DATE_POS                (DisplayBuffer + 2 * WIDTH + 2)
#define DATE_SEP                7
#define TIME_POS                (DisplayBuffer + 2 * WIDTH + 87)
#define TIME_SEP                3
#define SEC_POS                 (DisplayBuffer + 3 * WIDTH + 118)

;RTC
#if defined (DS3231)
 #define SLA_W                  (0x68) << 1 | 0
 #define SLA_R                  (0x68) << 1 | 1
 #define SLD_LEN                7
#elif defined (RV3028)
 #define SLA_W                  (0xA4)
 #define SLA_R                  (0xA5)
 #define SLD_LEN                17
#endif

;OLED display commands
#define OLED_SET_PAGE_ADDR          0xB0
#if defined OLED_SH1106
  #define OLED_SET_COLUMN_ADDR_LO   0x02
#else
  #define OLED_SET_COLUMN_ADDR_LO   0x00
#endif
#define OLED_SET_COLUMN_ADDR_HI     0x10
#define OLED_SET_DISPLAY_ON         0xAF
#define OLED_SET_DISPLAY_OFF        0xAE

;SPI serial flash
#ifdef CART_CS_SDA
#define CART_CS                     1
#else
#define CART_CS                     2
#endif
#define SFC_PAGE_PROGRAM            0x02
#define SFC_READ_DATA               0x03
#define SFC_READ_STATUS1            0x05
#define SFC_WRITE_ENABLE            0x06
#define SFC_SECTOR_ERASE            0x20
#define SFC_32K_ERASE               0x52
#define SFC_64K_ERASE               0xD8
#define SFC_JEDEC_ID                0x9F
#define SFC_RELEASE_POWERDOWN       0xAB
#define SFC_POWERDOWN               0xB9

;-------------------------------------------------------------------------------
;MCU related
;-------------------------------------------------------------------------------

;atmega32u4 signature

#define AVR_SIGNATURE_1         0x1E
#define AVR_SIGNATURE_2         0x95
#define AVR_SIGNATURE_3         0x87

#define APPLICATION_START_ADDR  0x0000
#define SPM_PAGESIZE            0x0080

#define RAMEND                  0x0AFF

;register ports (accessed through ld/st instructions)

#define UEBCHX  0x00F3
#define UEBCLX  0x00F2
#define UEDATX  0x00F1

#define UESTA0X 0x00EE
#define UECFG1X 0x00ED
#define UECFG0X 0x00EC
#define UECONX  0x00EB
#define UERST   0x00EA
#define UENUM   0x00E9
#define UEINTX  0x00E8

#define UDADDR  0x00E3
#define UDIEN   0x00E2
#define UDINT   0x00E1
#define UDCON   0x00E0

#define USBINT  0x00DA
#define USBSTA  0x00D9
#define USBCON  0x00D8
#define UHWCON  0x00D7

#define TWBR    0x00B8
#define TWSR    0x00B9
#define TWDR    0x00BB
#define TWCR    0x00BC

#define OCR1AH  0x0089
#define OCR1AL  0x0088

#define TCNT1H  0x0085
#define TCNT1L  0x0084

#define TCCR1B  0x0081

#define TIMSK1  0x006F

#define PRR1    0x0065
#define PRR0    0x0064

#define CLKPR   0x0061

#define WDTCSR  0x0060


;io ports (accessed through in/out instructions)

#define SREG    0x3f
#define SPH     0x3e
#define SPL     0x3d

#define SPMCSR  0x37
#define MCUCR   0x35
#define MCUSR   0x34
#define SMCR    0x33
#define PLLFRQ  0x32

#define ACSR    0x30

#define SPDR    0x2E
#define SPSR    0x2D
#define SPCR    0x2C
#define GPIOR2  0x2B
#define GPIOR1  0x2A
#define PLLCSR  0x29

#define EEARH   0x22
#define EEARL   0x21
#define EEDR    0x20
#define EECR    0x1F
#define GPIOR0  0x1E

#define PORTF   0x11
#define DDRF    0x10
#define PINF    0x0f
#define PORTE   0x0e
#define DDRE    0x0d
#define PINE    0x0c
#define PORTD   0x0b
#define DDRD    0x0a
#define PIND    0x09
#define PORTC   0x08
#define DDRC    0x07
#define PINC    0x06
#define PORTB   0x05
#define DDRB    0x04
#define PINB    0x03

#define I2C_SCL 0
#define I2C_SDA 1

;-------------------------------------------------------------------------------
;bit values
;-------------------------------------------------------------------------------

;UEINTX
#define TXINI    0
#define STALLEDI 1
#define RXOUTI   2
#define RXSTPI   3
#define NAKOUTI  4
#define RWAL     5
#define NAKINI   6
#define FIFOCON  7

;UDIEN
#define SUSPE   0
#define SOFE    2
#define EORSTE  3
#define WAKEUPE 4
#define EORSME  5
#define UPRSME  6

;UDINT
#define SUSPI   0
#define SOFI    2
#define EORSTI  3
#define WAKEUPI 4
#define EORSMI  5
#define UPRSMI  6

;UDCON
#define DETACH  0
#define RMWKUP  1
#define LSM     2
#define RSTCPU  3

;USBINT
#define VBUSTI  0

;USBSTA
#define VBUS    0
#define SPEED   3

;USBCON
#define VBUSTE  0
#define OTGPADE 4
#define FRZCLK  5
#define USBE    7

;TWCR
#define TWEN    2
#define TWSTO   4
#define TWSTA   5
#define TWEA    6
#define TWINT   7

;UHWCON
#define UVREGE  0

;PRR1
#define PRUSB    7
#define PRTIM4   4
#define PRTIM3   3
#define PRUSART1 0

;PRR0
#define PRTWI    7
#define PRTIM0   5
#define PRTIM1   3
#define PRSPI    2
#define PRADC    0

;CLKPR
#define CLKPCE  7

;SPMCSR
#define SPMEN   0
#define PGERS   1
#define PGWRT   2
#define BLBSET  3
#define RWWSRE  4
#define SIGRD   5
#define RWWSB   6
#define SPMIE   7

;MCUCR
#define JTD     7
#define PUD     4
#define IVSEL   1
#define IVCE    0

;MCUSR
#define USBRF   5
#define JTRF    4
#define WDRF    3
#define BORF    2
#define EXTRF   1
#define PORF    0

;SMCR
#define SM2     3
#define SM1     2
#define SM0     1
#define SE      0

;ACSR
#define ACD     7
#define ACBG    6
#define ACO     5
#define ACI     4
#define ACIE    3
#define ACIC    2
#define ACIS1   1
#define ACIS0   0

;SPSR
#define SPIF    7
#define WCOL    6
#define SPI2X   0

;SPCR
#define SPIE    7
#define SPE     6
#define DORD    5
#define MSTR    4
#define CPOL    3
#define CPHA    2
#define SPR1    1
#define SPR0    0

;EECR
#define EEPM1   5
#define EEPM0   4
#define EERIE   3
#define EEMPE   2
#define EEPE    1
#define EERE    0


;LED defines
#ifdef ARDUBOY_PROMICRO
#ifndef CART_CS_SDA
 #define OLED_RST        1
 #define OLED_CS         3
#endif
 #define OLED_DC         4
 #define RGB_R           6
 #define RGB_G           0
 #define RGB_B           5
 #ifdef LCD_ST7565
  #define RGB_RED_ON      sbi     PORTB, RGB_R
  #define RGB_GREEN_ON    sbi     PORTD, RGB_G
  #define RGB_BLUE_ON     sbi     PORTB, RGB_B
  #define RGB_RED_OFF     cbi     PORTB, RGB_R
  #define RGB_GREEN_OFF   cbi     PORTD, RGB_G
  #define RGB_BLUE_OFF    cbi     PORTB, RGB_B
 #else
  #define RGB_RED_ON      cbi     PORTB, RGB_R
  #define RGB_GREEN_ON    cbi     PORTD, RGB_G
  #define RGB_BLUE_ON     cbi     PORTB, RGB_B
  #define RGB_RED_OFF     sbi     PORTB, RGB_R
  #define RGB_GREEN_OFF   sbi     PORTD, RGB_G
  #define RGB_BLUE_OFF    sbi     PORTB, RGB_B
 #endif
#else
 #ifdef ARDUBOY_DEVKIT
  #define OLED_RST        6
  #define OLED_CS         7
  #define OLED_DC         4
  #define RGB_R           6
  #define RGB_G           7
  #define RGB_B           0
  #define RGB_RED_ON      cbi     PORTB, RGB_B
  #define RGB_GREEN_ON    cbi     PORTB, RGB_B
  #define RGB_BLUE_ON     cbi     PORTB, RGB_B
  #define RGB_RED_OFF     sbi     PORTB, RGB_B
  #define RGB_GREEN_OFF   sbi     PORTB, RGB_B
  #define RGB_BLUE_OFF    sbi     PORTB, RGB_B
 #else
  #define OLED_RST        7
  #define OLED_CS         6
  #define OLED_DC         4
  #define RGB_R           6
  #define RGB_G           7
  #define RGB_B           5
  #ifdef LCD_ST7565
   #define POWER_LED       0
   #define RGB_RED_ON      sbi     PORTB, RGB_R
   #define RGB_GREEN_ON    sbi     PORTB, RGB_G
   #define RGB_BLUE_ON     sbi     PORTB, RGB_B
   #define RGB_RED_OFF     cbi     PORTB, RGB_R
   #define RGB_GREEN_OFF   cbi     PORTB, RGB_G
   #define RGB_BLUE_OFF    cbi     PORTB, RGB_B
  #else
   #define RGB_RED_ON      cbi     PORTB, RGB_R
   #define RGB_GREEN_ON    cbi     PORTB, RGB_G
   #define RGB_BLUE_ON     cbi     PORTB, RGB_B
   #define RGB_RED_OFF     sbi     PORTB, RGB_R
   #define RGB_GREEN_OFF   sbi     PORTB, RGB_G
   #define RGB_BLUE_OFF    sbi     PORTB, RGB_B
  #endif
 #endif
#endif

;button defines
#ifdef ARDUBOY_DEVKIT
 #define BTN_UP_BIT        4
 #define BTN_UP_PIN        PINB
 #define BTN_UP_DDR        DDRB
 #define BTN_UP_PORT       PORTB

 #define BTN_RIGHT_BIT     6
 #define BTN_RIGHT_PIN     PINC
 #define BTN_RIGHT_DDR     DDRC
 #define BTN_RIGHT_PORT    PORTC

 #define BTN_LEFT_BIT      5
 #define BTN_LEFT_PIN      PINB
 #define BTN_LEFT_DDR      DDRB
 #define BTN_LEFT_PORT     PORTB

 #define BTN_DOWN_BIT      6
 #define BTN_DOWN_PIN      PINB
 #define BTN_DOWN_DDR      DDRB
 #define BTN_DOWN_PORT     PORTB

 #define BTN_A_BIT         7
 #define BTN_A_PIN         PINF
 #define BTN_A_DDR         DDRF
 #define BTN_A_PORT        PORTF

 #define BTN_B_BIT         6
 #define BTN_B_PIN         PINF
 #define BTN_B_DDR         DDRF
 #define BTN_B_PORT        PORTF

 #define LEFT_BUTTON       5
 #define RIGHT_BUTTON      2
 #define UP_BUTTON         4
 #define DOWN_BUTTON       6
 #define A_BUTTON          1
 #define B_BUTTON          0
 #define AB_BUTTON         0
#else
#if defined (MICROCADE)
 #define BTN_UP_BIT        4
 #define BTN_UP_PIN        PINF
 #define BTN_UP_DDR        DDRF
 #define BTN_UP_PORT       PORTF

 #define BTN_RIGHT_BIT     5
 #define BTN_RIGHT_PIN     PINF
 #define BTN_RIGHT_DDR     DDRF
 #define BTN_RIGHT_PORT    PORTF

 #define BTN_LEFT_BIT      6
 #define BTN_LEFT_PIN      PINF
 #define BTN_LEFT_DDR      DDRF
 #define BTN_LEFT_PORT     PORTF

 #define BTN_DOWN_BIT      7
 #define BTN_DOWN_PIN      PINF
 #define BTN_DOWN_DDR      DDRF
 #define BTN_DOWN_PORT     PORTF
#else
 #define BTN_UP_BIT        7
 #define BTN_UP_PIN        PINF
 #define BTN_UP_DDR        DDRF
 #define BTN_UP_PORT       PORTF

 #define BTN_RIGHT_BIT     6
 #define BTN_RIGHT_PIN     PINF
 #define BTN_RIGHT_DDR     DDRF
 #define BTN_RIGHT_PORT    PORTF

 #define BTN_LEFT_BIT      5
 #define BTN_LEFT_PIN      PINF
 #define BTN_LEFT_DDR      DDRF
 #define BTN_LEFT_PORT     PORTF

 #define BTN_DOWN_BIT      4
 #define BTN_DOWN_PIN      PINF
 #define BTN_DOWN_DDR      DDRF
 #define BTN_DOWN_PORT     PORTF
#endif
 #define BTN_A_BIT         6
 #define BTN_A_PIN         PINE
 #define BTN_A_DDR         DDRE
 #define BTN_A_PORT        PORTE

 #define BTN_B_BIT         4
 #define BTN_B_PIN         PINB
 #define BTN_B_DDR         DDRB
 #define BTN_B_PORT        PORTB
#if defined (MICROCADE)
 #define LEFT_BUTTON       6
 #define RIGHT_BUTTON      5
 #define UP_BUTTON         4
 #define DOWN_BUTTON       7
#else
 #define LEFT_BUTTON       5
 #define RIGHT_BUTTON      6
 #define UP_BUTTON         7
 #define DOWN_BUTTON       4
#endif
 #define A_BUTTON          3
 #define B_BUTTON          2
 #define AB_BUTTON         2
#endif

;LED Control bits
#define LED_CTRL_NOBUTTONS  7
#define LED_CTRL_RGB        6
#define LED_CTRL_RXTX       5
#define LED_CTRL_RX_ON      4
#define LED_CTRL_TX_ON      3
#define LED_CTRL_RGB_R_ON   1
#define LED_CTRL_RGB_G_ON   2
#define LED_CTRL_RGB_B_ON   0

;other LEDs
#define LLED            7
#define LLED_ON         sbi     PORTC, LLED
#define LLED_OFF        cbi     PORTC, LLED

#define RX_LED          0
#define TX_LED          5

#if DEVICE_PID == 0x0037        //; Polarity of the RX and TX LEDs is reversed on the Micro
    #define TX_LED_OFF          cbi  PORTD, TX_LED
    #define TX_LED_ON           sbi  PORTD, TX_LED

    #define RX_LED_OFF          cbi PORTB, RX_LED
    #define RX_LED_ON           sbi PORTB, RX_LED
#else
    #define TX_LED_OFF          sbi  PORTD, TX_LED
    #define TX_LED_ON           cbi  PORTD, TX_LED

    #define RX_LED_OFF          sbi PORTB, RX_LED
    #define RX_LED_ON           cbi PORTB, RX_LED
#endif

;-------------------------------------------------------------------------------
;USB
;-------------------------------------------------------------------------------

;size of structures

#define sizeof_USB_Descriptor_Header_t  2
#define sizeof_DeviceDescriptor         sizeof_USB_Descriptor_Header_t + (8 << 1)
#define sizeof_LanguageString           sizeof_USB_Descriptor_Header_t + (1 << 1)
#define sizeof_ProductString            sizeof_USB_Descriptor_Header_t + (16 << 1)
#define sizeof_ManufacturerString       sizeof_USB_Descriptor_Header_t + (11 << 1)
#define sizeof_ConfigurationDescriptor  62
#define sizeof_USB_ControlRequest_t     8
#define sizeof_LineEncoding             7

;USB_DescriptorTypes:

#define DTYPE_Device                    0x01    ;Indicates that the descriptor is a device descriptor.
#define DTYPE_Configuration             0x02    ;Indicates that the descriptor is a configuration descriptor.
#define DTYPE_String                    0x03    ;Indicates that the descriptor is a string descriptor.
#define DTYPE_Interface                 0x04    ;Indicates that the descriptor is an interface descriptor.
#define DTYPE_Endpoint                  0x05    ;Indicates that the descriptor is an endpoint descriptor.
#define DTYPE_DeviceQualifier           0x06    ;Indicates that the descriptor is a device qualifier descriptor.
#define DTYPE_Other                     0x07    ;Indicates that the descriptor is of other type.
#define DTYPE_InterfacePower            0x08    ;Indicates that the descriptor is an interface power descriptor.
#define DTYPE_InterfaceAssociation      0x0B    ;Indicates that the descriptor is an interface association descriptor.
#define DTYPE_CSInterface               0x24    ;Indicates that the descriptor is a class specific interface descriptor. *
#define DTYPE_CSEndpoint                0x25    ;Indicates that the descriptor is a class specific endpoint descriptor.

;CDC_Descriptor_ClassSubclassProtocol

#define CDC_CSCP_CDCClass               0x02    ;Descriptor Class value indicating that the device or interface belongs to the CDC class.
#define CDC_CSCP_NoSpecificSubclass     0x00    ;Descriptor Subclass value indicating that the device or interfacebelongs to no specific subclass of the CDC class.
#define CDC_CSCP_ACMSubclass            0x02    ;Descriptor Subclass value indicating that the device or interface belongs to the Abstract Control Model CDC subclass.
#define CDC_CSCP_ATCommandProtocol      0x01    ;Descriptor Protocol value indicating that the device or interface belongs to the AT Command protocol of the CDC class.
#define CDC_CSCP_NoSpecificProtocol     0x00    ;Descriptor Protocol value indicating that the device or interface belongs to no specific protocol of the CDC class.
#define CDC_CSCP_VendorSpecificProtocol 0xFF    ;Descriptor Protocol value indicating that the device or interface belongs to a vendor-specific protocol of the CDC class.
#define CDC_CSCP_CDCDataClass           0x0A    ;Descriptor Class value indicating that the device or interface belongs to the CDC Data class.
#define CDC_CSCP_NoDataSubclass         0x00    ;Descriptor Subclass value indicating that the device or interface belongs to no specific subclass of the CDC data class.
#define CDC_CSCP_NoDataProtocol         0x00    ;Descriptor Protocol value indicating that the device or interface belongs to no specific protocol of the CDC data class.

;CDC_ClassRequests

#define CDC_REQ_SendEncapsulatedCommand 0x00    ;CDC class-specific request to send an encapsulated command to the device.
#define CDC_REQ_GetEncapsulatedResponse 0x01    ;CDC class-specific request to retrieve an encapsulated command response from the device.
#define CDC_REQ_SetLineEncoding         0x20    ;CDC class-specific request to set the current virtual serial port configuration settings.
#define CDC_REQ_GetLineEncoding         0x21    ;CDC class-specific request to get the current virtual serial port configuration settings.
#define CDC_REQ_SetControlLineState     0x22    ;CDC class-specific request to set the current virtual serial port handshake line states.
#define CDC_REQ_SendBreak               0x23    ;CDC class-specific request to send a break to the receiver via the carrier channel.

#define LANGUAGE_ID_ENG                 0x0409

;-------------------------------------------------------------------------------
;USB strings

#if DEVICE_PID == 0x0036
#define PRODUCT_STRING          'A','r','d','u','i','n','o',' ','L','e','o','n','a','r','d','o'
#elif DEVICE_PID == 0x0037
#define PRODUCT_STRING          'A','r','d','u','i','n','o',' ','M','i','c','r','o',' ',' ',' '
#elif DEVICE_PID == 0x003C
#define PRODUCT_STRING          'A','r','d','u','i','n','o',' ','E','s','p','l','o','r','a',' '
#elif DEVICE_PID == 0x9205
#define PRODUCT_STRING          'P','r','o',' ','M','i','c','r','o',' ','5','V',' ',' ',' ',' '
#else
#define PRODUCT_STRING          'U','S','B',' ','I','O',' ','b','o','a','r','d',' ',' ',' ',' '
#endif

#if DEVICE_VID == 0x2341
#define MANUFACTURER_STRING     'A','r','d','u','i','n','o',' ','L','L','C'
#elif DEVICE_VID == 0x1B4F
#define MANUFACTURER_STRING     'S','p','a','r','k','F','u','n',' ',' ',' '
#else
#define MANUFACTURER_STRING     'U','n','k','n','o','w','n',' ',' ',' ',' '
#endif
;-------------------------------------------------------------------------------
                            .section .data  ;Initalized data copied to ram
;-------------------------------------------------------------------------------

;Note: this data section must be <= 256 bytes

;- DeviceDiscriptor structure -

DeviceDescriptor:           ;-header-
                            .byte   sizeof_DeviceDescriptor
                            .byte   DTYPE_Device
                            ;-data-
                            .word   0x0110      ;USB specification version = 01.10
                            .byte   0x02, 0x00  ;
                            .byte   0x00, 0x08  ;
                            .word   DEVICE_VID  ;
                            .word   DEVICE_PID  ;
                            .word   0x0001      ;version                = 00.01
                          #if defined (USE_MANUFACTURE_STRING)
                            .byte   0x02        ;ManufacturerStrIndex   = 2
                          #else
                            .byte   0x00        ;ManufacturerStrIndex   = NO_DISCRIPTOR
                          #endif
                          #if defined (USE_PRODUCT_STRING)
                            .byte   0x01        ;ProductStrIndex        = 1
                          #else
                            .byte   0x00        ;ProductStrIndex        = NO_DISCRIPTOR
                          #endif
                            .byte   0x00        ;SerialNumStrIndex      = NO_DESCRIPTOR
                            .byte   0x01        ;NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS

;- ConfigurationDescriptor structure -

ConfigurationDescriptor:    ;-config.header -
                            .byte   0x09        ;sizeof(USB_Descriptor_Configuration_Header_t)
                            .byte   DTYPE_Configuration
                            ;-config.data -
                            .byte   0x3e, 0x00  ;TotalConfigurationSize = sizeof(USB_Descriptor_Configuration_t)
                            .byte   0x02        ;TotalInterfaces = 2
                            .byte   0x01        ;ConfigurationNumber    = 1
                            .byte   0x00        ;ConfigurationStrIndex  = NO_DESCRIPTOR
                            .byte   0x80        ;ConfigAttributes       = USB_CONFIG_ATTR_BUSPOWERED
                            .byte   0x32        ;MaxPowerConsumption    = USB_CONFIG_POWER_MA(100)
                            ;-CDC_CCI_Interface.header-
                            .byte   0x09        ;sizeof(USB_Descriptor_Interface_t)
                            .byte   DTYPE_Interface
                            ;-CDC_CCI_Interface.data-
                            .byte   0x00        ;InterfaceNumber   = 0
                            .byte   0x00        ;AlternateSetting  = 0
                            .byte   0x01        ;TotalEndpoints    = 1
                            .byte   0x02        ;Class             = CDC_CSCP_CDCClass
                            .byte   0x02        ;SubClass          = CDC_CSCP_ACMSubclass
                            .byte   0x01        ;Protocol          = CDC_CSCP_ATCommandProtocol
                            .byte   0x00        ;InterfaceStrIndex = NO_DESCRIPTOR
                            ;CDC_Functional_Header.header
                            .byte   0x05        ;sizeof(USB_CDC_Descriptor_FunctionalHeader_t)
                            .byte   DTYPE_CSInterface
                            ;CDC_Functional_Header.data
                            .byte   0x00        ;Subtype = 0x00
                            .word   0x0110      ;CDCSpecification = VERSION_BCD(01.10)
                            ;.header
                            .byte   0x04
                            .byte   DTYPE_CSInterface
                            ;.data
                            .byte   0x02        ;Subtype = 0x02
                            .byte   0x04        ;d0      =
                            ;.header
                            .byte   0x05
                            .byte   DTYPE_CSInterface
                            ;.data
                            .byte   0x06        ;Subtype = 0x06 (control interface)
                            .byte   0x00        ;d0      =
                            .byte   0x01        ;d1      =
                            ;.header
                            .byte   0x07
                            .byte   DTYPE_Endpoint
                            ;.data
                            .byte   0x82        ;EndpointAddress   = ENDPOINT_DIR_IN | CDC_NOTIFICATION_EPNUM
                            .byte   0x03        ;Attributes        = EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC
                            .word   0x0008      ;EndpointSize      = CDC_NOTIFICATION_EPSIZE
                            .byte   0xff        ;PollingIntervalMS = 0xFF
                            ;.header
                            .byte   0x09
                            .byte   DTYPE_Interface
                            ;.data
                            .byte   0x01        ;InterfaceNumber   = 1
                            .byte   0x00        ;AlternateSetting  = 0
                            .byte   0x02        ;TotalEndpoints    = 2
                            .byte   0x0a        ;Class             = data
                            .byte   0x00        ;SubClass          =
                            .byte   0x00        ;Protocol          =
                            .byte   0x00        ;InterfaceStrIndex = NO_DISCRIPTOR
                            ;.header
                            .byte   0x07
                            .byte   DTYPE_Endpoint
                            ;.data
                            .byte   0x04        ;EndpointAddress   =
                            .byte   0x02        ;Attributes        =
                            .word   0x0010      ;EndpointSize      =
                            .byte   0x01        ;PollingIntervalMS =
                            ;.header
                            .byte   0x07
                            .byte   DTYPE_Endpoint
                            ;.data
                            .byte   0x83        ;EndpointAddress   =
                            .byte   0x02        ;Attributes        =
                            .word   0x0010      ;EndpointSize      =
                            .byte   0x01        ;PollingIntervalMS =

;UTF-8 strings
                          #if defined (USE_LANGUAGE_STRING)
LanguageString:             ;-header-
                            .byte   sizeof_LanguageString   ;USB_Descriptor_Header.Size
                            .byte   DTYPE_String            ;USB_Descriptor_Header.Type
                            ;-data-
                            .word   LANGUAGE_ID_ENG
                          #endif

                          #ifdef USE_PRODUCT_STRING
ProductString:              ;-header-
                            .byte   sizeof_ProductString    ;USB_Descriptor_Header.Size = 2 + (16 unicode chars) << 1
                            .byte   DTYPE_String            ;USB_Descriptor_Header.Type = DTYPE_String
                            ;-data-
                            .word   PRODUCT_STRING
                          #endif

                          #ifdef USE_MANUFACTURE_STRING
ManufacturerString:         ;-header-
                            .byte   sizeof_ManufacturerString
                            .byte   DTYPE_String
                            ;-data-
                            .word   MANUFACTURER_STRING
                          #endif

;-------------------------------------------------------------------------------
;Bootloader area
;-------------------------------------------------------------------------------

                            .section .boot, "ax" ;

;special register usage:
;   r0      temp reg
;   r1      zero reg
;   r2      frame counter
;   r3      bootloader timeout counter
;   r4, r5  Current Address
;   r6, r7  Current application page
;   r8      Current list
;   r9      buttons state
;   r10     replacement for GPIOR0

;-------------------------------------------------------------------------------
;7800 Reset Vector (do not change code size)
;-------------------------------------------------------------------------------

VECTOR_00_7800:             clr     r1                  ;global zero reg
                            out     SREG, r1            ;clear SREG
                            in      r16, MCUSR          ;save MCUSR state
                            out     MCUSR, r1           ;clear MCUSR
                            ldi     r24, 0x18           ;we want watch dog disabled asap
                            sts     WDTCSR, r24
                            sts     WDTCSR, r1

                            ;clear stack

                            ldi     r24, lo8(RAMEND-1)  ;-1 for power down flag
                            ldi     r25, hi8(RAMEND-1)
                            out     SPH, r25            ;SP = RAMEND-1
                            out     SPL, r24

                            ;clear vars stored in registers

                            clr     r2                  ;reset frame counter
                            clr     r3                  ;reset timeout counter
                            movw    r4, r2              ;clear address
                            movw    r6, r2              ;reset applicaton pointer
                            movw    r8, r2              ;clear list, buttons state
                            ldi     r24, 1 << CLKPCE    ;CLKPR value
                            rjmp    reset_b0

;-------------------------------------------------------------------------------
;7828 General USB vector (do not change code size)
;-------------------------------------------------------------------------------

VECTOR_10_7828:             push    r0
                            push    r24
                            push    r25
                            push    r1
                            in      r0, SREG
                            push    r0
                            eor     r1, r1
                            push    r20
                            push    r22
                            lds     r24, USBINT
                            sbrs    r24, VBUSTI
                            rjmp    USB_general_int_b3
                            rjmp    USB_general_int_b1

;-------------------------------------------------------------------------------
;7844 Timer 1 comparator A vector (do not change code size)
;-------------------------------------------------------------------------------

VECTOR_17_7844:
TIMER1_COMPA_interrupt:     push    r0
                            in      r0, SREG                    ;save SREG
                            push    r24
                            push    r25
                            push    r30
                            push    r31
                            eor     r24, r24                    ;use as temp zero reg
                            sts     TCNT1H, r24                 ;reset counter
                            sts     TCNT1L, r24
                            ldi     r30, lo8(IndexedVars)
                            ldi     r31, hi8(IndexedVars)
                           #if !(defined (DS3231) || defined (RV3028))
                            ldd     r25, z+IDX_LEDCONTROL
                            andi    r25, 1 << LED_CTRL_RXTX
                            brne    TIMER1_COMPA_interrupt_b2   ;don't update RxTx LEDs
                           #endif
                            ldd     r25, z+IDX_TXLEDPULSE
                            cp      r24, r25                    ;sets carry if r25 > 0
                            sbc     r25, r24                    ;r25 -0 - carry
                            std     z+IDX_TXLEDPULSE, r25
                            brne    TIMER1_COMPA_interrupt_b1

                            TX_LED_OFF
TIMER1_COMPA_interrupt_b1:
                            ldd     r25, z+IDX_RXLEDPULSE
                            cp      r24, r25                    ;again sets carry if r25 > 0
                            sbc     r25, r24                    ;r25 - 0 - carry
                            std     z+IDX_RXLEDPULSE, r25
                            brne    TIMER1_COMPA_interrupt_b2

                            RX_LED_OFF
TIMER1_COMPA_interrupt_b2:
                            inc     r2                      ;frame counter
                            brne    TIMER1_COMPA_int_end    ;no overflow

                            cp      r24, r3                 ;test timeout activated (sets carry)
                            adc     r3, r24                 ;increase timeout if so
TIMER1_COMPA_int_end:
                            pop     r31
                            pop     r30
                            rjmp    shared_reti

;-------------------------------------------------------------------------------
USB_general_int_b1:
                            lds     r24, USBCON
                            sbrs    r24, VBUSTE
                            rjmp    USB_general_int_b3

                            ;VBUS transition interrupt (connect/disconnect)

                            lds     r24, USBINT
                            andi    r24, ~(1 << VBUSTI)     ;clear VBUSTI interupt
                            sts     USBINT, r24
                            lds     r24, USBSTA
                            sbrs    r24, VBUS
                            rjmp    USB_general_int_b2

                            ;device connected (voltage on VBUS > 1.4V)

                            rcall   pll_enable
                            ldi     r24, 0x01
                            mov     r10, r24                ;GPIOR0
                            rjmp    USB_general_int_b3

USB_general_int_b2:         ;device disconnected

                            out     PLLCSR, r1
                            clr     r10                     ;GPIOR0
USB_general_int_b3:
                            lds     r24, UDINT
                            sbrs    r24, 0
                            rjmp    USB_general_int_b4

                            rcall   UDIEN_get
                            sbrs    r24, 0
                            rjmp    USB_general_int_b4

                            rcall   UDIEN_Clr0_Set4
                            lds     r24, USBCON
                            ori     r24, 0x20
                            rcall   USBCON_set
                            out     PLLCSR, r1
                            ldi     r24, 0x05
                            mov     r10, r24                ;GPIOR0
USB_general_int_b4:
                            lds     r24, UDINT
                            sbrs    r24, 4
                            rjmp    USB_general_int_b8

                            rcall   UDIEN_get
                            sbrs    r24, 4
                            rjmp    USB_general_int_b8
                            rcall   pll_enable
                            lds     r24, USBCON
                            andi    r24, 0xDF
                            rcall   USBCON_set
                            ldi     r24, 0xEF
                            rcall   UDINT_clr_bit
                            rcall   UDIEN_get
                            andi    r24, 0xEF
                            ;rcall   UDIEN_set
                            ori     r24, 0x01
                            rcall   UDIEN_set

                            lds     r24, USB_Device_ConfigurationNumber
                            and     r24, r24
                            brne    USB_general_int_b6

                            lds     r24, UDADDR
                            sbrc    r24, 7
                            rjmp    USB_general_int_b6

                            ldi     r24, 0x01
                            rjmp    USB_general_int_b7
USB_general_int_b6:
                            ldi     r24, 0x04               ;CDC_RX_EPNUM
USB_general_int_b7:
                            mov     r10, r24                ;GPIOR0
USB_general_int_b8:
                            lds     r24, UDINT
                            sbrs    r24, 3
                            rjmp    USB_general_int_ret

                            rcall   UDIEN_get
                            sbrs    r24, 3
                            rjmp    USB_general_int_ret

                            ldi     r24, 0xF7
                            rcall   UDINT_clr_bit
                            ldi     r24, 0x02
                            mov     r10, r24                ;GPIOR0
                            sts     USB_Device_ConfigurationNumber, r1
                            rcall   UDINT_clr_bit0
                            rcall   UDIEN_Clr0_Set4
                            rcall   Endpoint_ConfigureEndpoint_Prv_00_00_02  ;uses: r20,r22,r24,r25
USB_general_int_ret:
                            pop     r22
                            pop     r20
                            pop     r0
                            pop     r1
shared_reti:
                            pop     r25
                            pop     r24
                            out     SREG, r0
                            pop     r0
                            reti

;-------------------------------------------------------------------------------
reset_b0:
                            ;setup hardware

                            sts     CLKPR, r24              ;enable CLK prescaler change
                            sts     CLKPR, r1               ;PCLK/1
                            ldi     r27, (1 << IVCE)        ;== 0x01 enable interrupt vector select
                            out     MCUCR, r27
                            ldi     r24, 1 << IVSEL         ;select bootloader vectors
                            out     MCUCR, r24
                        #ifdef ARDUBOY_DEVKIT
                            ldi     r24, 0x07               ;SPI_CLK, MOSI, RXLED as outputs
                            out     DDRB, r24
                            ldi     r24, 0x71               ;Pull-ups on UP,LEFT,DOWN Buttons, RXLED off
                            out     PORTB, r24
                            out     DDRC, r1                ;all inputs
                            sbi     PORTC, BTN_RIGHT_BIT    ;pull-up on right button
                            out     DDRF, r1                ;Set all as inputs
                            ldi     r24, 0xC0               ;pullups on button A and B
                            out     PORTF, r24
                        #else
                          #if DEVICE_PID == 0x0037
                            ldi     r24, 0xF0               ;RGBLED OFF | PULLUP B-Button | RXLED OFF (Arduino micro)
                          #elif defined (MICROCADE)
                            ldi     r24, 0x91               ;RGBLED BLUE+RED ON | PULLUP B-Button | RXLED OFF
                          #else
                            ldi     r24, 0xF1               ;RGBLED OFF | PULLUP B-Button | RXLED OFF
                          #endif
                            out     PORTB, r24
                            ldi     r24, 0xE7               ;RGBLED, SPI_CLK, MOSI, RXLED as outputs
                            out     DDRB, r24
                        #if defined (ARDUBIGBOY) || (ARDUBOYMINI)
                            ldi     r24, (1 << CART_CS)     ; Flash cart as output
                            out     DDRE, r24
                            ldi     r24, (1 << BTN_A_BIT) | (1 << CART_CS) ; Enable pullup for A button, Flash cart inactive high
                            out     PORTE, r24
                        #else
                            out     DDRE, r1                ;all as inputs
                            sbi     PORTE, BTN_A_BIT        ;enable pullup for A button
                        #endif
                            out     DDRF, r1                ;all as inputs
                            ldi     r24, 0xF3               ;pullups on D-PAD and unused inputs
                            out     PORTF, r24
                        #endif

                            ;setup display io and reset

                        #if defined (ARDUBOY_PROMICRO)
                          #ifdef CART_CS_SDA
                            ldi     r24, (1 << TX_LED) | (1 << RGB_G) | (1 << CART_CS) ;Command mode, Tx LED off, RGB green off, Flash cart inactive high
                            out     PORTD, r24
                            ldi     r24, (1 << OLED_DC) | (1 << TX_LED) | (1 << RGB_G) | (1 << CART_CS) ; as outputs
                            out     DDRD, r24
                          #else
                           #if defined (MICROCADE)
                            ldi     r24, (0 << OLED_RST) | (1 << OLED_CS) | (0 << OLED_DC) | (1 << TX_LED) | (0 << RGB_G) | (1 << CART_CS) ;RST active low, CS inactive high, Command mode, Tx LED off, RGB green on, Flash cart inactive high
                           #else
                            ldi     r24, (0 << OLED_RST) | (1 << OLED_CS) | (0 << OLED_DC) | (1 << TX_LED) | (1 << RGB_G) | (1 << CART_CS) ;RST active low, CS inactive high, Command mode, Tx LED off, RGB green off, Flash cart inactive high
                           #endif
                            out     PORTD, r24
                            ldi     r24, (1 << OLED_RST) | (1 << OLED_CS) | (1 << OLED_DC) | (1 << TX_LED) | (1 << RGB_G) | (1 << CART_CS) ; as outputs
                            out     DDRD, r24
                          #endif
                        #elif defined (LCD_ST7565)
                            ldi     r24, (1 << OLED_CS) | (1 << TX_LED) | (1 << CART_CS) ;RST active low, CS inactive high, Command mode, Tx LED off, Flash cart inactive high. Power LED active low
                            out     PORTD, r24
                            ldi     r24, (1 << OLED_RST) | (1 << OLED_CS) | (1 << OLED_DC) | (1 << TX_LED) | (1 << CART_CS) | (1 << POWER_LED); as outputs
                            out     DDRD, r24
                        #elif defined (ARDUBIGBOY) || (ARDUBOYMINI)
                            ldi     r24, (1 << OLED_CS) | (1 << TX_LED); RST active low, CS inactive high, Command mode, Tx LED off
                            out     PORTD, r24
                            ldi     r24, (1 << OLED_RST) | (1 << OLED_CS) | (1 << OLED_DC) | (1 << TX_LED); as outputs
                            out     DDRD, r24
                        #else
                           #if !(defined (DS3231) || defined (RV3028))
                            ldi     r24, (0 << OLED_RST) | (1 << OLED_CS) | (0 << OLED_DC) | (1 << TX_LED) | (1 << CART_CS) ;RST active low, CS inactive high, Command mode, Tx LED off, Flash cart inactive high
                           #else
                            ldi     r24, (0 << OLED_RST) | (1 << OLED_CS) | (0 << OLED_DC) | (1 << TX_LED) | (1 << CART_CS) | (1 << I2C_SDA) | (1 << I2C_SCL) ;RST active low, CS inactive high, Command mode, Tx LED off, Flash cart inactive high, I2C inactive/pullups
                           #endif
                            out     PORTD, r24
                            ldi     r24, (1 << OLED_RST) | (1 << OLED_CS) | (1 << OLED_DC) | (1 << TX_LED) | (1 << CART_CS); as outputs
                            out     DDRD, r24
                        #endif

                            ;setup SPI

                            ldi     r24,  (1 << SPE) | (1 << MSTR)  ;SPI no interrupt, SPI enable, MSB first, SPI master, mode 0, Fosc/4
                            out     SPCR, r24
                            ;ldi     r27, 1 << SPI2X                ;== 0x01 == SPI 2x speed: CPU clock / 2 (8MHz) (note value in r27 also usedd below)
                            out     SPSR, r27

                            ;initialize .data section

                            ldi     r26, lo8(__data_start)    ;X
                            ;ldi     r27, hi8(__data_start)   ;== 0x01 == already set above
                            ldi     r30, lo8(_etext)          ;Z
                            ldi     r31, hi8(_etext)
reset_b1:
                            lpm     r0, Z+                          ;copy data from end of text section
                            st      X+, r0                          ;to data section in sram
                            cpi     r26, lo8(__data_end)
                            brne    reset_b1

                            ;clear .bss section and remaining ram
reset_b2:
                           #if defined (SUPPORT_POWERDOWN)
                            ld      r24, X                      ;after the loop, r24 contains last byte of RAM which is the power down flag
                           #endif
                            st      X+, r1                      ;set RAM to zero
                            cpi     r27, hi8(RAMEND+1)
                            brne    reset_b2

                           ;sbrc    r16, EXTRF                  ;MCUSR state test external reset
                           ;rjmp    bootloader_run              ;run bootloader if so

                          #if defined (SUPPORT_POWERDOWN)
                           ;sbrs    r16, EXTRF                  ;MCUSR state test external reset
                           ;rjmp    reset_por                   ;not external reset

                           ;external reset

                            cpi     r24, SFC_POWERDOWN
                            brne    reset_por

                            ;power down
                           ;#if !(defined (ARDUBOY_PROMICRO) && defined (CART_CS_SDA))
                           ; sbi     PORTD, OLED_RST             ;pull display out of reset
                           ;#endif
                            rcall   SPI_flash_cmd_deselect
                            rjmp    powerdown
                           #endif

reset_por:
                            ;pull display out of reset
                            
                           #if !(defined (ARDUBOY_PROMICRO) && defined (CART_CS_SDA))
                            sbi     PORTD, OLED_RST             ;pull display out of reset
                           #endif
                           
                            ;test power on reset

                           #ifdef  RUN_APP_ON_POWERON
                            sbrc    r16, PORF                   ;MCUSR state test power on reset. only do button test on POR
                           #else
                            rjmp    bootloader_run              ;always enter bootloader
                           #endif

                            ;power on reset

                            sbis    BTN_DOWN_PIN, BTN_DOWN_BIT  ;test DOWN button
                            rjmp    bootloader_run              ;button pressed, enter bootloader
run_sketch:
                            rcall   TestApplicationFlash
                            brne    StartSketch                 ;run application when loaded

                            ;enter bootloader mode
bootloader_run:
                            rcall   SetupHardware_bootloader    ;setup additional hardware
                            sei
                            rcall   LoadApplicationInfo
                           #if !(defined (DS3231) || defined (RV3028))
                            breq    bootloader_loop

                            ;flash cart not initialized, display USB icon
DisplayBootGfx:
                            ldi     r26, lo8(DisplayBuffer + BOOT_LOGO_OFFSET)
                            ldi     r27, hi8(DisplayBuffer + BOOT_LOGO_OFFSET)
                            ldi     r30, lo8(bootgfx)
                            ldi     r31, hi8(bootgfx)
DisplayBootGfx_l1:
                            ldi     r25, BOOTLOGO_WIDTH
DisplayBootGfx_l2:
                            lpm     r0, z+
                            st      x+, r0
                            dec     r25
                            brne    DisplayBootGfx_l2
                            subi    r26, lo8(-(WIDTH - BOOTLOGO_WIDTH))    ;'adiw' to one line down
                            sbci    r27, hi8(-(WIDTH - BOOTLOGO_WIDTH))
                            cpi     r30,lo8(bootgfx_end)
                            brne    DisplayBootGfx_l1

                            ;display USB icon

                            rcall   Display
                           #endif

bootloader_loop:
                            rcall   CDC_Task
                            rcall   USB_USBTask
                           #if !(defined (DS3231) || defined (RV3028))
                            lds     r24, LED_Control
                            andi    r24, 1 << LED_CTRL_NOBUTTONS
                            brne    bootloader_next              ;no menu control
                           #endif

                            ;read buttons

                          #ifdef ARDUBOY_DEVKIT
                            in      r24, PINB                   ;down, left, up buttons
                            com     r24
                            andi    r24, 0x70
                            sbis    PINC, BTN_RIGHT_BIT
                            ori     r24, 1 << RIGHT_BUTTON
                          #else
                            in      r24, PINF                   ;directional buttons
                            com     r24
                            andi    r24, 0xF0
                          #endif
                            sbic    BTN_A_PIN, BTN_A_BIT
                            sbis    BTN_B_PIN, BTN_B_BIT
                            ori     r24, 1 << AB_BUTTON         ;both buttons single function
                            mov     r9, r24                     ;save buttons state

                            ;select list, game

                            rcall   SelectList

                          #if defined (SUPPORT_POWERDOWN)
                            ldi     r24, SFC_POWERDOWN
                            eor     r24, r8                    ;xor list so only a reset on loader screen powers down
                            sts     (RAMEND), r24
                          #endif

                            cpse    r8, r1                      ;skip no game list selected
                            rcall   SelectGame
                            sbrc    r9, AB_BUTTON               ;skip if not pressed
                            rjmp    bootloader_run_last

                           #if defined (DS3231) || defined (RV3028)

                            ;show time on every 16th frame while on loader screen

                            ldi     r24, 0x0F                   ;every 16 frames mask
                            and     r24, r2                     ;frame counter
                            or      r24, r8                     ;selected list
                            brne    bootloader_next             ;skip if not at 16th frame and not on loader screen

                            lds     r24,(FlashBuffer + FBO_APPPAGE_MSB) ;get show date time flag
                            sbrs    r24, 7                              ;do not show if bit is set
                            rcall   DrawDateTime

                           #endif
bootloader_next:
                            tst     r3                          ;test sign bit as timeout
                            brpl    bootloader_loop             ;no timeout
bootloader_run_last:
                            rcall   TestApplicationFlash
                            breq    bootloader_loop             ;no sketch

;-------------------------------------------------------------------------------
StartSketch:
                            cli
                            ldi     r24,SFC_POWERDOWN
                            rcall   SPI_flash_cmd_deselect
                            ldi     r24, 1 << DETACH    ;USB DETACH
                            sts     UDCON, r24
                            sts     TIMSK1, r1          ;Undo TIMER1 setup and clear the count before running the sketch
                            sts     TCCR1B, r1
                            sts     TCNT1H, r1
                            sts     TCNT1L, r1
                            ldi     r24, 1 << IVCE      ;enable interrupt vector change
                            out     MCUCR, r24
                            out     MCUCR, r1           ;relocate vector table to application section
                          #if !(defined (DS3231) || defined (RV3028))
                           #ifndef MICROCADE
                            RGB_RED_OFF
                            RGB_GREEN_OFF
                            RGB_BLUE_OFF
                           #endif
                          #endif
                            TX_LED_OFF
                            RX_LED_OFF
                            jmp     0                   ; start application

;-------------------------------------------------------------------------------

                           #if defined (DS3231) || defined (RV3028)
DrawDateTime:
                            ;load graphics data from external flash
                            
                            ldi     r30, 1 + 4              ;skip header + title screen
                            ldi     r31, 0                  ;to point to 1K RTC data
                            rcall   SPI_flash_read_addr     ;select flash address
                            
                            ldi     r26, lo8(ScrollBuffer)
                            ldi     r27, hi8(ScrollBuffer)
                            ldi     r28, 8                  ;read 1K data
load_gfx_loop:                            
                            rcall   SPI_read_page
                            dec     r28
                            brne    load_gfx_loop
                            ;rcall   SPI_flash_deselect     ;not needed

                            ;read date and time from RTC

                            rcall   i2c_enable_start
                            ldi     r24, SLA_W          ;write slave address for write
                            rcall   i2c_write
                            ldi     r24, 0              ;select seconds register
                            rcall   i2c_write
                            rcall   i2c_stop
                            rcall   i2c_start
                            ldi     r24, SLA_R          ;write slave address for read
                            rcall   i2c_write

                            ldi     r28, lo8(DateTime)  ;Y = DateTime
                            ldi     r29, hi8(DateTime)  
                            movw    r26, r28            ;X = DateTime
read_rtc_loop:
                            ldi     r24, (1 << TWINT) | (1 << TWEA) | (1 << TWEN)   ;read ack
                            rcall   i2c_cmd
                            ldd     r24, Z+3            ;Y++ = TWDR
                            st      X+, r24
                            cpi     r26, lo8(DateTimeLast)
                            brne    read_rtc_loop       ;not last, read another byte using readAck
                            rcall   i2c_nack            ;read Nack 6th last byte
                            ldd     r24, Z+3            ; Y = TWDR
                            st      X+, r24
                            rcall   i2c_stop
                            std     Z+4, r1             ;TWCR = disable

                            ;draw the date

                            ldi     r26, lo8(DigitDataTable)
                            ldi     r27, hi8(DigitDataTable)

                            ldd     r24, Y + IDX_MONTH
                            rcall   drawDoubleDigit
                            ldd     r24, Y + IDX_DATE
                            rcall   drawDoubleDigit
                            ldd     r24, Y + IDX_YEAR
                            rcall   drawDoubleDigit

                            ;draw the time
                            
                            ldd     r25, Y + IDX_HOURMODE   ;get 12/24 hour mode
                            andi    r25, MSK_HOURMODE
                            ldd     r24, Y + IDX_HOUR
                            breq    DrawDateTime_hours      ;skip no changes for 24 hour mode
                            
                            ;for 12-hour mode replace leading zero with space (digit 11)

                            andi    r24, 0x1F               ;keep hours only
                            sbrs    r24, 4                  ;skip if not leading zero
                            subi    r24, -0xA0              ;else use 11th digit leading space
DrawDateTime_hours:
                            rcall   drawDoubleDigit
                            ldd     r24, Y + IDX_MIN
                            rcall   drawDoubleDigit
                            
                            ;draw seconds

                            ldd     r24, Y + IDX_SEC
                            rcall   drawDoubleDigit
                            
                            ;test 12/24-hour mode
                            
                            tst     r25
                            breq    DrawDateTime_display    ;skip 24-hour mode

                            ;draw am/pm icon

                            ldd     r24, Y + IDX_HOUR
                            lsr     r24                     ;am/pm flag to bit 4
                            andi    r24, 0x10               ;get am/pm index
                            rcall   DrawDigit
DrawDateTime_display:                            
                            rjmp    SPI_flash_deselect_display
                            
;----------------------------------------------------
drawDoubleDigit:
                            rcall   DrawDigit
                            ;rjmp   DrawDigit
                            
;----------------------------------------------------
DrawDigit:

;draws most significant nibble as digit

;entry:
;   r24      = bcd value
;   r26, r27 = digit data table
;exit:
;   r24      = bcd value with swapped nibbles
;   r26, r27 = digit data table + 4
;uses:
;   r0, r16, r17, r18, r19, r20, r21, r22, r23, r30, r31

                            movw    r18, r28                ;save time struct pointer
                            ld      r29, X+                 ;get display offset
                            ld      r28, X+                 
                            subi    r28, lo8(-(DisplayBuffer))  ;X = display address
                            sbci    r29, hi8(-(DisplayBuffer)) 
                            ld      r31, X+                     ; get graphics offset
                            ld      r30, X+                
                            subi    r30, lo8(-(FlashBuffer))    ;Z =  graphics addresss
                            sbci    r31, hi8(-(FlashBuffer)) 
                            ld      r20, Z+                 ;get columns
                            ld      r21, Z+                 ;get rows
                            
                            mul     r21, r20                ;size = rows*columns
                            swap    r24                     ;swap high and low digits
                            mov     r22, r24                ;keep lower digit only
                            andi    r22, 0x0F       
                            mul     r22, r0                 ;offset = digit * size
                            add     r30, r0                 ;add offset to digit gfx pointer
                            adc     r31, r1     
DrawDigit_col:      
                            movw    r22, r28                ;save displayBuffer position
                            mov     r1, r21                 ;get row
DrawDigit_row:      
                            movw    r16, r28                ;test if displayBuffer pointer is in range
                            subi    r16, lo8(DisplayBuffer)
                            sbci    r17, hi8(DisplayBuffer)                
                            cpi     r17, hi8(WIDTH * HEIGHT / 8)
                            brcc    DrawDigit_ret           ;return display buffer out of range

                            ld      r0, z+                  ;gfx to display buffer
                            st      y, r0       
                            subi    r28, lo8(-WIDTH)        ;display buffer += WIDTH
                            sbci    r29, hi8(-WIDTH)        
                            dec     r1      
                            brne    DrawDigit_row       
                            movw    r28, r22                ;restore displayBuffer position
                            adiw    r28, 1                  ;displayBuffer++
                            dec     r20     
                            brne    DrawDigit_col           ;loop next column
DrawDigit_ret:                            
                            movw    r28, r18                ;restore time struct pointer
                            ret

                           #endif
;-------------------------------------------------------------------------------
FetchNextCommandByte:
;                       entry:  none
;                       exit:   r24 = byte
;                       uses:   r24,r25

                            rcall   UENUM_set_04_UEINTX_get     ;CDC_RX_EPNUM
                            rjmp    FetchNextCommandByte_b4
FetchNextCommandByte_b1:
                            rcall   UEINTX_clear_FIFOCON_RXOUTI
                            rjmp    FetchNextCommandByte_b3
FetchNextCommandByte_b2:
                            tst     r10                         ;GPIOR0_test
                            breq    FetchNextCommandByte_ret
FetchNextCommandByte_b3:
                            rcall   UEINTX_get
                            sbrs    r24, RXOUTI
                            rjmp    FetchNextCommandByte_b2
FetchNextCommandByte_b4:
                            sbrs    r24, RWAL
                            rjmp    FetchNextCommandByte_b1
                            ;rjmp   UEDATX_get
;-------------------------------------------------------------------------------
UEDATX_get:
                            lds     r24, UEDATX
FetchNextCommandByte_ret:
                            ret
;-------------------------------------------------------------------------------
WriteNextResponseByte:

;uses r0,r24,r25
                            mov     r0, r24
                            ldi     r24, 0x03                   ;
                            rcall   UENUM_set_UEINTX_get
                            sbrc    r24, RWAL
                            rjmp    WriteNextResponseByte_b3

                            rcall   UEINTX_clear_FIFOCON_TXINI
                            rjmp    WriteNextResponseByte_b2
WriteNextResponseByte_b1:
                            tst     r10                         ;GPIOR0_test
                            breq    WriteNextResponseByte_ret
WriteNextResponseByte_b2:
                            rcall   UEINTX_get
                            sbrs    r24, TXINI
                            rjmp    WriteNextResponseByte_b1
WriteNextResponseByte_b3:
                            sts     UEDATX, r0
                           #if !(defined(DS3231) || defined (RV3028))
                            lds     r24, LED_Control
                            andi    r24, 1 << LED_CTRL_RXTX
                            brne    WriteNextResponseByte_ret           ;RxTx LEDs disabled
                           #endif
                            TX_LED_ON
                            ldi     r24, lo8(TX_RX_LED_PULSE_PERIOD)
                            sts     TxLEDPulse, r24
WriteNextResponseByte_ret:
                            ret
;-------------------------------------------------------------------------------
CDC_Task:
                            rcall   UENUM_set_04_UEINTX_get
                            sbrs    r24, RXOUTI
                            rjmp    CDC_Task_ret
                           #if !(defined(DS3231) || defined (RV3028))
                            ;endpoint has command from host
                            lds     r24, LED_Control
                            andi    r24, 1 << LED_CTRL_RXTX
                            brne    CDC_Task_b1                     ;RxTx LEDs disabled
                           #endif
                            RX_LED_ON
                            ldi     r24, lo8(TX_RX_LED_PULSE_PERIOD)
                            sts     RxLEDPulse, r24
CDC_Task_b1:
                            rcall   FetchNextCommandByte
                            mov     r17, r24                        ;save command
                            ;-----------------------------------End programmer command
                            cpi     r24, 'E'
                            brne    CDC_Task_Command_T

                            clr     r2                              ;clear frame counter
                            ldi     r24,128-2                       ;-512 msec timeout
                            mov     r3, r24
CDC_Task_w1:
                            rcall   eeprom_prep                     ;wait for any EEPROM writes to finish
CDC_Task_Acknowledge:
                            ldi     r24, 0x0D                       ;send acknowledge
                            rjmp    CDC_Task_Response
CDC_Task_Command_T:         ;-----------------------------------select device
                            cpi     r24, 'T'
                            brne    CDC_Task_Command_L

                            rcall   FetchNextCommandByte    ;drop device byte
                            rjmp    CDC_Task_Acknowledge
CDC_Task_Command_L:         ;-----------------------------------leave programming mode
                            cpi     r24, 'L'
                            breq    CDC_Task_Acknowledge     ;just acknowledge

                            ;-----------------------------------enter programming mode
                            cpi     r24, 'P'
                            breq    CDC_Task_Acknowledge     ;just acknowledge

                            ;-----------------------------------Request supported device list
                            cpi     r24, 't'
                            brne    CDC_Task_Command_a

                            ;'t': Return ATMEGA128 part code - this is only to allow AVRProg to use the bootloader

                            ;ldi     r24, 0x44                   ;supported device  (not required for AVRdude)
                            ;rcall   WriteNextResponseByte
                            ldi     r24, 0x00                   ;end of supported devices list
                            rjmp    CDC_Task_Response
CDC_Task_Command_a:         ;-----------------------------------auto address increment inquiry (not required AVRdude)
                            ;cpi     r24, 'a'
                            ;brne    CDC_Task_Command_A
                            ;
                            ;ldi     r24, 'Y'                    ;'Y'es supported
                            ;rjmp    CDC_Task_Response
CDC_Task_Command_A:         ;-----------------------------------set current address / flash sector
                            cpi     r24, 'A'
                            brne    CDC_Task_Command_p

                            rcall   FetchNextCommandByte
                            mov     r5, r24
                            rcall   FetchNextCommandByte
                            mov     r4, r24
                            rjmp    CDC_Task_Acknowledge
CDC_Task_Command_p:         ;-----------------------------------programmer type (not required AVRdude)
                            ;cpi     r24, 'p'
                            ;brne    CDC_Task_Command_S
                            ;
                            ;ldi     r24, 'S'                ;'S'erial programmer
                            ;rjmp    CDC_Task_Response
CDC_Task_Command_S:         ;-----------------------------------send software identifier string response
                            cpi     r24, 'S'
                            brne    CDC_Task_Command_V

                            ldi     r30, lo8(SOFTWARE_IDENTIFIER)
                            ldi     r31, hi8(SOFTWARE_IDENTIFIER)
CDC_Task_SendID:
                            lpm     r24, z+
                            rcall   WriteNextResponseByte
                            cpi     r30, lo8(SOFTWARE_IDENTIFIER + 7)
                            brne    CDC_Task_SendID
                            rjmp    CDC_Task_Complete
CDC_Task_Command_V:         ;-----------------------------------Software version
                            cpi     r24, 'V'
                            brne    CDC_Task_Command_v

                            ldi     r24, BOOTLOADER_VERSION_MAJOR
                            rcall   WriteNextResponseByte
                            ldi     r24, BOOTLOADER_VERSION_MINOR
                            rjmp    CDC_Task_Response
CDC_Task_Command_v:         ;-----------------------------------Hardware version
                           #if !(defined (DS3231) || defined (RV3028))
                            cpi     r24, 'v'
                            brne    CDC_Task_Command_x

                            ;'v': Hardware version (returns Arduboy button states)

                            ldi     r24, '1'                ;'1' + (A-button << 1) + (B-button)
                            sbis    BTN_A_PIN, BTN_A_BIT
                            subi    r24, -2
                            sbis    BTN_B_PIN, BTN_B_BIT
                            subi    r24, -1
                            rcall   WriteNextResponseByte
                          #if defined (ARDUBOY_DEVKIT) || (MICROCADE)
                            ldi     r24, 'A'
                            sbis    BTN_UP_PIN, BTN_UP_BIT
                            subi    r24, -8
                            sbis    BTN_RIGHT_PIN, BTN_RIGHT_BIT
                            subi    r24, -4
                            sbis    BTN_LEFT_PIN, BTN_LEFT_BIT
                            subi    r24, -2
                            sbis    BTN_DOWN_PIN, BTN_DOWN_BIT
                            subi    r24, -1
                          #else
                            in      r24,PINF            ;read D-Pad buttons
                            com     r24                 ;get active high button states in low nibble
                            swap    r24
                            andi    r24, 0x0F
                            subi    r24,-'A'            ;'A' + (UP << 3) + (RIGHT << 2) + (LEFT << 1) + DOWN
                          #endif
                            rjmp    CDC_Task_Response
CDC_Task_Command_x:         ;-----------------------------------set LEDs
                            cpi     r24, 'x'
                            brne    CDC_Task_Command_j

                            rcall   FetchNextCommandByte
                            sts     LED_Control, r24
                            RX_LED_OFF
                            sbrc    r24,LED_CTRL_RX_ON
                            RX_LED_ON
                            TX_LED_OFF
                            sbrc    r24,LED_CTRL_TX_ON
                            TX_LED_ON
                           #if defined (MICROCADE)
                            sbrc    r24, LED_CTRL_NOBUTTONS     ;move this test here since this bit is cleared by and below
                            clr     r8
                            andi    r24, 0x07                   ;test RGB LED off
                            brne    .+2
                            ori     r24, 0x07                   ;if so change to white
                           #endif
                            RGB_RED_OFF
                            sbrc    r24,LED_CTRL_RGB_R_ON
                            RGB_RED_ON
                            RGB_GREEN_OFF
                            sbrc    r24,LED_CTRL_RGB_G_ON
                            RGB_GREEN_ON
                            RGB_BLUE_OFF
                            sbrc    r24,LED_CTRL_RGB_B_ON
                            RGB_BLUE_ON
                           #if !defined (MICROCADE)
                            sbrc    r24, LED_CTRL_NOBUTTONS
                            clr     r8                          ;reset menu to bootloader list
                           #endif
                            rjmp    CDC_Task_Acknowledge
                           #endif
CDC_Task_Command_j:
                            cpi     r24, 'j'
                            brne    CDC_Task_Command_s

                            ;read SPI flash Jedec ID

                            ldi     r24, SFC_JEDEC_ID
                            rcall   SPI_flash_cmd
                            ldi     r28, 3
CDC_Task_get_jedec:
                            rcall   SPI_transfer
                            rcall   WriteNextResponseByte
                            dec     r28
                            brne    CDC_Task_get_jedec
                            rcall   SPI_flash_deselect
                            rjmp    CDC_Task_Complete
CDC_Task_Command_s:         ;-----------------------------------avr signature
                            cpi     r24, 's'
                            brne    CDC_Task_Command_e

                            ldi     r24, AVR_SIGNATURE_3
                            rcall   WriteNextResponseByte
                            ldi     r24, AVR_SIGNATURE_2
                            rcall   WriteNextResponseByte
                            ldi     r24, AVR_SIGNATURE_1
                            rjmp    CDC_Task_Response
CDC_Task_Command_e:         ;-----------------------------------
                           ; cpi     r24, 'e'                            ;erase chip
                           ; brne    CDC_Task_Command_b
                           ;
                           ; ;'e': erase application section
                           ;
                           ; ldi     r30, lo8(BOOT_START_ADDR - SPM_PAGESIZE)
                           ; ldi     r31, hi8(BOOT_START_ADDR - SPM_PAGESIZE)
CDC_Task_Erase:            ;
                           ; ;ldi     r24, (1 << PGERS) | (1 << SPMEN)    ;Page erase
                           ; rcall   SPM_page_erase
                           ; subi    r30, lo8(SPM_PAGESIZE)
                           ; sbci    r31, hi8(SPM_PAGESIZE)
                           ; brcc    CDC_Task_Erase                      ;loop until 1st application page done
                           ; rjmp    CDC_Task_Acknowledge
CDC_Task_Command_b:         ;-----------------------------------block support command
                            cpi     r24, 'b'
                            brne    CDC_Task_Command_B

                            ldi     r24, 'Y'                ;Yes
                            rcall   WriteNextResponseByte
                            ldi     r24, hi8(SPM_PAGESIZE)
                            rcall   WriteNextResponseByte   ;MSB flash Page size
                            ldi     r24, lo8(SPM_PAGESIZE)
                            rjmp    CDC_Task_Response       ;LSB flash Page size

CDC_Task_Command_B:         ;-----------------------------------Write memory block
                            cpi     r24, 'B'
                            breq    CDC_Task_RdWrBlk
                            ;-----------------------------------Read memory block
                            cpi     r24, 'g'
                            breq    CDC_Task_RdWrBlk
                            rjmp    CDC_Task_TestBitCmds
CDC_Task_RdWrBlk:           ;-----------------------------------'B' or 'g': write/read memory block
                            clr     r3
                            rcall   FetchNextCommandByte
                            mov     r29, r24                ;BlockSize MSB
                            rcall   FetchNextCommandByte
                            mov     r28, r24                ;BlockSize LSB
                            rcall   FetchNextCommandByte
                            mov     r16, r24                ;MemoryType
                            subi    r24, 'C'                ;Arduboy Supports 'C'artridge and 'D'isplay memory blocks
                            cpi     r24, 0x04               ;'F' - 'D' + 1
                            brcs    CDC_Task_RdWrBlk_check_f
                            rjmp    CDC_Task_Error          ;not 'D'ISPLAY, 'E'EPROM or 'F'LASH
CDC_Task_RdWrBlk_check_f:
                            cpi     r16, 'F'
                            brne    CDC_Task_RdWrBlk_begin
                            lsl     r4                     ;word to byte addr for Flash
                            rol     r5
CDC_Task_RdWrBlk_begin:
                            movw    r30, r4                 ;get current Address in Z
                            sts     TIMSK1, r1              ;disable timer 1 interrupt
                            cpi     r17, 'g'
                            brne    CDC_Task_WriteMem

                            ;Read Block
CDC_Task_ReadBlk:
                            cpi     r16, 'C'                    ;test SPI flash cart
                            brne    CDC_Task_ReadBlk_next

                            ;read SPI flash cart

                            ldi     r24, SFC_READ_DATA
                            rcall   SPI_flash_cmd_addr          ;send read command, set address
                            rcall   SPI_transfer                ;slow first read
CDC_Task_ReadBlk_cart:
                            in      r24, SPDR                   ;read flash byte
                            out     SPDR, r1                    ;start next read
                            rcall   WriteNextResponseByte
                            in      r0, SPSR                    ;clear SPIF
                            sbiw    r28, 1                      ;length of 0 == 64K length
                            brne    CDC_Task_ReadBlk_cart
CDC_Task_ReadBlk_cart_end:
                            rcall   SPI_flash_deselect
                            rjmp    CDC_Task_ReadBlk_end
CDC_Task_ReadBlk_loop:
                            cpi     r16, 'F'
                            brne    CDC_Task_ReadBlk_EEPROM

                            lpm     r24, Z+
                            rjmp    CDC_Task_ReadBlk_send
CDC_Task_ReadBlk_EEPROM:
                            rcall   eeprom_read
CDC_Task_ReadBlk_send:
                            rcall   WriteNextResponseByte
CDC_Task_ReadBlk_next:
                            sbiw    r28, 0x01
                            brpl    CDC_Task_ReadBlk_loop
CDC_Task_ReadBlk_end:
                            movw    r4, r30                     ;update current Address
                            rjmp    CDC_Task_RdWrBlk_end

                            ;write block
CDC_Task_WriteMem:
                            cpi     r16, 'C'
                            brne    CDC_Task_WriteMem_flash

                            ;write flash cart
CDC_Task_Write_cart_sector:
                            cpse    r30, r1                      ;test if at beginnning of 64K block (page = 0)
                            rjmp    CDC_Task_Write_cart_page

                            rcall   SPI_write_enable
                            ldi     r24, SFC_64K_ERASE
                            rcall   SPI_flash_cmd_addr
                            rcall   SPI_flash_wait
CDC_Task_Write_cart_page:
                            rcall   SPI_write_enable
                            ldi     r24, SFC_PAGE_PROGRAM
                            rcall   SPI_flash_cmd_addr
CDC_Task_Write_cart_data:
                            rcall   FetchNextCommandByte        ;write page data
                            in      r0, SPSR                    ;clear SPIF from previous transfer
                            out     SPDR, r24                   ;SPI transfer without wait
                            dec     r28                         ;test last page byte written
                            brne    CDC_Task_Write_cart_data

                            adiw    r30, 1                      ;next page
                            rcall   SPI_wait                    ;wait for last SPI transfer to complete
                            rcall   SPI_flash_wait              ;program page amd wait to complete
                            dec     r29
                            brne    CDC_Task_Write_cart_sector
                            movw    r4, r30                     ;update current Address
                            rjmp    CDC_Task_WriteMem_end
CDC_Task_WriteMem_flash:
                            cpi     r16, 'F'
                            brne    CDC_Task_WriteMem_next

                            ;write flash memory block

                            movw    r18, r30                    ;save addr for page write
                            cpi     r31, hi8(BOOT_START_ADDR)   ;test extended bootloader area
                            brcc    CDC_Task_WriteMem_next

                            ;Flash memory page erase

                            rcall   SPM_page_erase
                            rjmp    CDC_Task_WriteMem_next

                            ;Write Memory loop
CDC_Task_WriteMem_loop:
                            rcall   FetchNextCommandByte
                            cpi     r16, 'F'
                            brne    CDC_Task_WriteMem_display

                            ;Flash

                            bst     r28, 0                   ;block length
                            brts    CDC_Task_WriteMem_lsb

                            ;msb,  write word

                            cpi     r31, hi8(BOOT_START_ADDR)
                            brcc    CDC_Task_WriteMem_inc

                            mov     r1, r24                 ;word in r0:r1
                            ldi     r24,(1 << SPMEN)
                            out     SPMCSR, r24             ;write word to page buffer
                            spm
                            eor     r1, r1                  ;restore zero reg
CDC_Task_WriteMem_inc:
                            adiw    r30, 2
CDC_Task_WriteMem_lsb:
                            mov     r0, r24                 ;save lsb
                            rjmp    CDC_Task_WriteMem_next

CDC_Task_WriteMem_display:
                            cpi     r16, 'D'
                            brne    CDC_Task_WriteMem_eeprom

                            ;OLED display

                           #if !(defined (DS3231) || defined (RV3028))
                            movw    r26, r30                    ;current addr
                            andi    r27, 0x3                    ;keep 1K address range
                            subi    r26, lo8(-(DisplayBuffer))
                            sbci    r27, hi8(-(DisplayBuffer))
                            st      X+, r24
                            adiw    r30, 1
                           #endif
                            rjmp    CDC_Task_WriteMem_next

                            ;EEPROM
CDC_Task_WriteMem_eeprom:
                            rcall   eeprom_write
CDC_Task_WriteMem_next:
                            sbiw    r28, 0x01
                            brpl    CDC_Task_WriteMem_loop

                            ;block write complete

                           #if !(defined (DS3231) || defined (RV3028))
                            cpi     r16, 'D'
                            brne    CDC_Task_WriteMem_flash_end

                            ;copy display buffer to display if full

                            movw    r4, r30                             ;update current address
                            andi    r31, 0x03
                            or      r31, r30
                            brne    CDC_Task_WriteMem_end               ;update display on 1K overflow
                            rcall   Display
                            rjmp    CDC_Task_WriteMem_end
                           #endif
CDC_Task_WriteMem_flash_end:
                            cpi     r16, 'F'
                            brne    CDC_Task_WriteMem_end

                            ;Flash memory Page write

                            movw    r4, r30
                            movw    r30, r18                           ;page addr

                            cpi     r31, hi8(BOOT_START_ADDR)
                            brcc    CDC_Task_WriteMem_end              ;don't flash pages in protected bootloader area

                            rcall   SPM_page_write
CDC_Task_WriteMem_end:
                            ldi     r24, 0x0D
                            rcall   WriteNextResponseByte
CDC_Task_RdWrBlk_end:
                            cpi     r16, 'F'                    ;convert byte to word addr for flash
                            brne    CDC_Task_RdWrBlk_end_2
                            lsr     r5
                            ror     r4
CDC_Task_RdWrBlk_end_2:
                            ldi     r24, 0x02                   ;OCIE1A
                            sts     TIMSK1, r24                 ;enable timer1 int
                            rjmp    CDC_Task_Complete
CDC_Task_TestBitCmds:       ;-----------------------------------get lock bits
                            cpi     r24, 'r'
                            ldi     r30, 0x01
                            breq    CDC_Task_getfusebits
                            ;-----------------------------------get low fuse bits
                            cpi     r24, 'F'
                            ldi     r30, 0x00
                            breq    CDC_Task_getfusebits
                            ;-----------------------------------get high fuse bits
                            cpi     r24, 'N'
                            ldi     r30, 0x03
                            breq    CDC_Task_getfusebits
                            ;-----------------------------------get extended fuse bits
                            cpi     r24, 'Q'
                            brne    CDC_Task_Command_D

                            ldi     r30, 0x02               ;get extended fuse bits

                            ;r30 = type of bits to read
CDC_Task_getfusebits:
                            ldi     r31, 0x00
                            ldi     r24, 0x09
                            out     SPMCSR, r24
                            lpm     r24, Z
                            rjmp    CDC_Task_Response
CDC_Task_Command_D:         ;-----------------------------------Write EEPROM byte
;                            cpi     r24, 'D'
;                            brne    CDC_Task_Command_d
;
;                            rcall   FetchNextCommandByte
;                            rcall   eeprom_write
;                            rjmp    CDC_Task_Acknowledge
;
;CDC_Task_Command_d:         ;-----------------------------------Read EEPROM byte
;                            cpi     r24, 'd'
;                            brne    CDC_Task_Command_1B
;
;                            rcall   eeprom_read
;                            rjmp    CDC_Task_Response
CDC_Task_Command_1B:        ;-----------------------------------ESCAPE
                            cpi     r24, 0x1B
                            breq    CDC_Task_Complete
CDC_Task_Error:             ;-----------------------------------Unsupported command
                            ldi     r24, '?'
                            ;-----------------------------------(send byte in r24)
CDC_Task_Response:
                            rcall   WriteNextResponseByte
CDC_Task_Complete:          ;-----------------------------------
                            ldi     r24, 0x03
                            rcall   UENUM_set
                            rcall   UEINTX_clear_FIFOCON_TXINI
                            sbrs    r25, RWAL
                            rjmp    x76d6
                            rjmp    x76f0

x76d0:                      tst     r10                         ;GPIOR0_test
                            breq    CDC_Task_ret

x76d6:                      rcall   UEINTX_get
                            sbrs    r24, TXINI
                            rjmp    x76d0

                            rcall   UEINTX_clear_FIFOCON_TXINI
                            rjmp    x76f0

x76ea:                      tst     r10                         ;GPIOR0_test
                            breq    CDC_Task_ret

x76f0:                      rcall   UEINTX_get
                            sbrs    r24, TXINI
                            rjmp    x76ea

                            ldi     r24, 4
                            rcall   UENUM_set
                            ;rjmp   UEINTX_clear_FIFOCON_RXOUTI
;-------------------------------------------------------------------------------
UEINTX_clear_FIFOCON_RXOUTI:
                            ldi     r24, ~(1 << FIFOCON | 1 << RXOUTI)
                            rjmp   UEINTX_clearbits
;-------------------------------------------------------------------------------
CDC_Task_ret:
                            ret
;-------------------------------------------------------------------------------
EVENT_USB_Device_ControlRequest:
                            ldi     r30, lo8(LineEncoding)
                            ldi     r31, hi8(LineEncoding)
                            ldi     r22, lo8(sizeof_LineEncoding)

                            lds     r25, USB_ControlRequest_bmRequestType
                            lds     r24, USB_ControlRequest_brequest
                            cpi     r25, 0x21                           ;REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE
                            breq    EVENT_USB_Device_ControlRequest_b1

                            cpi     r25, 0xA1                           ;REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE
                            brne    EVENT_USB_Device_ControlRequest_ret

                            ;REQUEST_DEVICETOHOST_CLASS_INTERFACE

                            cpi     r24, CDC_REQ_GetLineEncoding
                            brne    EVENT_USB_Device_ControlRequest_ret
                            rcall   UEINTX_clear_RXSTPI
                            rjmp    Endpoint_Write_Control_Stream_LE

EVENT_USB_Device_ControlRequest_b1:
                            ;REQUEST_HOSTTODEVICE_CLASS_INTERFACE

                            cpi     r24, CDC_REQ_SetControlLineState
                            brne    EVENT_USB_Device_ControlRequest_b2

                            ld      r24, z+         ;check LineEncoding.BaudRateBPS
                            subi    r24, lo8(1200)
                            ld      r24, z+
                            sbci    r24, hi8(1200)
                            brne    EVENT_USB_Device_ControlRequest_ret ;return baudrate != 1200

                            lds     r24, USB_ControlRequest_wValueL
                            sbrc    r24, 0
                            rjmp    EVENT_USB_Device_ControlRequest_ret ;return DTR set

                            ;reset bootloader

                            cli                         ;disabgle interrupts
                            ldi     r24, 1 << DETACH    ;USB DETACH
                            sts     UDCON, r24
                           #if defined (SUPPORT_POWERDOWN)
                            sts     (RAMEND), r1        ;prevent shutdown on restart
                           #endif
                            rjmp    VECTOR_00_7800      ;restart bootloader

EVENT_USB_Device_ControlRequest_b2:
                            cpi     r24, CDC_REQ_SetLineEncoding
                            brne    EVENT_USB_Device_ControlRequest_ret

                            rcall   UEINTX_clear_RXSTPI
                            ;rcall   Endpoint_Read_Control_Stream_LE
                            ;rjmp   UEINTX_clear_FIFOCON_TXINI

;-------------------------------------------------------------------------------
Endpoint_Read_Control_Stream_LE:

;entry:
;   r30:31 points to LineEncoding, r22 lineEncoding length

Endpoint_Read_CtrlStrm_b2:
                            tst     r10                         ;GPIOR0_test
                            breq    Endpoint_Read_CtrlStrm_ret

                            cpi     r24, 5
                            breq    Endpoint_Read_CtrlStrm_ret

                            rcall   UEINTX_get
                            sbrc    r24, RXSTPI
                            rjmp    Endpoint_Read_CtrlStrm_ret
Endpoint_Read_CtrlStrm_b3:
                            sbrs    r24, RXOUTI
                            rjmp    Endpoint_Read_CtrlStrm_b2
                            rjmp    Endpoint_Read_CtrlStrm_b5
Endpoint_Read_CtrlStrm_b4:
                            rcall   UEDATX_get
                            st      Z+, r24
                            subi    r22, 1
                            breq    Endpoint_Read_CtrlStrm_b6
Endpoint_Read_CtrlStrm_b5:
                            lds     r25, UEBCHX
                            lds     r24, UEBCLX
                            or      r24, r25                    ;r24 = UEBCLX | UEBCHX
                            brne    Endpoint_Read_CtrlStrm_b4
Endpoint_Read_CtrlStrm_b6:
                            rcall   UEINTX_clear_FIFOCON_RXOUTI
Endpoint_Read_CtrlStrm_b7:
                            cp      r22, r1
                            brne    Endpoint_Read_CtrlStrm_b2   ;length != 0
                            rjmp    Endpoint_Read_CtrlStrm_b9
Endpoint_Read_CtrlStrm_b8:
                            tst     r10                         ;GPIOR0_test
                            breq    Endpoint_Read_CtrlStrm_ret

                            cpi     r24, 5
                            breq    Endpoint_Read_CtrlStrm_ret
Endpoint_Read_CtrlStrm_b9:
                            rcall   UEINTX_get
                            sbrs    r24, TXINI
                            rjmp    Endpoint_Read_CtrlStrm_b8
Endpoint_Read_CtrlStrm_ret:
                            ;ret

;-------------------------------------------------------------------------------
UEINTX_clear_FIFOCON_TXINI:
                            ldi     r24, ~(1 << FIFOCON | 1 << TXINI)
                            ;rjmp    UEINTX_clearbits

;-------------------------------------------------------------------------------
;USB Endpoint clear interrupt flag bits

;entry:
;   r24 = bitmask
;exit:
;   r25 = UEINTX state

UEINTX_clearbits:
                            lds     r25, UEINTX
                            and     r24, r25
                            ;rjmp   UEINTX_set
;-------------------------------------------------------------------------------
UEINTX_set:
                            sts     UEINTX, r24
EVENT_USB_Device_ControlRequest_ret:
                            ret
;-------------------------------------------------------------------------------
UENUM_set_04_UEINTX_get:    ldi     r24, 0x04
                            ;rjmp   UENUM_set_UEINTX_get
;-------------------------------------------------------------------------------
UENUM_set_UEINTX_get:       rcall   UENUM_set
                            ;rjmp   UEINTX_get
;-------------------------------------------------------------------------------
UEINTX_get:
                            lds     r24, UEINTX
                            ret
;-------------------------------------------------------------------------------
UDIEN_get:
                            lds     r24, UDIEN
                            ret
;-------------------------------------------------------------------------------
UDIEN_Clr0_Set4:
                            rcall   UDIEN_get
                            andi    r24, 0xFE
                            ori     r24, 0x10
                            ;rjmp   UDIEN_set
;-------------------------------------------------------------------------------
UDIEN_set:
                            sts     UDIEN, r24
                            ret
;-------------------------------------------------------------------------------
EVENT_USB_Device_ConfigurationChanged:
                            ldi     r24, 0x02
                            ldi     r22, 0xC1
                            rcall   Endpoint_ConfigureEndpoint_Prv_02
                            ldi     r24, 0x03
                            ldi     r22, 0x81
                            rcall   Endpoint_ConfigureEndpoint_Prv_12
                            ldi     r24, 0x04
                            ldi     r22, 0x80
                            ;rjmpEndpoint_ConfigureEndpoint_Prv_12
;-------------------------------------------------------------------------------
Endpoint_ConfigureEndpoint_Prv_12:
                            ldi     r20, 0x12
                            rjmp    Endpoint_ConfigureEndpoint_Prv
;-------------------------------------------------------------------------------
Endpoint_ConfigureEndpoint_Prv_00_00_02:
                            ldi     r24, 0x00
                            ldi     r22, 0x00
                            ;rjmp   Endpoint_ConfigureEndpoint_Prv_02
;-------------------------------------------------------------------------------
Endpoint_ConfigureEndpoint_Prv_02:
                            ldi     r20, 0x02
                            ;rjmp   Endpoint_ConfigureEndpoint_Prv
;-------------------------------------------------------------------------------
Endpoint_ConfigureEndpoint_Prv:

;uses: r20,r22,r24,r25
                            rcall   UENUM_set
                            ldi     r24, 0x01
                            rcall   UECONX_setbits
                            sts     UECFG1X, r1
                            sts     UECFG0X, r22
                            sts     UECFG1X, r20
                            ret
;-------------------------------------------------------------------------------
UECONX_setbits:

;uses: r24,r25
                            lds     r25, UECONX
                            or      r24, r25
                            sts     UECONX, r24
                            ret
;-------------------------------------------------------------------------------
CALLBACK_USB_GetDescriptor:

;entry:
;   r24 = DiscriptorNumber, r25 = DiscriptorType
;exit:
;   r30:31 = Descriptor address, r22 = length
;note:
;   when length == 0, pointer is ignored

                            ldi     r22, 0x00                  ;zero length for unsupported types
                            cpi     r25, 0x02                   ;DTYPE_Configuration
                            breq    CALLBACK_USB_GetDesc_conf

                          #if defined (USE_LANGUAGE_STRING) || (USE_PRODUCT_STRING) || (USE_LANGUAGE_STRING)
                            cpi     r25, 0x03                   ;DTYPE_String
                            breq    CALLBACK_USB_GetDesc_str
                          #endif

                            cpi     r25, 0x01                   ;DTYPE_Device
                            brne    CALLBACK_USB_GetDesc_ret

                            ;01: DTYPE_Device
CALLBACK_USB_GetDesc_dev:
                            ldi     r30, lo8(DeviceDescriptor)
                            ldi     r22, lo8(sizeof_DeviceDescriptor)
                            rjmp    CALLBACK_USB_GetDesc_ret

                            ;02: DTYPE_Configuration
CALLBACK_USB_GetDesc_conf:
                            ldi     r30, lo8(ConfigurationDescriptor)
                            ldi     r22, lo8(sizeof_ConfigurationDescriptor)
                          #if defined (USE_PRODUCT_STRING) || (USE_LANGUAGE_STRING) || (USE_LANGUAGE_STRING)
                            rjmp    CALLBACK_USB_GetDesc_ret

                            ;03: DTYPE_String

CALLBACK_USB_GetDesc_str:
                            cpi     r24, 1
                          #endif
                          #if defined (USE_LANGUAGE_STRING)
                            brcc    CALLBACK_USB_GetDesc_b1 ; > 0

                            ;0: LanguageString

                            ldi     r30, lo8(LanguageString)
                            ldi     r22, lo8(sizeof_LanguageString)
                           #if defined (USE_PRODUCT_STRING) || (USE_MANUFACTURE_STRING)
                            rjmp    CALLBACK_USB_GetDesc_ret
                           #endif
                          #endif

                            ;!0:
CALLBACK_USB_GetDesc_b1:
                          #if defined (USE_PRODUCT_STRING)
                            brne    CALLBACK_USB_GetDesc_b2

                            ;1: ProductString

                            ldi     r30, lo8(ProductString)
                            ldi     r22, lo8(sizeof_ProductString)
                          #endif
                          #if defined (USE_MANUFACTURE_STRING)
                            rjmp    CALLBACK_USB_GetDesc_ret
                          #endif

                            ;>1:
CALLBACK_USB_GetDesc_b2:
                          #if defined (USE_MANUFACTURE_STRING)
                            cpi     r24, 0x02
                            brne    CALLBACK_USB_GetDesc_ret

                            ;2: ManufacturerString

                            ldi     r30, lo8(ManufacturerString)
                            ldi     r22, lo8(sizeof_ManufacturerString)
                          #endif
CALLBACK_USB_GetDesc_ret:
                            ldi     r31, hi8(DeviceDescriptor) ;all descriptor data in same 256 byte page
Endpoint_ClearStatus_ret:
                            ret
;-------------------------------------------------------------------------------
Endpoint_ClearStatusStage:
                            lds     r24, USB_ControlRequest_bmRequestType
                            and     r24, r24
                            brge    Endpoint_ClearStatus_b4

                            rjmp    Endpoint_ClearStatus_b2
Endpoint_ClearStatus_b1:
                            tst     r10                         ;GPIOR0_test
                            breq    Endpoint_ClearStatus_ret
Endpoint_ClearStatus_b2:
                            rcall   UEINTX_get
                            sbrs    r24, RXOUTI
                            rjmp    Endpoint_ClearStatus_b1
                            rjmp    UEINTX_clear_FIFOCON_RXOUTI
Endpoint_ClearStatus_b3:
                            tst     r10                         ;GPIOR0_test
                            breq    Endpoint_ClearStatus_ret
Endpoint_ClearStatus_b4:
                            rcall   UEINTX_get
                            sbrs    r24, TXINI
                            rjmp    Endpoint_ClearStatus_b3

                            rjmp   UEINTX_clear_FIFOCON_TXINI
;-------------------------------------------------------------------------------
Endpoint_Write_Control_Stream_LE:

;entry:
;   r30:31 = pointer to data, r22 = length

                            lds     r20, USB_ControlRequest_wLength+0
                            lds     r21, USB_ControlRequest_wLength+1
                            cp      r20, r22
                            cpc     r21, r1
                            brcs    x7b80                               ;ControlRequest_wLength < length

                            ;ControlRequest_wLength >= length
x7b86:
                            mov     r20, r22
                            cp      r22, r1
                            brne    x7b80
x7b90:
                            rcall   UEINTX_clear_FIFOCON_TXINI

                            ;ControlRequest_wLength < length
x7b80:
                            ldi     r21, 0x00                           ;LastPacketFull = false
                            rjmp    x7c0e                               ;continue

                            ;---- r0 > 0 or LastPacketFull
x7ba0:
                            tst     r10                         ;GPIOR0_test
                            breq    x7c30

                            cpi     r24, 0x05
                            breq    x7c34

                            rcall   UEINTX_get
                            sbrc    r24, RXSTPI
                            rjmp    UEINTX_clear_FIFOCON_RXOUTI

                            rcall   UEINTX_get
                            sbrc    r24, RXOUTI
                            rjmp    x7c24

                            sbrs    r24, TXINI
                            rjmp    x7c0e

                            lds     r19, UEBCHX             ;FIFO endpoint byte count MSB
                            lds     r18, UEBCLX             ;FIFO endpoint byte count LSB
                            rjmp    x7bee

                            ;0..7
x7be0:
                            ld      r24, Z+                 ;data
                            sts     UEDATX, r24             ;to USB
                            subi    r20, 0x01               ;length--
                            subi    r18, 0xFF               ;BytesInEndPoint++
                            sbci    r19, 0xFF
x7bee:
                            cp      r20, r1
                            breq    x7bfa                   ;length = 0

                            cpi     r18, 0x08               ;USB_Device_ControlEndpointSize
                            cpc     r19, r1
                            brcs    x7be0                   ;loop < 8
x7bfa:
                            ldi     r21, 0x00               ;LastPacketFull = false
                            cpi     r18, 0x08               ;USB_Device_ControlEndpointSize
                            cpc     r19, r1
                            brne    x7c04

                            ldi     r21, 0x01               ;LastPacketFull = true
x7c04:
                            rcall   UEINTX_clear_FIFOCON_TXINI

                            ;loop
x7c0e:
                            cp      r20, r1
                            brne    x7ba0

                            ;r20 = 0

                            and     r21, r21
                            brne    x7ba0                   ;LastPacketFull
                            rjmp    x7c24
x7c1a:
                            tst     r10                     ;GPIOR0_test
                            breq    x7c30

                            cpi     r24, 0x05
                            breq    x7c34
x7c24:
                            rcall   UEINTX_get
                            sbrs    r24, RXOUTI
                            rjmp    x7c1a
x7c30:
x7c34:
                            rjmp    UEINTX_clear_FIFOCON_RXOUTI
;-------------------------------------------------------------------------------
USB_Device_ProcessControlRequest:

                            ldi  r30, lo8(USB_ControlRequest)
                            ldi  r31, hi8(USB_ControlRequest)

                            ;read USB_ControlRequest

USB_Device_ProcessControlRequest_b1:
                            rcall   UEDATX_get
                            st      Z+, r24
                            ;ldi    r24, hi8(USB_ControlRequest + sizeof_USB_ControlRequest_t)
                            cpi     r30, lo8(USB_ControlRequest + sizeof_USB_ControlRequest_t)
                            ;cpc    r31, r24                                                    ;not required size < 256
                            brne    USB_Device_ProcessControlRequest_b1

                            rcall   EVENT_USB_Device_ControlRequest
                            rcall   UEINTX_get                      ;get USB Endpoint Interrupt
                            sbrs    r24, RXSTPI                     ;Received setup Interrupt Flag
jmp_x7eb2:                  rjmp    x7eb2

                            ;RXSTPI Setup received

                            lds     r24, USB_ControlRequest_bmRequestType
                            lds     r25, USB_ControlRequest_brequest

                            cpi     r25, 0x01
                            breq    x7d60       ;bmRequestType = 1: REQ_ClearFeature
                            brcs    x7d20       ;< 1: bmRequestType= 0: REQ_GetStatus

                            cpi     r25, 0x03   ;bmRequestType = 3: REQ_SetFeature
                            breq    x7d60

                            cpi     r25, 0x05
                            brne    x7cf8
                            rjmp    x7dd2       ;bmRequestType = 5: REQ_SetAddress
x7cf8:
                            cpi     r25, 0x06
                            brne    x7d0c
                            rjmp    x7e18       ;bmRequestType = 6: REQ_GetDescriptor
x7d0c:
                            cpi     r25, 0x08
                            brne    x7d12
                            rjmp    x7e58       ;bmRequestType = 8: REQ_GetConfiguration
x7d12:
                            cpi     r25, 0x09
                            brne    jmp_x7eb2   ;others: end
                            rjmp    x7e7c       ;bmRequestType = 9: REQ_SetConfiguration

                            ;bmRequestType= 0: REQ_GetStatus

x7d20:
                            cpi     r24, 0x82
                            brne    jmp_x7eb2

                            ;r24 = 0x82

                            lds     r24, USB_ControlRequest_wIndex
                            rcall   UENUM_set_and_07
                            lds     r18, UECONX
                            sts     UENUM, r1
                            rcall   UEINTX_clear_RXSTPI
                            swap    r18                     ;(r18 >> 5) & 1
                            lsr     r18
                            andi    r18, 0x01               ;STALLRQ ? 1 : 0
                            sts     UEDATX, r18
                            sts     UEDATX, r1
                            rjmp    x7e6e

                            ;bmRequestType = 1,3 REQ_ClearFeature, REQ_SetFeature

x7d60:                      and     r24, r24    ;USB_ControlRequest_bmRequestType
                            breq    x7d6a

                            cpi     r24, 0x02
                            brne    jmp_x7eb2

                            ;0,2:

x7d6a:                      andi    r24, 0x1F
                            cpi     r24, 0x02
                            brne    jmp_x7eb2

                            lds     r24, USB_ControlRequest_wValueL
                            and     r24, r24
                            brne    x7dc6

                            lds     r18, USB_ControlRequest_wIndex
                            andi    r18, 0x07
                            breq    jmp_x7eb2

                            sts     UENUM, r18
                            lds     r24, UECONX
                            sbrs    r24, 0
                            rjmp    x7dc6

                            cpi     r25, 0x03
                            brne    x7d9c

                            ldi     r24, 0x20
                            rjmp    x7dc2
x7d9c:
                            ldi     r24, 0x10
                            rcall   UECONX_setbits
                            ldi     r24, 0x01   ; 1 << r18 (r18 > 0)
x7dac:
                            add     r24, r24
                            dec     r18
                            brne    x7dac

                            sts     UERST, r24
                            sts     UERST, r1
                            ldi     r24, 0x08
x7dc2:
                            rcall   UECONX_setbits
x7dc6:
                            sts     UENUM, r1
                            rcall   UEINTX_clear_RXSTPI
                            rjmp    x7e74

                            ;bmRequestType = 5 REQ_SetAddress

x7dd2:                      and     r24, r24
                            brne    jmp_x7eb2_2

                            lds     r17, USB_ControlRequest_wValueL
                            ori     r17, 0x80
                            in      r16, SREG
                            cli
                            rcall   UEINTX_clear_RXSTPI
                            rcall   Endpoint_ClearStatusStage

x7dee:                      rcall   UEINTX_get
                            sbrs    r24, TXINI
                            rjmp    x7dee

                            sts     UDADDR, r17
                            cpi     r17, 0x80
                            ldi     r24, 0x03
                            brne    x7e12

                            ldi     r24, 0x02
x7e12:
                            mov     r10, r24    ;GPIOR0
                            out     SREG, r16
jmp_x7eb2_2:
                            rjmp    x7eb2

                            ;bmRequestType = 6: REQ_GetDescriptor

x7e18:                      subi    r24, 0x80
                            cpi     r24, 0x02
                            brcc    x7eb2       ;wasn't 128 or 129

x7e20:                      lds     r24, USB_ControlRequest_wValueL
                            lds     r25, USB_ControlRequest_wValueH
                            rcall   CALLBACK_USB_GetDescriptor
                            cpi     r22, 0
                            breq    x7eb2

                            ;length > 0

                            rcall   UEINTX_clear_RXSTPI
                            rcall   Endpoint_Write_Control_Stream_LE
                            rjmp    x7eb2

x7e58:                      cpi     r24, 0x80
                            brne    x7eb2

                            rcall   UEINTX_clear_RXSTPI
                            lds     r24, USB_Device_ConfigurationNumber
                            sts     UEDATX, r24
x7e6e:
                            rcall   UEINTX_clear_FIFOCON_TXINI
x7e74:
                            rcall   Endpoint_ClearStatusStage
                            rjmp    x7eb2

x7e7c:                      and     r24, r24
                            brne    x7eb2

                            lds     r25, USB_ControlRequest_wValueL
                            cpi     r25, 0x02
                            brcc    x7eb2

                            sts     USB_Device_ConfigurationNumber, r25
                            rcall   UEINTX_clear_RXSTPI
                            rcall   Endpoint_ClearStatusStage
                            lds     r24, USB_Device_ConfigurationNumber
                            and     r24, r24
                            brne    x7eac

                            lds     r24, UDADDR
                            ldi     r25, 0x01       ;DEVICE_STATE_Powered
                            sbrc    r24, 7
x7eac:                      ldi     r25, 0x04       ;DEVICE_STATE_Configured
                            mov     r10, r25        ;GPIOR0
                            rcall   EVENT_USB_Device_ConfigurationChanged
x7eb2:
                            rcall   UEINTX_get
                            sbrs    r24, RXSTPI
                            ret

                            ldi     r24, 0x20
                            rcall   UECONX_setbits
                            ;rjmp    UEINTX_clear_RXSTPI
;-------------------------------------------------------------------------------
UEINTX_clear_RXSTPI:
                            ldi     r24, ~(1 << RXSTPI)
                            rjmp    UEINTX_clearbits
;-------------------------------------------------------------------------------
;flash PROGMEM
;-------------------------------------------------------------------------------
FlashPage_pageBuffer:
                            ldi     r26, lo8(FlashBuffer)
                            ldi     r27, hi8(FlashBuffer)
                            ;rjmp    FlashPage
;-------------------------------------------------------------------------------
FlashPage:

;entry:
;    x = *data to burn in ram
;    z = page address
;uses:
;    r0, r24, r25, x
                            cpi     r31, hi8(BOOT_START_ADDR)
                            brcc    SPM_ret                     ;protect bootloader area

                            rcall   SPM_page_erase
                            ldi     r25, SPM_PAGESIZE >> 1
FlahsPage_loop:
                            ld      r0, x+
                            ld      r1, x+
                            ldi     r24, (1 << SPMEN)
                            out     SPMCSR, r24
                            spm
                            adiw    r30, 2
                            dec     r25
                            brne    FlahsPage_loop
                            clr     r1                          ;restore zero reg
                            subi    r30, lo8(SPM_PAGESIZE)
                            sbci    r31, hi8(SPM_PAGESIZE)
                            ;rjmp   SPM_page_write
;-------------------------------------------------------------------------------
SPM_page_write:
                            ldi     r24, (1 << PGWRT) | (1 << SPMEN)    ;write page
                            rjmp    SPM_write
;-------------------------------------------------------------------------------
SPM_page_erase:
                            ldi     r24, (1 << PGERS) | (1 << SPMEN) ;Page Erase
                            ;rjmp   SPM_write
;-------------------------------------------------------------------------------
SPM_write:                  rcall   SPM_write_sub
                            ldi     r24, (1 << RWWSRE) | (1 << SPMEN)   ;RWW section read enable
                            ;rjmp   SPM_write_sub
;-------------------------------------------------------------------------------
SPM_write_sub:
                            out     SPMCSR, r24
                            spm
SPM_wait:
                            in      r0, SPMCSR
                            sbrc    r0, 0
                            rjmp    SPM_wait
;-------------------------------------------------------------------------------
SPM_ret:                    ret

;-------------------------------------------------------------------------------
                            .section    .text
;-------------------------------------------------------------------------------

pll_enable:
                            ldi     r24, 0x10       ;PINDIV (PLL 1:2, clear PLOCK)
                            out     PLLCSR, r24
                            ldi     r24, 0x12       ;PINDIV | PLLE (PLL 1:2, PLL enable)
                            out     PLLCSR, r24
pll_wait_locked:
                            in      r0, PLLCSR
                            sbrs    r0, 0           ;PLOCK
                            rjmp    pll_wait_locked ;wait for PLL locked
                            ret
;-------------------------------------------------------------------------------
USB_USBTask:
                            tst     r10                     ;GPIOR0_test
                            breq    USB_USBTask_ret         ;ret, zero

                            lds     r24, UENUM
                            push    r24                     ;save USB endpoint
                            sts     UENUM, r1
                            rcall   UEINTX_get
                            sbrs    r24, RXSTPI
                            rjmp    USB_USB_USBTask_restore ;clear, no process

                            rcall   USB_Device_ProcessControlRequest
USB_USB_USBTask_restore:
                            pop     r24                     ;restore USB endpoint
                            ;rjmp   UENUM_set_and_07
;-------------------------------------------------------------------------------
UENUM_set_and_07:
                            andi    r24, 0x07
                            ;rjmp   UENUM_set
;-------------------------------------------------------------------------------
UENUM_set:
                            sts     UENUM, r24
USB_USBTask_ret:            ret
;-------------------------------------------------------------------------------
UDINT_clr_bit0:             ldi     r24, 0xFE
;-------------------------------------------------------------------------------
UDINT_clr_bit:              lds     r0, UDINT
                            and     r0, r24
                            sts     UDINT, r0
                            ret
;-------------------------------------------------------------------------------
;EEPROM code
;-------------------------------------------------------------------------------
eeprom_prep:
                            movw    r30, r4             ;get current address in Z
eeprom_wait:
                            sbic    EECR, EEPE
                            rjmp    eeprom_wait

                            out     EEARH, r31          ;write address
                            out     EEARL, r30
                            ret
;-------------------------------------------------------------------------------
eeprom_read:
                            rcall   eeprom_prep         ;wait and select address
                            sbi     EECR, EERE
                            in      r24, EEDR           ;read eeprom byte
                            rjmp    eeprom_addr_inc
;-------------------------------------------------------------------------------
eeprom_write:
                            rcall   eeprom_prep         ;wait and select address
                            out     EECR, r1
                            out     EEDR, r24
                            in      r0, SREG
                            cli
                            sbi     EECR, EEMPE
                            sbi     EECR, EEPE
                            out     SREG, r0
eeprom_addr_inc:
                            adiw    r30, 1              ;current address++
                            movw    r4, r30             ;update current address
                            ret
;-------------------------------------------------------------------------------
TestApplicationFlash:

;returns r24:25 = 0000 and Z flag set if unprogrammed application flash (FFFF)

                            ldi     r30, lo8(APPLICATION_START_ADDR)
                            ldi     r31, hi8(APPLICATION_START_ADDR)
                            lpm     r24, Z+
                            lpm     r25, Z
                            adiw    r24, 1
                            ret
;-------------------------------------------------------------------------------
SetupHardware_bootloader:
                       ;    ;pull display out of reset and activate flash CART CS
                       ;
                       ;#if defined (ARDUBOY_PROMICRO)
                       ;  #ifdef CART_CS_SDA
                       ;    ldi     r24,  (1 << TX_LED) | (1 << RGB_G) ;Command mode, Tx LED off, RGB green off, Flash cart active
                       ;    out     PORTD, r24
                       ;  #else
                       ;   #if defined (MICROCADE)
                       ;    ldi     r24, (1 << OLED_RST) | (1 << TX_LED) | (0 << RGB_G)  | (1 << OLED_CS) ;RST inactive, OLED CS inactive, Command mode, Tx LED off, RGB green on, Flash cart active
                       ;   #else
                       ;    ldi     r24, (1 << OLED_RST) | (1 << TX_LED) | (1 << RGB_G)  | (1 << OLED_CS) ;RST inactive, OLED CS inactive, Command mode, Tx LED off, RGB green off, Flash cart active
                       ;   #endif
                       ;    out     PORTD, r24
                       ;  #endif
                       ;#elif defined (ARDUBIGBOY) || (ARDUBOYMINI)
                       ;    cbi     PORTE, CART_CS      ;enable SPI flash cart
                       ;    sbi     PORTD, OLED_RST     ;RST inactive
                       ;#else
                       ;   #if defined (DS3231) || defined (RV3028)
                       ;    ldi     r24, (1 << OLED_RST) | (1 << TX_LED)  | (1 << OLED_CS) | (1<< CART_CS) | (1 << I2C_SDA) | (1 << I2C_SCL) ;RST inactive, OLED CS inactive, Command mode, Tx LED off, Flash cart inactive, I2C inactive
                       ;   #else
                       ;    ldi     r24, (1 << OLED_RST) | (1 << TX_LED)  | (1 << OLED_CS) | (1<< CART_CS) ;RST inactive, OLED CS inactive, Command mode, Tx LED off, Flash cart inactive
                       ;   #endif
                       ;    out     PORTD, r24
                       ;#endif

                            ;release SPI flash from power down

                            ldi     r24, SFC_RELEASE_POWERDOWN
                            rcall   SPI_flash_cmd_deselect

                            sts     OCR1AH, r1
                            ldi     r24, 0xFA           ;for 1 millisec (PCLK/64/1000)
                            sts     OCR1AL, r24
                            ldi     r24, 0x02           ;OCIE1A
                            sts     TIMSK1, r24         ;enable timer 1 output compare A match interrupt
                            ldi     r24, 0x03           ;CS11 | CS10 1/64 prescaler on timer 1 input
                            sts     TCCR1B, r24

                            ;clear all display ram for SSD132X displays

                           #if defined (OLED_SSD132X_96X96) || (OLED_SSD132X_128X96) || (OLED_SSD132X_128X128)
                            rcall   Display                     ;also selects data mode
                            rcall   Display
                            cbi     PORTD, OLED_DC              ;select command mode
                           #endif

                            ;Setup display

                            ldi     r30,lo8(DisplaySetupData)
                            ldi     r31,hi8(DisplaySetupData)
SetupHardware_display:      lpm     r24, z+
                            rcall   SPI_transfer
                            cpi     r30, lo8(DisplaySetupData_End)
                            brne    SetupHardware_display
                            ;ret
;-------------------------------------------------------------------------------
USB_Init:
                            ldi     r24, (1 << UVREGE)      ;enable USB pad regulator
                            sts     UHWCON, r24
                            ldi     r24, 0x4A
                            out     PLLFRQ, r24

                            ;USB_INT_DisableAllInterrupts

                            sts     USBCON, r1              ;clear VBUSTE
                            sts     UDIEN, r1

                            ;USB_INT_ClearAllInterrupts

                            sts     USBINT, r1              ;clear VBUSTI
                            sts     UDINT, r1               ;clear USB device interrupts
                            ldi     r24, 0x80               ;USBE
                            rcall   USBCON_set
                            out     PLLCSR, r1
                            clr     r10                     ;GPIOR0
                            ;sts     USB_Device_ConfigurationNumber, r1     ;(cleared by BSS init)
                            sts     UDCON, r1                               ;full speed
                            rcall   Endpoint_ConfigureEndpoint_Prv_00_00_02
                            ldi     r24, (1 << EORSTE) | (1 << SUSPE)
                            rcall   UDIEN_set
                            ldi     r24, 0x91
USBCON_set:
                            sts     USBCON, r24
                            ret
;-------------------------------------------------------------------------------
;SPI FLASH
;-------------------------------------------------------------------------------
SPI_read_page_FlashBuffer:
                            ldi     r26, lo8(FlashBuffer)
                            ldi     r27, hi8(FlashBuffer)
                            ;rjmp   SPI_read_page
;-------------------------------------------------------------------------------
SPI_read_page:

;entry: x = buffer
;uses:  r24, r25, x
                            ldi     r25, SPM_PAGESIZE
SPI_read_page_loop:
                            rcall   SPI_transfer
                            st      x+, r24
                            dec     r25
                            brne    SPI_read_page_loop
                            ret
;-------------------------------------------------------------------------------
SPI_write_enable:
                            ldi     r24, SFC_WRITE_ENABLE
                            ;rjmp   SPI_flash_cmd_deselect
;-------------------------------------------------------------------------------
SPI_flash_cmd_deselect:
                            rcall   SPI_flash_cmd
                            rjmp    SPI_flash_deselect
;-------------------------------------------------------------------------------
SPI_flash_read_addr:
                            ldi     r24, SFC_READ_DATA
                            ;rjmp   SPI_flash_cmd_addr
;-------------------------------------------------------------------------------
SPI_flash_cmd_addr:

;Send SPI command and sets flash sector address
;
;entry:
;        r24 = SPI command
;        Z   = page address
;uses:
;        r24, r25
                            rcall   SPI_flash_cmd       ;select SPI flash and send command
                            mov     r24, r31
                            rcall   SPI_transfer        ;address bits 23-16
                            mov     r24, r30
                            rcall   SPI_transfer
                            ldi     r24, 0x00           ;address bits 7-0
                            ;rjmp    SPI_transfer
;-------------------------------------------------------------------------------
SPI_flash_cmd:
                        #if !(defined (CART_CS_SDA) && defined(ARDUBOY_PROMICRO))
                            sbi     PORTD, OLED_CS      ;disable display
                        #endif
                        #if defined (ARDUBIGBOY) || (ARDUBOYMINI)
                            cbi     PORTE, CART_CS      ;enable SPI flash cart
                        #else
                            cbi     PORTD, CART_CS      ;enable SPI flash cart
                        #endif
                            rjmp    SPI_transfer        ;send command
;-------------------------------------------------------------------------------
SPI_flash_wait:
                            rcall   SPI_flash_deselect
                            ldi     r24, SFC_READ_STATUS1
                            rcall   SPI_flash_cmd
SPI_flash_wait_2:           rcall   SPI_transfer        ;read status reg
                            sbrc    r24, 0              ;test busy bit
                            rjmp    SPI_flash_wait_2
                            ;rjmp   SPI_flash_deselect
;-------------------------------------------------------------------------------
SPI_flash_deselect:
                        #if defined(ARDUBIGBOY) || (ARDUBOYMINI)
                            sbi     PORTE, CART_CS      ;deselect SPI flash cart to complete command
                        #else
                            sbi     PORTD, CART_CS      ;deselect SPI flash cart to complete command
                        #endif
                        #if !(defined (CART_CS_SDA) && defined(ARDUBOY_PROMICRO))
                            cbi     PORTD, OLED_CS      ;select display
                        #endif
                            ret
;-------------------------------------------------------------------------------
SPI_transfer:
                            out     SPDR, r24
                            ;nop
SPI_wait:
                            in      r24, SPSR
                            sbrs    r24, SPIF
                            rjmp    SPI_wait
                            in      r24, SPDR
                            ret
;-------------------------------------------------------------------------------
SPI_flash_deselect_display_wait:
                            ldi     r24, 0x0F   ;every 16 frames
                            and     r24, r2
                            brne    SPI_flash_deselect_display_wait
                            ;rjmp   SPI_flash_deselect_display
;-------------------------------------------------------------------------------
SPI_flash_deselect_display:
                            rcall   SPI_flash_deselect
                            ;rjmp   Display
;-------------------------------------------------------------------------------
Display:
;-------------------------------------------------------------------------------

;copies display buffer to OLED display using page mode (supported on most displays)

;                       Uses:
;                           r20, r21, r22, r23, r24, r25, r30, r31 (4-bit displays)
;                           r24, r25, r30, r31                     (1-bit displays)
                        #if defined(OLED_SSD132X_96X96)
                            ldi     r30, lo8(DisplayBuffer + 16)
                            ldi     r31, hi8(DisplayBuffer + 16)
                        #else
                            ldi     r30, lo8(DisplayBuffer)
                            ldi     r31, hi8(DisplayBuffer)
                        #endif
                        #if defined(OLED_SSD132X_96X96) || (OLED_SSD132X_128X96) || (OLED_SSD132X_128X128)
                            sbi     PORTD, OLED_DC          ;ensure Data mode
                          #if defined(OLED_SSD132X_96X96)
                            ldi     r20, 96 / 2             ;visible width
                          #else
                            ldi     r20, WIDTH / 2
                          #endif
Display_column:
                            ldi     r21, HEIGHT / 8
Display_row:
                            ld      r22, z
                            ldd     r23, z+1
                            ldi     r25, 8
Display_shift:
                            ldi     r24, 0xff       ;expand 1 bit to MSB 4 bits
                            sbrs    r22, 0
                            andi    r24, 0x0f
                            sbrs    r23, 0          ;expand 1 bit to LSB 4 bits
                            andi    r24, 0xf0
                            rcall   SPI_transfer
                            lsr     r22
                            lsr     r23
                            dec     r25
                            brne    Display_shift

                            subi     r30, lo8(-WIDTH)   ;add WIDTH
                            sbci     r31, hi8(-WIDTH)
                            dec      r21
                            brne     Display_row

                            subi     r30, lo8(HEIGHT / 8 * WIDTH - 2)
                            sbci     r31, hi8(HEIGHT / 8 * WIDTH - 2)
                            dec      r20
                            brne     Display_column
                        #else
                            ldi     r25, OLED_SET_PAGE_ADDR
Display_l1:
                            cbi     PORTD, OLED_DC                  ;Command mode
                            mov     r24, r25
                            rcall   SPI_transfer                    ;select page
                            ldi     r24, OLED_SET_COLUMN_ADDR_HI
                            rcall   SPI_transfer                    ;select column hi nibble
                            sbi     PORTD, OLED_DC                  ;Data mode
Display_l2:
                            ld      r24, Z+
                            rcall   SPI_transfer
                            ldi     r24, lo8(DisplayBuffer)
                            eor     r24, r30
                            andi    r24, 0x7F                       ;every 128 zero
                            brne    Display_l2

                            inc     r25
                            cpi     r25, OLED_SET_PAGE_ADDR + 8
                            brne    Display_l1
                        #endif
                            ret
;-------------------------------------------------------------------------------
SelectGame:

;Note: select game may only be called after SelectList or successful LoadApplicationInfo

                            ldi     r30, lo8(FlashBuffer)
                            ldi     r31, hi8(FlashBuffer)
                            sbrc    r9, UP_BUTTON
                            rjmp    SelectGame_up
                            sbrc    r9, DOWN_BUTTON
SelectGame_down_jmp:        rjmp    SelectGame_down
                            sbrs    r9, AB_BUTTON           ;skip if A or B pressed to flash application
SelectGame_ret:             ret

                            ;test if there is an application to flash

                            ldd     r9, z+FBO_APPSIZE       ;application length in 128 byte pages
                            tst     r9                      ;test zero length
                            breq    SelectGame_down_jmp     ;nothing to burn, simulate DOWN press

                            ;size > 0, flash application
SelectGame_burn:
                            cli                                     ;no ints wanted
                            ldd     r28, z+FBO_APPPAGE_LSB          ;flash cart application page address
                            ldd     r29, z+FBO_APPPAGE_MSB
                            ldi     r18, lo8(APPLICATION_START_ADDR)
                            ldi     r19, hi8(APPLICATION_START_ADDR)

                            ;get progressbar step increment in 8.8 fixed point

                            ldi     r24, 0                  ;PROGRESSBAR_STEPS in 8.8 fixed point
                            ldi     r25, PROGRESSBAR_STEPS
                            ldi     r26, 0xFF               ;8.8 division result.
                            ldi     r27, 0xFF               ;start with  -1 to compensate for next addition
progress_div:
                            adiw    r26, 1
                            sub     r24, r9                 ;- application size
                            sbci    r25, 0
                            brcc    progress_div
                            movw    r4, r26                 ;progress step increment = steps / application size  in 8.8 fixed point

                            sbci    r27, lo8(-(PROGRESSBAR_START))  ;add lsb display addr to progressbar position
                            movw    r16, r26                        ;progressbar position in 8.8 format
FlashApp_loop:
                            sbrc    r18, 7                          ;test even 128 byte page
                            rjmp    FlashApp_load

                            ;even page, draw progress bar and select flash address

                            ;draw progress bar

                            ldi     r30, lo8(PROGRESSBAR_POS)
                            ldi     r31, hi8(PROGRESSBAR_POS)
                            st      z+, r1                      ;black outer line
                            ldi     r24,0x7E                    ;white inner line
                            st      z+, r24
                            ldi     r25,0x42                    ;top and botom pixels, empty progress bar
progressbar_loop:
                            cp      r30, r17                    ;progress display position lsb
                            st      z+, r25
                            ldi     r25,0x5A                    ;fill progress bar
                            brcs    progressbar_fill
                            ldi     r25,0x42                    ;progress bar empty
progressbar_fill:
                            cpi     r30, lo8(PROGRESSBAR_END)   ;end of progress bar
                            brne    progressbar_loop
                            st      z+, r24                     ;white inner line
                            st      z, r1                       ;black outer line
                            rcall   SPI_flash_deselect_display

                            ;select flash cart address

                            movw    r30, r28                    ;get cart flash page
                            adiw    r28, 1                      ;post inc flash cart page
                            rcall   SPI_flash_read_addr

                            ;load 128 byte page data from flash cart
FlashApp_load:
                            rcall   SPI_read_page_FlashBuffer

                            ;program page

                            movw    r30, r18                    ;PROGMEM address
                            rcall   FlashPage_pageBuffer
                            subi    r18, lo8(-(SPM_PAGESIZE))   ;App addr += PAGESIZE
                            sbci    r19, hi8(-(SPM_PAGESIZE))
                            add     r16, r4                     ;progressbar position += step
                            adc     r17, r5
                            dec     r9                          ;App size --
                            brne    FlashApp_loop
                            rjmp    StartSketch

SelectGame_up:              ;select previous game in list

                            movw    r16, r6                             ;save current selected game
                            mov     r18, r8
SelectGame_prev:
                            lds     r6, FlashBuffer+FBO_PREVSLOT_LSB
                            lds     r7, FlashBuffer+FBO_PREVSLOT_MSB
                            rcall   LoadApplicationInfo
                            brne    SelectGame_last                     ;no previous game found
                            tst     r24                                 ;test found game in list
                            brne    SelectGame_prev                     ;not on list, try previous
                            ret                                         ;return found

                            ;no previous game found, wrap to last
SelectGame_last:
                            movw    r6, r16                             ;last selected game page addr
                            clr     r8                                  ;use bootloader list to prevent loading of title screens
SelectGame_last_b1:
                            rcall   LoadApplicationInfo
                            brne    SelectList_eof                      ;end of storage, select last found game

                            lds     r24, FlashBuffer+FBO_LIST
                            cp      r24, r18                            ;test member of wanted game list
                            brne    SelectGame_last_b2                  ;loop if not

                            movw    r16, r6                             ;update last game page addr
SelectGame_last_b2:
                            lds     r6, FlashBuffer+FBO_NEXTSLOT_LSB
                            lds     r7, FlashBuffer+FBO_NEXTSLOT_MSB
                            rjmp    SelectGame_last_b1

SelectGame_down:            ;select next game in list

                            ldi     r24, 1 << DOWN_BUTTON               ; signal DOWN pressed for scroll
                            mov     r9, r24                             ; required because A+B on category screen jumps here too

                            lds     r6, FlashBuffer+FBO_NEXTSLOT_LSB
                            lds     r7, FlashBuffer+FBO_NEXTSLOT_MSB
                            rcall   LoadApplicationInfo
                            brne    SelectGame_first                    ;end of storage, find first in list
                            tst     r24
                            brne    SelectGame_down                     ;different list, try again
                            ret
;-------------------------------------------------------------------------------
SelectList:
                            ;r9 = buttons

                            ldi     r25, -1
                            sbrc    r9, LEFT_BUTTON
                            rjmp    SelectList_prev
                            sbrs    r9, RIGHT_BUTTON
SelectList_ret:             ret                         ;return no left or right or A button

                            ;next list
SelectList_next:
                            ldi     r25, 1
SelectList_prev:
                            bst     r25, 7              ;save direction in T flag
                            add     r8, r25             ;get new list
SelectGame_first:
                            clr     r6                  ;search from beginning of cart
                            clr     r7
                            movw    r16, r6             ;reset nearest list page
                            clr     r18                 ;reset nearest list nr
SelectList_loop:
                            rcall   LoadApplicationInfo
                            brne    SelectList_eof     ;end of file storage
                            tst     r24
                            breq    SelectList_ret     ;return list found

                            ;not found, check if nearest

                            lds     r24, FlashBuffer+FBO_LIST  ;get list
                            brts    SelectList_below

                            ;get nearest list above
SelectList_above:
                            cp      r8, r24
                            brcc    SelectList_cont    ;not above
                            cp      r24, r18            ;lowest above
                            brcs    SelectList_nearest
                            cpi      r18, 1             ;lowest can't be zero
                            rjmp    SelectList_check

                            ;get nearest list below

SelectList_below:           cp      r24, r8
                            brcc    SelectList_cont    ;not below
                            cp      r18, r24           ;highest below
SelectList_check:
                            brcc    SelectList_cont    ;not new nearest

                            ;update  nearest
SelectList_nearest:
                            movw    r16, r6             ;update nearest list page
                            mov     r18, r24            ;update nearest list nr

SelectList_cont:           ;try next

                            lds     r6, FlashBuffer+FBO_NEXTSLOT_LSB    ;next application slot page
                            lds     r7, FlashBuffer+FBO_NEXTSLOT_MSB
                            rjmp    SelectList_loop                    ;loop different list

SelectList_eof:            ;list not found, set nearest

                            movw    r6, r16             ;select nearest list
                            mov     r8, r18

                            ;rjmp   LoadApplicationInfo
;-------------------------------------------------------------------------------
LoadApplicationInfo:

;entry:
;   r6,r7 = flash cart page address
;   r8    = list
;exit:
;   Z-flag clear: No application info found (end of storage)
;   Z-flag set:   Application info found. r24 = load status:
;                 r24 > 0: Application info not loaded (not a member of selected list)
;                 r24 = 0: Application info loaded

                            movw    r30, r6
                            rcall   SPI_flash_read_addr
                            rcall   SPI_read_page_FlashBuffer

                            ;check arduboy magic key

                            ldi     r28, lo8(FlashBuffer)        ;(using y to preserve x)
                            ldi     r29, hi8(FlashBuffer)
                            ldi     r30, lo8(SOFTWARE_IDENTIFIER)
                            ldi     r31, hi8(SOFTWARE_IDENTIFIER)
LoadAppInfo_CheckKey:
                            ld      r0, y+
                            lpm     r24, z+
                            cp      r0, r24
                            brne    LoadAppInfo_Fail        ;Z flag cleared: End of storage
                            cpi     r30, lo8(SOFTWARE_IDENTIFIER + 7)
                            brne    LoadAppInfo_CheckKey

                            ;key ok, check list

                            ld      r0, y
                            cpse    r8, r0                  ;test appt list = current list
LoadAppInfo_Fail:           rjmp    SPI_flash_deselect      ;Z flag set from cpi, r24 > 0

                            ;load remaining application info + title screen
LoadAppInfo_Load:
                            ldi     r28, 1 + 8              ;remaining header + titlescreen
LoadAppInfo_Page:
                            rcall   SPI_read_page
                            dec     r28
                            brne    LoadAppInfo_Page

                            ;scroll titlescreen onto display

                            ldi     r29, 16             ;frames to scroll up/down
                           #if defined (ARDUBOYMINI)
                            sbrc    r9, LEFT_BUTTON
                            rjmp    scroll_right
                           #else
                            sbrc    r9, UP_BUTTON
                            rjmp    scroll_down
                           #endif
                            sbrc    r9, DOWN_BUTTON
                            rjmp    scroll_up
                           #if defined (ARDUBOYMINI)
                            sbrc    r9, RIGHT_BUTTON
                            rjmp    scroll_left
                            rjmp    scroll_down         ;default: scroll down
                           #else
                            sbrs    r9, RIGHT_BUTTON
                            rjmp    scroll_right        ;default: scroll left to right
                           #endif

                            ;scroll left
scroll_left:
                            ldi     r28, lo8(ScrollBuffer)
                            ldi     r29, hi8(ScrollBuffer)
scroll_left_frame:
                            ldi     r30, lo8(DisplayBuffer)
                            ldi     r31, hi8(DisplayBuffer)

                            ldi     r25, HEIGHT / 8
scroll_left_b1:
                            ldi     r24, WIDTH - HSCROLL_STEP
                            add     r24, r30
scroll_left_b2:
                            ldd     r0, z + HSCROLL_STEP
                            st      z+, r0
                            cp      r30, r24
                            brne    scroll_left_b2
                            sbci    r24, -HSCROLL_STEP
scroll_left_b3:
                            ld      r0, y+
                            st      z+, r0
                            cp      r30, r24
                            brne    scroll_left_b3
                            subi    r28, lo8(-(WIDTH - HSCROLL_STEP))
                            sbci    r29, hi8(-(WIDTH - HSCROLL_STEP))
                            dec     r25
                            brne    scroll_left_b1

                            rcall   SPI_flash_deselect_display_wait
                            subi    r28, lo8(WIDTH * HEIGHT /8 - HSCROLL_STEP)
                            sbci    r29, hi8(WIDTH * HEIGHT /8 - HSCROLL_STEP)
                            cpi     r28, lo8(ScrollBuffer + WIDTH)
                            brne    scroll_left_frame
                            rjmp    LoadAppInfo_end

                            ;scroll right
scroll_right:
                            ldi     r28, lo8(ScrollBuffer + WIDTH * HEIGHT / 8)
                            ldi     r29, hi8(ScrollBuffer + WIDTH * HEIGHT / 8)
scroll_right_frame:
                            ldi     r30, lo8(DisplayBuffer + WIDTH * HEIGHT / 8 - HSCROLL_STEP)
                            ldi     r31, hi8(DisplayBuffer + WIDTH * HEIGHT / 8 - HSCROLL_STEP)
                            ldi     r25, HEIGHT / 8
scroll_right_b1:
                            mov     r24, r30
                            subi    r24, WIDTH - HSCROLL_STEP
scroll_right_b2:
                            ld      r0, -z
                            std     z + HSCROLL_STEP, r0
                            cp      r30, r24
                            brne    scroll_right_b2
                            adiw    r30, HSCROLL_STEP
scroll_right_b3:
                            ld      r0, -y
                            st      -z, r0
                            cp      r30, r24
                            brne    scroll_right_b3

                            sbiw    r30, HSCROLL_STEP
                            subi    r28, lo8(WIDTH - HSCROLL_STEP)
                            sbci    r29, hi8(WIDTH - HSCROLL_STEP)
                            dec     r25
                            brne    scroll_right_b1

                            rcall   SPI_flash_deselect_display_wait
                            subi    r28, lo8(-(WIDTH * HEIGHT / 8 - HSCROLL_STEP )) ;Y += WIDTH * HEIGHT /8 - HSCROLL_STEP
                            sbci    r29, hi8(-(WIDTH * HEIGHT / 8 - HSCROLL_STEP ))
                            cpi     r28, lo8(ScrollBuffer + WIDTH * HEIGHT / 8 - WIDTH)
                            brne    scroll_right_frame
                            rjmp    LoadAppInfo_end

                            ;scroll up 4 pixels
scroll_up:
scroll_up_frame:
                            ldi     r26, lo8(DisplayBuffer + WIDTH)
                            ldi     r27, hi8(DisplayBuffer + WIDTH)
                            ldi     r30, lo8(DisplayBuffer)
                            ldi     r31, hi8(DisplayBuffer)
                            ldi     r25, hi8(DisplayBuffer + WIDTH * HEIGHT / 8)
                            ldi     r28, hi8(ScrollBuffer + WIDTH * HEIGHT / 8)
scroll_up_b1:
                            ld      r0, x+
                            ld      r24, z              ;high nibble / bottom 4 pixels upper row
                            eor     r24, r0             ;xor low nibble / top 4 pixels lower row
                            andi    r24, 0xF0           ;mask off low nibble
                            eor     r24, r0             ;restore high nibble, xor in low nibble
                            swap    r24                 ;swap into display order
                            st      z+, r24
                            cpi     r26, lo8(DisplayBuffer + WIDTH * HEIGHT / 8)
                            cpc     r27, r25
                            brne    scroll_up_b2        ;not dealing with bottom page

                            ;bottom page needs data from scroll buffer

                            ldi     r27, hi8(ScrollBuffer) ;set only msb, lsb is same
scroll_up_b2:
                            cpi     r30, lo8(DisplayBuffer + WIDTH * HEIGHT / 8)
                            cpc     r31, r25
                            brne    scroll_up_b3        ;not and end of display buffer

                            ;display buffer done, scroll scroll buffer also

                            ldi     r31, hi8(ScrollBuffer)  ;set only msb, lsb is same
scroll_up_b3:
                            cpi     r30, lo8(ScrollBuffer + WIDTH * HEIGHT / 8)
                            cpc     r31, r28
                            brne    scroll_up_b1        ;loop until both buffers scrolled

                            rcall   SPI_flash_deselect_display_wait
                            dec     r29
                            brne    scroll_up_frame     ;loop to scroll next frame
                            rjmp    LoadAppInfo_end

                            ;scroll down
scroll_down:
scroll_down_frame:
                            ldi     r26, lo8(DisplayBuffer + WIDTH * HEIGHT /8 - WIDTH)
                            ldi     r27, hi8(DisplayBuffer + WIDTH * HEIGHT /8 - WIDTH)
                            ldi     r30, lo8(DisplayBuffer + WIDTH * HEIGHT /8)
                            ldi     r31, hi8(DisplayBuffer + WIDTH * HEIGHT /8)
                            ldi     r25, hi8(ScrollBuffer)
scroll_down_b1:
                            ld      r0, -x
                            ld      r24, -z             ;low nibble / top 4 pixels lower row
                            eor     r24, r0             ;xor high nibble / lower 4 pixels upper row
                            andi    r24, 0x0F           ;mask off low nibble
                            eor     r24, r0             ;restore high nibble, xor in low nibble
                            swap    r24                 ;swap into display order
                            st      z, r24
                            cpi     r30, lo8(ScrollBuffer)
                            cpc     r31, r25
                            brne    scroll_down_b1

                            rcall   SPI_flash_deselect_display_wait
                            dec     r29
                            brne    scroll_down_frame   ;loop to scroll next frame
LoadAppInfo_end:
                            clr     r24                 ;Z flag set , R24 = 0 signal app info loaded

                            ret

;-------------------------------------------------------------------------------

                          #if defined (SUPPORT_POWERDOWN)
powerdown:
                            ldi     r24, (1 << ACD)             ;Disable Analog Comparator power
                            out     ACSR, r24
                            ldi     r24, (1 << PRTWI) | (1 << PRTIM0) | (1 << PRTIM1) | (1 << PRSPI) | (1 << PRADC) ;enable all power reductionss
                            sts     (PRR0),r24
                            ldi     r24, (1 << PRUSB) | (1 << PRTIM4)| (1 << PRTIM3) | (1 << PRUSART1)
                            sts     (PRR1), r24
                            ldi     r24, (1 << SM1) | (1 << SE) ;power down mode, enable sleep mode
                            out     SMCR, r24
                            sleep
                          #endif

;-------------------------------------------------------------------------------
;i2c subs
                        #if defined (DS3231) || defined (RV3028)
i2c_enable_start:
                            ldi     r30, TWBR   ;Use Z as I2C base register
                            ldi     r31, 0
                            ldd     r24, Z+1    ;get TWSR
                            andi    r24, 0xFC   ;I2C prescaler to 1
                            std     Z+1, r24    ;set TWSR
                            ldi     r24, 12     ;400KHz bitrate
                            st      Z, r24      ;set TWBR
                            ldi     r23, 4      ;3..4 frames timeout
                            add     r23, r2     ;add frame counter
                            
                            ;rjmp   i2c_start

                        ;i2c_start

;(Note Z must point to TWBR when any of the subs below are called, r23 holds timeout value)

i2c_start:
                            ldi     r24, (1 << TWINT) | (1 << TWSTA) | (1 << TWEN)
                            rjmp    i2c_cmd
i2c_write:
                            std     z+3, r24    ;TWDR = r24
                            ;rjmp   i2c_nack
i2c_nack:
                            ldi     r24, (1 << TWINT) | (1 << TWEN)
                            ;rjmp   i2c_cmd
i2c_cmd:
                            std     z+4, r24    ;TWCR = r24
                            ;rjmp   i2c_wait
;i2c_wait:
;                            ldi     r23, 2      ;1..2 frames timeout
;                            add     r23, r2     ;add frame counter
i2c_wait_loop:
                            ldd     r24, z+4    ;TWCR
                            sbrc    r24, TWINT  ;skip if still busy
                            ret
                            cpse    r23, r2     ;skip and return if timeout
                            rjmp    i2c_wait_loop
                            ret
i2c_stop:
                            rcall   i2c_2us_delay   ;stop setup time
                            ldi     r24, (1 << TWINT) | (1 << TWSTO) | (1 << TWEN)
                            std     Z+4, r24        ;TWCR
                            ;rjmp   i2c_delay       ;setup time for next start or disable
i2c_2us_delay:
                            ldi     r24, 8     ;~2 usec delay (including call + ret)
                            dec     r24
                            brne    .-4
                            ret

                           #endif
;-------------------------------------------------------------------------------
;PROGMEM data
;-------------------------------------------------------------------------------

                            ;Software ID string - (7 characters)

SOFTWARE_IDENTIFIER:
                            .ascii  "ARDUBOY"

                            ;OLED display initialization data
DisplaySetupData:
                        #if defined(OLED_SSD132X_96X96) || (OLED_SSD132X_128X96) || (OLED_SSD132X_128X128)
                          #if defined(OLED_SSD132X_96X96)
                            .byte   0x15, 0x10, 0x3f        ;Set column start and end address  skipping left most 32 pixels
                          #else
                            .byte   0x15, 0x00, 0x3f        ;Set column start and end address full width
                          #endif
                          #if defined (OLED_SSD132X_96X96)
                            .byte   0x75, 0x30, 0x6f        ;Set row start and end address
                          #elif defined (OLED_SSD132X_128X96)
                            .byte   0x75, 0x10, 0x4f
                          #else
                            .byte   0x75, 0x20, 0x5f
                          #endif
                            .byte   0xA0, 0x55              ;set re-map: split odd-even COM signals|COM remap|vertical address increment|column address remap
                            .byte   0xA1, 0x00              ;set display start line
                            .byte   0xA2, 0x00              ;set display offset
                            .byte   0xA8, 0x7F              ;Set MUX ratio
                            .byte   0x81, 0xCF              ;Set contrast
                            .byte   0xB1, 0x21              ;reset and 1st precharge phase length
                            .byte   OLED_SET_DISPLAY_ON     ;display on
                        #elif defined (LCD_ST7565)
                            .byte   0xC8                    ;SET_COM_REVERSE
                            .byte   0x28 | 0x7              ;SET_POWER_CONTROL  | 0x7
                            .byte   0x20 | 0x5              ;SET_RESISTOR_RATIO | 0x5
                            .byte   0x81                    ;SET_VOLUME_FIRST
                            .byte   0x13                    ;SET_VOLUME_SECOND
                            .byte   0xAF                    ;DISPLAY_ON
                        #else
                            .byte   0xD5, 0xF0              ;Display Clock Divisor
                            .byte   0x8D, 0x14              ;Charge Pump Setting enabled
                            .byte   0xA1                    ;Segment remap
                            .byte   0xC8                    ;COM output scan direction
                            .byte   0x81, 0xCF              ;Set contrast
                            .byte   0xD9, 0xF1              ;set precharge
                            .byte   OLED_SET_DISPLAY_ON     ;display on
                            .byte   OLED_SET_COLUMN_ADDR_LO
                        #endif
DisplaySetupData_End:
                            ;USB boot icon graphics

                           #if !(defined (DS3231) || defined (RV3028))
bootgfx:                    .byte   0x00, 0x00, 0xff, 0xff, 0xcf, 0xcf, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xcf, 0xff, 0xff, 0x00, 0x00
                            .byte   0xfe, 0x06, 0x7e, 0x7e, 0x06, 0xfe, 0x46, 0x56, 0x56, 0x16, 0xfe, 0x06, 0x56, 0x46, 0x1e, 0xfe
                            .byte   0x3f, 0x61, 0xe9, 0xe3, 0xff, 0xe3, 0xeb, 0xe3, 0xff, 0xe3, 0xeb, 0xe3, 0xff, 0xe1, 0x6b, 0x3f
bootgfx_end:
                           #endif
;-------------------------------------------------------------------------------
                            .section .bss   ;zero initialized data
;-------------------------------------------------------------------------------
SECTION_BSS_START:
                                    ;indices for IndexedVars
                                    .equ    IDX_TXLEDPULSE,     0
                                    .equ    IDX_RXLEDPULSE,     1
                                    .equ    IDX_LEDCONTROL,     2
IndexedVars:
TxLEDPulse:                         .byte   0   ;| Note:  Do not change order of these vars
RxLEDPulse:                         .byte   0   ;|
LED_Control:                        .byte   0   ;|

USB_Device_ConfigurationNumber:     .byte   0

LineEncoding:                       ;structure:
                                    .long   0   ;BaudRateBPS
                                    .byte   0   ;CharFormat
                                    .byte   0   ;ParityType
                                    .byte   0   ;DataBits

USB_ControlRequest:                 ;structure:
USB_ControlRequest_bmRequestType:   .byte   0
USB_ControlRequest_brequest:        .byte   0
USB_ControlRequest_wValueL:         .byte   0
USB_ControlRequest_wValueH:         .byte   0
USB_ControlRequest_wIndex:          .word   0
USB_ControlRequest_wLength:         .word   0

FlashBuffer:                        .space  256
                                    .equ    FBO_SIGNATURE,      0   ;u8  "ARDUBOY" signature
                                    .equ    FBO_LIST,           7   ;u8  list/group
                                    .equ    FBO_PREVSLOT,       8   ;u16 page address to previous slot or 0xFFFF for none
                                    .equ    FBO_PREVSLOT_MSB,   8
                                    .equ    FBO_PREVSLOT_LSB,   9
                                    .equ    FBO_NEXTSLOT,       10  ;u16 page address to next slot
                                    .equ    FBO_NEXTSLOT_MSB,   10
                                    .equ    FBO_NEXTSLOT_LSB,   11
                                    .equ    FBO_SLOTSIZE,       12  ;u16 slot size in 256 byte pages
                                    .equ    FBO_SLOTSIZE_MSB,   12
                                    .equ    FBO_SLOTSIZE_LSB,   13
                                    .equ    FBO_APPSIZE,        14  ;u8 application size in 128 byte PROGMEM pages or 0 titlescreen
                                    .equ    FBO_APPPAGE,        15  ;u16 start page of application code
                                    .equ    FBO_APPPAGE_MSB,    15
                                    .equ    FBO_APPPAGE_LSB,    16
ScrollBuffer:                       .space  1024                    ;1K display buffer used for scrolling
DisplayBuffer:                      .space  1024                    ;1K display buffer copied to dispay

                                  #if defined (DS3231) || defined(RV3028)

                                    .equ    LargeDigits,        FlashBuffer + 32        ;Arduboy GT Large digits gfx
                                    .equ    SmallDigits,        LargeDigits + 11 * 14   ;Arduboy GT small digits +space gfx
                                    .equ    AmPmIcon,           SmallDigits + 10 * 5    ;Arduboy GT am/pml icon gfx

                                    .equ    DateTime,   FlashBuffer + 17
                                    .equ    DateTimeSize, SLD_LEN
                                    .equ    DateTimeLast, DateTime + DateTimeSize - 1
                                    
                                    .equ    DigitDataTable, DisplayBuffer - (13 * 2 * 2)
                                    
                                    .equ    IDX_SEC,    0x00
                                    .equ    IDX_MIN,    0x01
                                    .equ    IDX_HOUR,   0x02    ;hour | 12/24 mode(DS3231) | am/pm
                                    .equ    IDX_DAY,    0x03    ;Day of the Week
                                    .equ    IDX_DATE,   0x04    ;Day of the Month
                                    .equ    IDX_MONTH,  0x05
                                    .equ    IDX_YEAR,   0x06

                                    #if defined(DS3231)
                                    .equ    IDX_HOURMODE,  0x02 ;hour register contains 12/24 mode flag
                                    .equ    MSK_HOURMODE,  0x40 ;bit 6 is 12/24 mode flag
                                    #elif defined(RV3028)
                                    .equ    IDX_HOURMODE,  0x10 ;control 2 register contains 12/24 mode flag
                                    .equ    MSK_HOURMODE,  0x02 ;bit 1 is 12/24 mode flag
                                    #endif

                                   #endif
SECTION_BSS_END:
;-------------------------------------------------------------------------------
                            .section .bootsignature, "ax"
;-------------------------------------------------------------------------------

                            .byte   BOOTLOADER_VERSION_MAJOR, BOOTLOADER_VERSION_MINOR

                            rjmp    FlashPage

                            .word   BOOT_SIGNATURE
;===============================================================================