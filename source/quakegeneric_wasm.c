/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakegeneric.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <js/glue.h>
#include <js/dom_pk_codes.h>

// TODO: sound, float formatting pdclib %f bugs, wheel/joy input
void __unordtf2(void) {}
#include "quakedef.h"
double atof(const char *nptr) {
	return Q_atof((char*)nptr);
}
uint32_t rgbpixels[QUAKEGENERIC_RES_X * QUAKEGENERIC_RES_Y];
unsigned char pal[768];

// #define ARGB(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define ABGR(r, g, b, a) (((a) << 24) | ((b) << 16) | ((g) << 8) | (r))

// #define KEYBUFFERSIZE	32
#define KEYBUFFERSIZE	UINT8_MAX
static int keybuffer[KEYBUFFERSIZE];  // circular key buffer
static int keybuffer_len;  // number of keys in the buffer
static int keybuffer_start;  // index of next item to be read

static int mouse_x, mouse_y;
static float joy_axes[QUAKEGENERIC_JOY_MAX_AXES];

void onblur(void);
bool onkeydown(void* userData, int key, int code, int modifiers);
bool onkeyup(void* userData, int key, int code, int modifiers);
bool onmousemove(void *userData, int button, int x, int y);
bool onmousedown(void *userData, int button, int x, int y);
bool onmouseup(void *userData, int button, int x, int y);

void QG_Init(void)
{
	JS_createCanvas(QUAKEGENERIC_RES_X, QUAKEGENERIC_RES_Y);
	JS_setTitle("Quake");
	JS_requestPointerLock();

    JS_addBlurEventListener(onblur);
    JS_addMouseMoveEventListener(NULL, onmousemove);
    JS_addMouseDownEventListener(NULL, onmousedown);
    JS_addMouseUpEventListener(NULL, onmouseup);
    JS_addKeyDownEventListener(NULL, onkeydown);
    JS_addKeyUpEventListener(NULL, onkeyup);

	keybuffer_len = 0;
	keybuffer_start = 0;
	mouse_x = mouse_y = 0;
	memset(joy_axes, 0, sizeof(joy_axes));
}

static int ConvertToQuakeButton(unsigned char button)
{
	int qbutton;

	switch (button)
	{
		case 0:
			qbutton = K_MOUSE1;
			break;
		case 1:
			qbutton = K_MOUSE3;
			break;
		case 2:
			qbutton = K_MOUSE2;
			break;
		default:
			qbutton = -1;
			break;
	}
	return qbutton;
}

static int ConvertToQuakeKey(int key, int code)
{
	int qkey;

	switch (code)
	{
		case DOM_PK_TAB:
			qkey = K_TAB;
			break;
		case DOM_PK_ENTER:
			qkey = K_ENTER;
			break;
		case DOM_PK_ESCAPE:
			qkey = K_ESCAPE;
			break;
		case DOM_PK_SPACE:
			qkey = K_SPACE;
			break;
		case DOM_PK_BACKSPACE:
			qkey = K_BACKSPACE;
			break;
		case DOM_PK_ARROW_UP:
			qkey = K_UPARROW;
			break;
		case DOM_PK_ARROW_DOWN:
			qkey = K_DOWNARROW;
			break;
		case DOM_PK_ARROW_LEFT:
			qkey = K_LEFTARROW;
			break;
		case DOM_PK_ARROW_RIGHT:
			qkey = K_RIGHTARROW;
			break;
		case DOM_PK_ALT_LEFT:
		case DOM_PK_ALT_RIGHT:
			qkey = K_ALT;
			break;
		case DOM_PK_CONTROL_LEFT:
		case DOM_PK_CONTROL_RIGHT:
			qkey = K_CTRL;
			break;
		case DOM_PK_SHIFT_LEFT:
		case DOM_PK_SHIFT_RIGHT:
			qkey = K_SHIFT;
			break;
		case DOM_PK_F1:
			qkey = K_F1;
			break;
		case DOM_PK_F2:
			qkey = K_F2;
			break;
		case DOM_PK_F3:
			qkey = K_F3;
			break;
		case DOM_PK_F4:
			qkey = K_F4;
			break;
		case DOM_PK_F5:
			qkey = K_F5;
			break;
		case DOM_PK_F6:
			qkey = K_F6;
			break;
		case DOM_PK_F7:
			qkey = K_F7;
			break;
		case DOM_PK_F8:
			qkey = K_F8;
			break;
		case DOM_PK_F9:
			qkey = K_F9;
			break;
		case DOM_PK_F10:
			qkey = K_F10;
			break;
		case DOM_PK_F11:
			qkey = K_F11;
			break;
		case DOM_PK_F12:
			qkey = K_F12;
			break;
		case DOM_PK_INSERT:
			qkey = K_INS;
			break;
		case DOM_PK_DELETE:
			qkey = K_DEL;
			break;
		case DOM_PK_PAGE_DOWN:
			qkey = K_PGDN;
			break;
		case DOM_PK_PAGE_UP:
			qkey = K_PGUP;
			break;
		case DOM_PK_HOME:
			qkey = K_HOME;
			break;
		case DOM_PK_END:
			qkey = K_END;
			break;
		case DOM_PK_PAUSE:
			qkey = K_PAUSE;
			break;
		default:
			qkey = tolower(key);
			break;

		/*
		 * Not yet converted:
		 *   K_JOY*
		 *   K_AUX*
		 */
	}

	return qkey;
}

static int KeyPop(int *down, int *key)
{
	if (keybuffer_len == 0)
		return 0; // underflow

	*key = keybuffer[keybuffer_start];
	*down = *key < 0;
	if (*key < 0)
		*key = -*key;
	keybuffer_start = (keybuffer_start + 1) % KEYBUFFERSIZE;
	keybuffer_len--;

	return 1;
}

static int KeyPush(int down, int key)
{
	if (keybuffer_len == KEYBUFFERSIZE)
		return 0; // overflow
	assert(key > 0);
	if (down) {
		key = -key;
	}
	keybuffer[(keybuffer_start + keybuffer_len) % KEYBUFFERSIZE] = key;
	keybuffer_len++;

	return 1;
}

int QG_GetKey(int *down, int *key)
{
	return KeyPop(down, key);
}

void QG_GetJoyAxes(float *axes)
{
	memcpy(axes, joy_axes, sizeof(joy_axes));
}

void QG_GetMouseMove(int *x, int *y)
{
	*x = mouse_x;
	*y = mouse_y;

	mouse_x = mouse_y = 0;
}

void QG_Quit(void)
{
}

void QG_DrawFrame(void *pixels)
{
	// convert pixels
	for (int i = 0; i < QUAKEGENERIC_RES_X * QUAKEGENERIC_RES_Y; i++)
	{
		uint8_t pixel = ((uint8_t *)pixels)[i];
		uint8_t *entry = &((uint8_t *)pal)[pixel * 3];
		// rgbpixels[i] = ARGB(*(entry), *(entry + 1), *(entry + 2), 255);
		rgbpixels[i] = ABGR(*(entry), *(entry + 1), *(entry + 2), 255);
	}

	// blit
	JS_setPixelsAlpha(rgbpixels);
	JS_requestAnimationFrame();
}

void QG_SetPalette(unsigned char palette[768])
{
	memcpy(pal, palette, 768);
}

// NOTE: mouse btns can still get stuck clicking outside canvas, not sure how to fix
void onblur(void) {
	keybuffer_start = 0;
	keybuffer_len = 0;
	for (int i = 1; i < KEYBUFFERSIZE; i++) {
		KeyPush(0, i);
	}
}

bool onkeydown(void* userData, int key, int code, int modifiers) {
    (void)userData,(void)modifiers;
	(void) KeyPush(1, ConvertToQuakeKey(key, code));
    if (code == DOM_PK_F12) {
        return 0;
    }
    return 1;
}
bool onkeyup(void* userData, int key, int code, int modifiers) {
    (void)userData,(void)modifiers;
	(void) KeyPush(0, ConvertToQuakeKey(key, code));
    if (code == DOM_PK_F12) {
        return 0;
    }
    return 1;
}

bool onmousemove(void *userData, int button, int x, int y) {
	(void)userData,(void)button;
	mouse_x += x;
	mouse_y += y;
	return 0;
}
bool onmousedown(void *userData, int button, int x, int y) {
	(void)x,(void)y;
	button = ConvertToQuakeButton(button);
	if (button != -1) {
		(void) KeyPush(1, button);
	}
	return 0;
}
bool onmouseup(void *userData, int button, int x, int y) {
	(void)x,(void)y;
	button = ConvertToQuakeButton(button);
	if (button != -1) {
		(void) KeyPush(0, button);
	}
	return 0;
}
// TODO
// 		case SDL_MOUSEWHEEL:
// 			if (event.wheel.y > 0)
// 			{
// 				(void) KeyPush(1, K_MWHEELUP);
// 				(void) KeyPush(0, K_MWHEELUP);
// 			}
// 			else if (event.wheel.y < 0)
// 			{
// 				(void) KeyPush(1, K_MWHEELDOWN);
// 				(void) KeyPush(0, K_MWHEELDOWN);
// 			}
// 			break;
// 		case SDL_JOYAXISMOTION:
// 			if (event.jaxis.axis < QUAKEGENERIC_JOY_MAX_AXES) {
// 				joy_axes[event.jaxis.axis] = event.jaxis.value / 32767.0f;
// 			}
// 			break;

// 		case SDL_JOYBUTTONDOWN:
// 		case SDL_JOYBUTTONUP:
// 			button = event.jbutton.button + ((event.jbutton.button < 4) ? K_JOY1 : K_AUX1);
// 			(void) KeyPush((event.type == SDL_JOYBUTTONDOWN), button);
// 			break;
// 	}
// }


int main(int argc, char *argv[])
{
	double oldtime, newtime;
	int running = 1;

	QG_Create(argc, argv);

	oldtime = (double)JS_performanceNow() / 1000 - 0.1;
	while (running)
	{
		// Run the frame at the correct duration.
		newtime = (double)JS_performanceNow() / 1000;
		QG_Tick(newtime - oldtime);
		oldtime = newtime;
	}

	return 0;
}
