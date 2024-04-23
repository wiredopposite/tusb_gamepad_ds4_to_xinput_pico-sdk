/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#include "bsp/board_api.h"
#include "tusb.h"
#include "host/usbh.h"
#include "class/hid/hid_host.h"
#include "tusb_gamepad.h"

enum Dualshock4DpadMask
{
    PS4_DPAD_MASK_UP = 0x00,
    PS4_DPAD_MASK_UP_RIGHT = 0x01,
    PS4_DPAD_MASK_RIGHT = 0x02,
    PS4_DPAD_MASK_RIGHT_DOWN = 0x03,
    PS4_DPAD_MASK_DOWN = 0x04,
    PS4_DPAD_MASK_DOWN_LEFT = 0x05,
    PS4_DPAD_MASK_LEFT = 0x06,
    PS4_DPAD_MASK_LEFT_UP = 0x07,
    PS4_DPAD_MASK_NONE = 0x08,
};

// Sony DS4 report layout detail https://www.psdevwiki.com/ps4/DS4-USB
typedef struct TU_ATTR_PACKED
{
    uint8_t lx, ly, rx, ry; // joystick

    struct {
        uint8_t dpad     : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
        uint8_t square   : 1; // west
        uint8_t cross    : 1; // south
        uint8_t circle   : 1; // east
        uint8_t triangle : 1; // north
    };

    struct {
        uint8_t l1     : 1;
        uint8_t r1     : 1;
        uint8_t l2     : 1;
        uint8_t r2     : 1;
        uint8_t share  : 1;
        uint8_t option : 1;
        uint8_t l3     : 1;
        uint8_t r3     : 1;
    };

    struct {
        uint8_t ps      : 1; // playstation button
        uint8_t tpad    : 1; // track pad click
        uint8_t counter : 6; // +1 each report
    };

    uint8_t l2_trigger; // 0 released, 0xff fully pressed
    uint8_t r2_trigger; // as above

    //  uint16_t timestamp;
    //  uint8_t  battery;
    //
    //  int16_t gyro[3];  // x, y, z;
    //  int16_t accel[3]; // x, y, z

    // there is still lots more info

} sony_ds4_report_t;

typedef struct TU_ATTR_PACKED {
    // First 16 bits set what data is pertinent in this structure (1 = set; 0 = not set)
    uint8_t set_rumble : 1;
    uint8_t set_led : 1;
    uint8_t set_led_blink : 1;
    uint8_t set_ext_write : 1;
    uint8_t set_left_volume : 1;
    uint8_t set_right_volume : 1;
    uint8_t set_mic_volume : 1;
    uint8_t set_speaker_volume : 1;
    uint8_t set_flags2;

    uint8_t reserved;

    uint8_t motor_right;
    uint8_t motor_left;

    uint8_t lightbar_red;
    uint8_t lightbar_green;
    uint8_t lightbar_blue;
    uint8_t lightbar_blink_on;
    uint8_t lightbar_blink_off;

    uint8_t ext_data[8];

    uint8_t volume_left;
    uint8_t volume_right;
    uint8_t volume_mic;
    uint8_t volume_speaker;

    uint8_t other[9];
} sony_ds4_output_report_t;

static bool ds4_mounted = false;
static uint8_t ds4_dev_addr = 0;
static uint8_t ds4_instance = 0;
static uint8_t motor_left = 0;
static uint8_t motor_right = 0;

int16_t scale_uint8_to_int16(uint8_t value, bool invert);

// check if device is Sony DualShock 4
static inline bool is_sony_ds4(uint8_t dev_addr)
{
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    return ( (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4)) // Sony DualShock4
            || (vid == 0x0f0d && pid == 0x005e)                 // Hori FC4
            || (vid == 0x0f0d && pid == 0x00ee)                 // Hori PS4 Mini (PS4-099U)
            || (vid == 0x1f4f && pid == 0x1002)                 // ASW GG xrd controller
          );
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

void hid_app_task(void)
{
    if (ds4_mounted)
    {
        const uint32_t interval_ms = 200;
        static uint32_t start_ms = 0;

        uint32_t current_time_ms = board_millis();
        if ( current_time_ms - start_ms >= interval_ms)
        {
            start_ms = current_time_ms;

            sony_ds4_output_report_t output_report = {0};
            output_report.set_rumble = 1;
            output_report.motor_left = motor_left;
            output_report.motor_right = motor_right;
            tuh_hid_send_report(ds4_dev_addr, ds4_instance, 5, &output_report, sizeof(output_report));
        }
    }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    (void)desc_report;
    (void)desc_len;
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
    printf("VID = %04x, PID = %04x\r\n", vid, pid);

    // Sony DualShock 4 [CUH-ZCT2x]
    if ( is_sony_ds4(dev_addr) )
    {
        if (!ds4_mounted)
        {
            ds4_dev_addr = dev_addr;
            ds4_instance = instance;
            motor_left = 0;
            motor_right = 0;
            ds4_mounted = true;
        }

        // request to receive report
        // tuh_hid_report_received_cb() will be invoked when report is available
        if ( !tuh_hid_receive_report(dev_addr, instance) )
        {
            printf("Error: cannot request to receive report\r\n");
        }
    }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
    
    if (ds4_mounted && ds4_dev_addr == dev_addr && ds4_instance == instance)
    {
        ds4_mounted = false;
    }
}

// check if different than 2
bool diff_than_2(uint8_t x, uint8_t y)
{
    return (x - y > 2) || (y - x > 2);
}

// check if 2 reports are different enough
bool diff_report(sony_ds4_report_t const* rpt1, sony_ds4_report_t const* rpt2)
{
    bool result;

    // x, y, z, rz must different than 2 to be counted
    result = diff_than_2(rpt1->ly, rpt2->ly) || diff_than_2(rpt1->lx , rpt2->lx ) ||
            diff_than_2(rpt1->ry, rpt2->ry) || diff_than_2(rpt1->rx, rpt2->rx);

    // check the rest with mem compare
    result |= memcmp(&rpt1->ry + 1, &rpt2->ry + 1, sizeof(sony_ds4_report_t)-6);

    return result;
}

void process_sony_ds4(uint8_t const* report, uint16_t len)
{
    // previous report used to compare for changes
    static sony_ds4_report_t prev_report = { 0 };

    uint8_t const report_id = report[0];
    report++;
    len--;

    // all buttons state is stored in ID 1
    if (report_id == 1)
    {
        sony_ds4_report_t ds4_report;
        memcpy(&ds4_report, report, sizeof(ds4_report));

        // counter is +1, assign to make it easier to compare 2 report
        prev_report.counter = ds4_report.counter;

        Gamepad* gp = gamepad(0);

        if ( diff_report(&prev_report, &ds4_report) )
        {
            gp->reset_pad(gp);

            switch(ds4_report.dpad)
            {
                case PS4_DPAD_MASK_UP:
                    gp->buttons.up = true;
                    break;
                case PS4_DPAD_MASK_UP_RIGHT:
                    gp->buttons.up = true;
                    gp->buttons.right = true;
                    break;
                case PS4_DPAD_MASK_RIGHT:
                    gp->buttons.right = true;
                    break;
                case PS4_DPAD_MASK_RIGHT_DOWN:
                    gp->buttons.right = true;
                    gp->buttons.down = true;
                    break;
                case PS4_DPAD_MASK_DOWN:
                    gp->buttons.down = true;
                    break;
                case PS4_DPAD_MASK_DOWN_LEFT:
                    gp->buttons.down = true;
                    gp->buttons.left = true;
                    break;
                case PS4_DPAD_MASK_LEFT:
                    gp->buttons.left = true;
                    break;
                case PS4_DPAD_MASK_LEFT_UP:
                    gp->buttons.left = true;
                    gp->buttons.up = true;
                    break;
            }

            if (ds4_report.square)   gp->buttons.x = true;
            if (ds4_report.cross)    gp->buttons.a = true;
            if (ds4_report.circle)   gp->buttons.b = true;
            if (ds4_report.triangle) gp->buttons.y = true;

            if (ds4_report.share)    gp->buttons.back  = true;
            if (ds4_report.option)   gp->buttons.start = true;
            if (ds4_report.ps)       gp->buttons.sys   = true;
            if (ds4_report.tpad)     gp->buttons.misc  = true;

            if (ds4_report.l1) gp->buttons.lb = true;
            if (ds4_report.r1) gp->buttons.rb = true;

            if (ds4_report.l3) gp->buttons.l3 = true;
            if (ds4_report.r3) gp->buttons.r3 = true;

            gp->triggers.l = ds4_report.l2_trigger;
            gp->triggers.r = ds4_report.r2_trigger;

            gp->joysticks.lx = scale_uint8_to_int16(ds4_report.lx, false);
            gp->joysticks.ly = scale_uint8_to_int16(ds4_report.ly, true);
            gp->joysticks.rx = scale_uint8_to_int16(ds4_report.rx, false);
            gp->joysticks.ry = scale_uint8_to_int16(ds4_report.ry, true);
        }

        // If host has rumble data, use it. Otherwise use trigger as intensity
        if (gp->rumble.l + gp->rumble.r > 0)
        {
            motor_left = gp->rumble.l;
            motor_right = gp->rumble.r;
        }
        else
        {
            motor_left = ds4_report.l2_trigger;
            motor_right = ds4_report.r2_trigger;
        }

        prev_report = ds4_report;
    }
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    if ( is_sony_ds4(dev_addr) )
    {
        process_sony_ds4(report, len);
    }

    // continue to request to receive report
    if ( !tuh_hid_receive_report(dev_addr, instance) )
    {
        printf("Error: cannot request to receive report\r\n");
    }
}

int16_t scale_uint8_to_int16(uint8_t value, bool invert) 
{
    const uint32_t scaling_factor = UINT16_MAX;
    const int32_t bias = INT16_MIN;

    int32_t scaled_value = ((uint32_t)value * scaling_factor) >> 8;
    scaled_value += bias;

    if (invert)
    {
        scaled_value = -scaled_value - 1;
    }

    if (scaled_value < INT16_MIN)
    {
        scaled_value = INT16_MIN;
    } 
    else if (scaled_value > INT16_MAX) 
    {
        scaled_value = INT16_MAX;
    }

    return (int16_t)scaled_value;
}