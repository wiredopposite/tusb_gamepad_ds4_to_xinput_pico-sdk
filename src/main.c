#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

#include "tusb.h"
#include "host/usbh.h"
#include "bsp/board_api.h"
#include "pio_usb.h"
#include "tusb_gamepad.h"

// Board defs, values don't matter as long as they're unique
#define PI_PICO 1
#define ADAFRUIT_FEATHER 2

// Choose a board here //
#define RP2040_BOARD ADAFRUIT_FEATHER
// ------------------- //

// Setting D+/D- host pins, DM = DP + 1
#if RP2040_BOARD == PI_PICO
    #define PIO_USB_DP_PIN 0
#elif RP2040_BOARD == ADAFRUIT_FEATHER
    #define PIO_USB_DP_PIN 16
#endif

// define pio config
#define PIO_USB_CONFIG {    \
    PIO_USB_DP_PIN,         \
    PIO_USB_TX_DEFAULT,     \
    PIO_SM_USB_TX_DEFAULT,  \
    PIO_USB_DMA_TX_DEFAULT, \
    PIO_USB_RX_DEFAULT,     \
    PIO_SM_USB_RX_DEFAULT,  \
    PIO_SM_USB_EOP_DEFAULT, \
    NULL,                   \
    PIO_USB_DEBUG_PIN_NONE, \
    PIO_USB_DEBUG_PIN_NONE, \
    false,                  \
    PIO_USB_PINOUT_DPDM }

extern void hid_app_task(void); // see hid_app.c

void usbh_task()
{
    #if RP2040_BOARD == ADAFRUIT_FEATHER // Board needs VCC enabled on the USB host port
        #define VCC_EN_PIN 18
        gpio_init(VCC_EN_PIN);
        gpio_set_dir(VCC_EN_PIN, GPIO_OUT);
        gpio_put(VCC_EN_PIN, 1);
    #endif

    pio_usb_configuration_t pio_cfg = PIO_USB_CONFIG;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tuh_init(BOARD_TUH_RHPORT);

    while (1)
    {
        tuh_task();
        hid_app_task(); // updates gamepad with dualshock 4 data
    }
}

int main(void) 
{
    set_sys_clock_khz(120000, true);

    board_init();

    enum InputMode input_mode = INPUT_MODE_XINPUT; // choose an input mode

    init_tusb_gamepad(input_mode); // initalize usb device with chosen input mode

    multicore_reset_core1();
    multicore_launch_core1(usbh_task); // usb host stack on core 1

    while (1) 
    {
        tusb_gamepad_task(); // send and receive gamepad data

        sleep_ms(1);
        tud_task();
    }

    return 0;
}