#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"

#include "tusb.h"
#include "bsp/board_api.h"
#include "tusb_gamepad.h"

void update_gamepad(Gamepad* gp);

int main(void) 
{
    set_sys_clock_khz(120000, true);

    board_init();

    enum InputMode input_mode = INPUT_MODE_XINPUT;

    init_tusb_gamepad(input_mode);

    Gamepad* gp = gamepad(0);

    while (1) 
    {
        update_gamepad(gp);

        tusb_gamepad_task();

        sleep_ms(1);
        tud_task();
    }

    return 0;
}

void update_gamepad(Gamepad* gp)
{
    // Increment triggers
    if (gp->triggers.l < UINT8_MAX) 
    {
        gp->triggers.l++;
        gp->triggers.r++;
    } 
    else 
    {
        gp->reset_triggers(gp); // Reset triggers to zero
    }

    unsigned long current_time = to_ms_since_boot(get_absolute_time());

    // Change joystick direction every second
    static unsigned long last_joy_time = 0;
    static int current_direction = 0;

    if (current_time - last_joy_time >= 1000) 
    {
        last_joy_time = current_time;

        gp->reset_joysticks(gp); // Reset joysticks to center

        switch (current_direction)
        {
            case 0:
                gp->joysticks.ly = INT16_MAX;
                gp->joysticks.ry = INT16_MAX;
                current_direction++;
                break;
            case 1:
                gp->joysticks.ly = INT16_MAX;
                gp->joysticks.ry = INT16_MAX;
                gp->joysticks.lx = INT16_MAX;
                gp->joysticks.rx = INT16_MAX;
                current_direction++;
                break;
            case 2:
                gp->joysticks.lx = INT16_MAX;
                gp->joysticks.rx = INT16_MAX;
                current_direction++;
                break;
            case 3:
                gp->joysticks.ly = INT16_MIN;
                gp->joysticks.ry = INT16_MIN;
                gp->joysticks.lx = INT16_MAX;
                gp->joysticks.rx = INT16_MAX;
                current_direction++;
                break;
            case 4:
                gp->joysticks.ly = INT16_MIN;
                gp->joysticks.ry = INT16_MIN;
                current_direction++;
                break;
            case 5:
                gp->joysticks.ly = INT16_MIN;
                gp->joysticks.ry = INT16_MIN;
                gp->joysticks.lx = INT16_MIN;
                gp->joysticks.rx = INT16_MIN;                
                current_direction++;
                break;
            case 6:
                gp->joysticks.lx = INT16_MIN;
                gp->joysticks.rx = INT16_MIN;
                current_direction++;
                break;
            case 7:
                gp->joysticks.lx = INT16_MIN;
                gp->joysticks.rx = INT16_MIN;
                gp->joysticks.ly = INT16_MAX;
                gp->joysticks.ry = INT16_MAX;
                current_direction = 0;
                break;
        }
    }


    // Change button press every 500ms
    static unsigned long last_btn_time = 0;
    static int current_button = 0;

    if (current_time - last_btn_time >= 500) 
    {
        last_btn_time = current_time;

        gp->reset_buttons(gp); // Reset all buttons

        switch (current_button)
        {
            case 0:
                gp->buttons.up = true;
                current_button++;
                break;
            case 1:
                gp->buttons.down = true;
                current_button++;
                break;
            case 2:
                gp->buttons.left = true;
                current_button++;
                break;
            case 3:
                gp->buttons.right = true;
                current_button++;
                break;
            case 4:
                gp->buttons.a = true;
                current_button++;
                break;
            case 5:
                gp->buttons.b = true;
                current_button++;
                break;
            case 6:
                gp->buttons.x = true;
                current_button++;
                break;
            case 7:
                gp->buttons.y = true;
                current_button++;
                break;
            case 8:
                gp->buttons.l3 = true;
                current_button++;
                break;
            case 9:
                gp->buttons.r3 = true;
                current_button++;
                break;
            case 10:
                gp->buttons.back = true;
                current_button++;
                break;
            case 11:
                gp->buttons.start = true;
                current_button++;
                break;
            case 12:
                gp->buttons.rb = true;
                current_button++;
                break;
            case 13:
                gp->buttons.lb = true;
                current_button++;
                break;
            case 14:
                gp->buttons.sys = true;
                current_button++;
                break;
            case 15:
                gp->buttons.misc = true;
                current_button = 0;
                break;
        }
    }
}