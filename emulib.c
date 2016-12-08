/*
 * Compiles HACK machine code to C.
 * Copyright (C) 2016 Pochang Chen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void write_screen(uint16_t, uint16_t);
uint16_t read_key();
extern void program();

static Display *display;
static unsigned long black, white;
static Window window;
static GC gc;
static Pixmap pixmap;

static bool screenbuf[256][512];
static uint16_t last_key;

uint16_t read_key() {
    while(XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);

        if(event.type == Expose) {
            for(int y = 0; y < 256; y++)
                for(int x = 0; x < 512; x++) {
                    XSetForeground(display, gc,
                            screenbuf[y][x] ? black : white);
                    XDrawPoint(display, pixmap, gc, x, y);
                }
            XCopyArea(display, pixmap, window, gc,
                    0, 0, 512, 256, 0, 0);
        } else if(event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            switch(key) {
            case XK_Return:    last_key = 128; break;
            case XK_BackSpace: last_key = 129; break;
            case XK_Left:      last_key = 130; break;
            case XK_Up:        last_key = 131; break;
            case XK_Right:     last_key = 132; break;
            case XK_Down:      last_key = 133; break;
            case XK_Home:      last_key = 134; break;
            case XK_End:       last_key = 135; break;
            case XK_Page_Up:   last_key = 136; break;
            case XK_Page_Down: last_key = 137; break;
            case XK_Insert:    last_key = 138; break;
            case XK_Delete:    last_key = 139; break;
            case XK_Escape:    last_key = 140; break;
            default:
                if(key >= ' ' && key <= '~')
                    last_key = key;
                else if(key >= XK_F1 && key <= XK_F12)
                    last_key = 141 + (key - XK_F1);
            }
        } else if(event.type == KeyRelease) {
            last_key = 0;
        }
    }
    return last_key;
}
void write_screen(uint16_t addr, uint16_t val) {
    if(addr < 0x4000 || addr >= 0x6000)
        __builtin_unreachable();
    int y =  (addr - 0x4000) >> 5;
    int x = ((addr - 0x4000) & 31) << 4;
    for(int i = 0; i < 16; i++) {
        XSetForeground(display, gc,
                (val & (1 << i)) ? black : white);
        screenbuf[y][x + i] = val & (1 << i);
        XDrawPoint(display, window, gc, x + i, y);
    }
    XFlush(display);
}
int main() {
    display = XOpenDisplay(NULL);
    if (!display)
        return 1;

    int screen = DefaultScreen(display);
    black = BlackPixel(display, screen);
    white = WhitePixel(display, screen);

    window = XCreateSimpleWindow(display,
            RootWindow(display, screen), 0, 0, 512, 256, 0,
            black, white);

    XStoreName(display, window, "Emulator");

    XSizeHints *hints = XAllocSizeHints();
    hints->flags = PMinSize | PMaxSize;
    hints->min_width = hints->max_width = 512;
    hints->min_height = hints->max_height = 256;
    XSetWMNormalHints(display, window, hints);
    XFree(hints);

    XSelectInput(display, window,
            ExposureMask | KeyPressMask | KeyReleaseMask);

    XMapWindow(display, window);

    int depth = XDefaultDepth(display, screen);
    gc = DefaultGC(display, screen);

    pixmap = XCreatePixmap(display, window, 512, 256, depth);
    XSetForeground(display, gc, white);
    XFillRectangle(display, pixmap, gc, 0, 0, 512, 256);

    program();

    XFreePixmap(display, pixmap);

    XCloseDisplay(display);
}

