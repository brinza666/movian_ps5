/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MOVIAN_PS5_DISPLAY_H
#define MOVIAN_PS5_DISPLAY_H

/* Software framebuffer on the PS5 SDL2 port (sceVideoOut behind SDL).
 * All of these must be called from one thread. */

int movian_ps5_display_open(int width, int height);
void movian_ps5_display_close(void);

/* Replace the status page. text is UTF-8, lines split on '\n'. */
void movian_ps5_status(const char *text);

/* Present the status page. Safe to call often. */
void movian_ps5_present(void);

/* One edge-triggered input event.
 * *code is an SDL_CONTROLLER_BUTTON_* value, or 1000+SDL_Scancode,
 * or -1 on SDL_QUIT. Returns 1 if an event was written. */
int movian_ps5_poll(int *code);

#endif
