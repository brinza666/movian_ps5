/*
 * Movian PS5 - Input / Controller Implementation
 * scePad integration for PS5 controller input
 */

#include "paw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__PS5__)
#include <psx/scePad.h>
#endif

static ps5_input_t g_input = {0};

/* Button mappings */
#define PS5_BUTTON_CROSS   0x0001
#define PS5_BUTTON_CIRCLE  0x0002
#define PS5_BUTTON_SQUARE  0x0004
#define PS5_BUTTON_TRIANGLE 0x0008
#define PS5_BUTTON_L1      0x0010
#define PS5_BUTTON_R1      0x0020
#define PS5_BUTTON_L2      0x0040
#define PS5_BUTTON_R2      0x0080
#define PS5_BUTTON_L3      0x0100
#define PS5_BUTTON_R3      0x0200
#define PS5_BUTTON_SHARE   0x0400
#define PS5_BUTTON_OPTIONS 0x0800
#define PS5_BUTTON_PS      0x1000
#define PS5_BUTTON_MUTE    0x2000
#define PS5_BUTTON_DPAD_UP    0x4000
#define PS5_BUTTON_DPAD_DOWN  0x8000
#define PS5_BUTTON_DPAD_LEFT  0x10000
#define PS5_BUTTON_DPAD_RIGHT 0x20000
#define PS5_BUTTON_CREATE    0x40000
#define PS5_BUTTON_TRACKPAD  0x80000

/*
 * Poll controller input
 * Read current state from scePad
 */
int ps5_input_poll(ps5_input_t *input) {
    if (!input) return -1;

    memset(input, 0, sizeof(*input));

#if defined(__PS5__)
    /*
     * Read pad data
     * scePadRead(port, data, buffer_size)
     */
    ScePadData pad_data;
    memset(&pad_data, 0, sizeof(pad_data));

    int result = scePadRead(0, &pad_data, sizeof(pad_data));
    if (result < 0) {
        fprintf(stderr, "[PS5 Input] scePadRead failed: 0x%x\n", result);
        return -1;
    }

    /* Map buttons */
    if (pad_data.buttons & SCE_PAD_BUTTON_CROSS)      input->buttons |= PS5_BUTTON_CROSS;
    if (pad_data.buttons & SCE_PAD_BUTTON_CIRCLE)     input->buttons |= PS5_BUTTON_CIRCLE;
    if (pad_data.buttons & SCE_PAD_BUTTON_SQUARE)     input->buttons |= PS5_BUTTON_SQUARE;
    if (pad_data.buttons & SCE_PAD_BUTTON_TRIANGLE)   input->buttons |= PS5_BUTTON_TRIANGLE;
    if (pad_data.buttons & SCE_PAD_BUTTON_L1)         input->buttons |= PS5_BUTTON_L1;
    if (pad_data.buttons & SCE_PAD_BUTTON_R1)         input->buttons |= PS5_BUTTON_R1;
    if (pad_data.buttons & SCE_PAD_BUTTON_L2)         input->buttons |= PS5_BUTTON_L2;
    if (pad_data.buttons & SCE_PAD_BUTTON_R2)         input->buttons |= PS5_BUTTON_R2;
    if (pad_data.buttons & SCE_PAD_BUTTON_L3)         input->buttons |= PS5_BUTTON_L3;
    if (pad_data.buttons & SCE_PAD_BUTTON_R3)         input->buttons |= PS5_BUTTON_R3;
    if (pad_data.buttons & SCE_PAD_BUTTON_SHARE)      input->buttons |= PS5_BUTTON_SHARE;
    if (pad_data.buttons & SCE_PAD_BUTTON_OPTIONS)    input->buttons |= PS5_BUTTON_OPTIONS;
    if (pad_data.buttons & SCE_PAD_BUTTON_PS)         input->buttons |= PS5_BUTTON_PS;
    if (pad_data.buttons & SCE_PAD_BUTTON_MUTE)       input->buttons |= PS5_BUTTON_MUTE;
    if (pad_data.buttons & SCE_PAD_BUTTON_DPAD_UP)    input->buttons |= PS5_BUTTON_DPAD_UP;
    if (pad_data.buttons & SCE_PAD_BUTTON_DPAD_DOWN)  input->buttons |= PS5_BUTTON_DPAD_DOWN;
    if (pad_data.buttons & SCE_PAD_BUTTON_DPAD_LEFT)  input->buttons |= PS5_BUTTON_DPAD_LEFT;
    if (pad_data.buttons & SCE_PAD_BUTTON_DPAD_RIGHT) input->buttons |= PS5_BUTTON_DPAD_RIGHT;
    if (pad_data.buttons & SCE_PAD_BUTTON_CREATE)     input->buttons |= PS5_BUTTON_CREATE;
    if (pad_data.buttons & SCE_PAD_BUTTON_TRACKPAD)   input->buttons |= PS5_BUTTON_TRACKPAD;

    /* Map analog sticks */
    input->left_stick_x = pad_data.left_stick_x;
    input->left_stick_y = pad_data.left_stick_y;
    input->right_stick_x = pad_data.right_stick_x;
    input->right_stick_y = pad_data.right_stick_y;
    input->left_trigger = pad_data.left_trigger;
    input->right_trigger = pad_data.right_trigger;
#else
    /* Host build - no input */
#endif

    return 0;
}

/* Check if a specific button is pressed */
int ps5_input_button_pressed(ps5_input_t *input, uint32_t button) {
    if (!input) return 0;
    return (input->buttons & button) ? 1 : 0;
}

/* Get global input instance */
ps5_input_t* ps5_input_get_global(void) {
    return &g_input;
}

/* Initialize global input */
int ps5_input_global_init(void) {
#if defined(__PS5__)
    /* Initialize pad subsystem */
    int result = scePadInit();
    if (result < 0) {
        fprintf(stderr, "[PS5 Input] scePadInit failed: 0x%x\n", result);
        return -1;
    }
#endif
    return 0;
}

/* Shutdown global input */
void ps5_input_global_shutdown(void) {
#if defined(__PS5__)
    scePadEnd();
#endif
}
