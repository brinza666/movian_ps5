/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Status framebuffer for the PS5 SDL2 port.
 * That driver (src/video/ps5 in ps5-payload-dev/SDL) is a software
 * surface flipped with sceVideoOutSubmitFlip. It is not GL. */

#include "ps5_display.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *window;
static int disp_w = 1920;
static int disp_h = 1080;
static char status[4096];
static int stop_requested;

/* 5x7 glyphs, columns packed in bits 4..0 of each row. ASCII 32..90. */
static const unsigned char font_5x7[][7] = {
	{0, 0, 0, 0, 0, 0, 0},                                           /*   */
	{0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},                       /* ! */
	{0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00},                       /* " */
	{0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},                       /* # */
	{0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},                       /* $ */
	{0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},                       /* % */
	{0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D},                       /* & */
	{0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},                       /* ' */
	{0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},                       /* ( */
	{0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},                       /* ) */
	{0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00},                       /* * */
	{0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},                       /* + */
	{0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08},                       /* , */
	{0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},                       /* - */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},                       /* . */
	{0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00},                       /* / */
	{0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},                       /* 0 */
	{0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},                       /* 1 */
	{0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},                       /* 2 */
	{0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},                       /* 3 */
	{0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},                       /* 4 */
	{0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},                       /* 5 */
	{0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},                       /* 6 */
	{0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},                       /* 7 */
	{0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},                       /* 8 */
	{0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},                       /* 9 */
	{0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},                       /* : */
	{0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08},                       /* ; */
	{0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},                       /* < */
	{0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},                       /* = */
	{0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},                       /* > */
	{0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04},                       /* ? */
	{0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E},                       /* @ */
	{0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},                       /* A */
	{0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},                       /* B */
	{0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},                       /* C */
	{0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},                       /* D */
	{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},                       /* E */
	{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},                       /* F */
	{0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},                       /* G */
	{0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},                       /* H */
	{0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},                       /* I */
	{0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},                       /* J */
	{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},                       /* K */
	{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},                       /* L */
	{0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},                       /* M */
	{0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},                       /* N */
	{0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},                       /* O */
	{0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},                       /* P */
	{0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},                       /* Q */
	{0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},                       /* R */
	{0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},                       /* S */
	{0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},                       /* T */
	{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},                       /* U */
	{0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},                       /* V */
	{0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},                       /* W */
	{0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},                       /* X */
	{0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},                       /* Y */
	{0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},                       /* Z */
};

static void putpix(SDL_Surface *s, int x, int y, Uint32 color) {
	if (x < 0 || y < 0 || x >= s->w || y >= s->h) {
		return;
	}
	Uint8 *row = (Uint8 *)s->pixels + (size_t)y * s->pitch;
	Uint32 *px = (Uint32 *)(row + (size_t)x * 4);
	*px = color;
}

static void glyph(SDL_Surface *s, int x, int y, int scale, char ch, Uint32 color) {
	int idx;
	if (ch >= 'a' && ch <= 'z') {
		ch = (char)(ch - 'a' + 'A');
	}
	if (ch < 32 || ch > 'Z') {
		ch = ' ';
	}
	idx = ch - 32;
	for (int row = 0; row < 7; row++) {
		unsigned char bits = font_5x7[idx][row];
		for (int col = 0; col < 5; col++) {
			if (bits & (0x10 >> col)) {
				for (int sy = 0; sy < scale; sy++) {
					for (int sx = 0; sx < scale; sx++) {
						putpix(s, x + col * scale + sx, y + row * scale + sy, color);
					}
				}
			}
		}
	}
}

static void draw_status(SDL_Surface *s) {
	Uint32 bg = SDL_MapRGB(s->format, 0x12, 0x13, 0x0f);
	Uint32 fg = SDL_MapRGB(s->format, 0xe2, 0xa1, 0x5a);
	Uint32 dim = SDL_MapRGB(s->format, 0xb7, 0xc4, 0xa4);
	SDL_FillRect(s, NULL, bg);
	const int scale = s->w >= 1600 ? 3 : 2;
	const int cw = 6 * scale;
	const int ch = 9 * scale;
	int x = 48;
	int y = 48;
	int line = 0;
	Uint32 color = fg;
	for (const char *p = status; *p && y < s->h - ch; p++) {
		if (*p == '\n') {
			x = 48;
			y += ch;
			line++;
			color = line == 0 ? fg : dim;
			continue;
		}
		glyph(s, x, y, scale, *p, color);
		x += cw;
		if (x > s->w - 48) {
			x = 48;
			y += ch;
		}
	}
}

int movian_ps5_display_open(int width, int height) {
	if (width > 0) {
		disp_w = width;
	}
	if (height > 0) {
		disp_h = height;
	}
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
		fprintf(stderr, "movian-ps5: SDL_Init: %s\n", SDL_GetError());
		return -1;
	}
	window = SDL_CreateWindow("Movian",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		disp_w, disp_h, 0);
	if (window == NULL) {
		fprintf(stderr, "movian-ps5: SDL_CreateWindow: %s\n", SDL_GetError());
		return -1;
	}
	SDL_GameControllerEventState(SDL_ENABLE);
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i)) {
			SDL_GameControllerOpen(i);
		}
	}
	snprintf(status, sizeof status, "MOVIAN\nstarting");
	movian_ps5_present();
	return 0;
}

void movian_ps5_display_close(void) {
	if (window != NULL) {
		SDL_DestroyWindow(window);
		window = NULL;
	}
	SDL_Quit();
}

void movian_ps5_status(const char *text) {
	if (text == NULL) {
		status[0] = 0;
		return;
	}
	snprintf(status, sizeof status, "%s", text);
}

void movian_ps5_present(void) {
	if (window == NULL) {
		return;
	}
	SDL_Surface *s = SDL_GetWindowSurface(window);
	if (s == NULL) {
		return;
	}
	if (SDL_LockSurface(s) == 0) {
		draw_status(s);
		SDL_UnlockSurface(s);
	}
	SDL_UpdateWindowSurface(window);
}

int movian_ps5_poll(int *code) {
	SDL_Event e;
	if (code == NULL || stop_requested) {
		return 0;
	}
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_CONTROLLERDEVICEADDED) {
			if (SDL_IsGameController(e.cdevice.which)) {
				SDL_GameControllerOpen(e.cdevice.which);
			}
			continue;
		}
		if (e.type == SDL_CONTROLLERBUTTONDOWN) {
			*code = e.cbutton.button;
			return 1;
		}
		if (e.type == SDL_KEYDOWN && !e.key.repeat) {
			*code = 1000 + (int)e.key.keysym.scancode;
			return 1;
		}
		if (e.type == SDL_QUIT) {
			*code = -1;
			return 1;
		}
	}
	return 0;
}
