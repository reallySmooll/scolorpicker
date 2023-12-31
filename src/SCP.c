/*
 * MIT License
 *
 * Copyright (c) 2023 Jakub Skowron
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SCP/SCP.h"
#include "SCP/SCP_CLI.h"
#include "SCP/SCP_Clipboard.h"

#include <X11/extensions/shape.h>

Display *dpy;

int screen;
int x;
int y;
int eventBase;
int errorBase;

Window root;
Window pixelWindow;

XEvent event;

XColor color;

Cursor cursor;

char *hex;

Pixmap shapePixmap;

GC shapeGC;
GC windowGC;

void
SCP_Init()
{
    dpy = XOpenDisplay(NULL);

    if (dpy == NULL)
    {
        fprintf(stderr, "Unable to open display!\n");
        exit(1);
    }

    screen = XDefaultScreen(dpy);
    root   = XDefaultRootWindow(dpy);
    cursor = XCreateFontCursor(dpy, XC_crosshair);

    hex = malloc(8 * sizeof(char));
}

void
SCP_GetPixelColor(Display *display, int x, int y, XColor *color)
{
    XImage *image;
    image = XGetImage(display, root, x, y, 1, 1, AllPlanes, ZPixmap);
    if (image == NULL)
    {
        XCloseDisplay(display);
        exit(1);
    }

    color->pixel = XGetPixel(image, 0, 0);

    XFree(image);

    XQueryColor(display, XDefaultColormap(display, screen), color);
}

void
SCP_CreatePixelWindow(Display *display, XColor *color)
{
    int width = 100;
    int height = 100;

    Window rootReturn, childReturn;
    int rootXReturn, rootYReturn;
    int childXReturn, childYReturn;
    unsigned int mask;

    XQueryPointer(display, root, &rootReturn, &childReturn, &rootXReturn, &rootYReturn, &childXReturn, &childYReturn, &mask);

    SCP_GetPixelColor(dpy, rootXReturn, rootYReturn, color);

    pixelWindow = XCreateSimpleWindow(display, root, rootXReturn + 10, rootYReturn + 10, width, height, 0, color->pixel, color->pixel);

    windowGC = XCreateGC(display, pixelWindow, 0, NULL);

    if (XShapeQueryExtension(dpy, &eventBase, &errorBase))
    {
        shapePixmap = XCreatePixmap(display, pixelWindow, width, height, 1);

        shapeGC = XCreateGC(display, shapePixmap, 0, NULL);

        XSetForeground(display, shapeGC, 0);

        XFillRectangle(display, shapePixmap, shapeGC, 0, 0, width, height);

        XSetForeground(display, shapeGC, 1);

        int corner_radius = 10;
        XFillArc(display, shapePixmap, shapeGC, 0, 0, 2 * corner_radius, 2 * corner_radius, 90 * 64, 90 * 64);
        XFillArc(display, shapePixmap, shapeGC, width - 2 * corner_radius, 0, 2 * corner_radius, 2 * corner_radius, 0 * 64, 90 * 64);
        XFillArc(display, shapePixmap, shapeGC, 0, height - 2 * corner_radius, 2 * corner_radius, 2 * corner_radius, 180 * 64, 90 * 64);
        XFillArc(display, shapePixmap, shapeGC, width - 2 * corner_radius, height - 2 * corner_radius, 2 * corner_radius, 2 * corner_radius, 270 * 64, 90 * 64);

        XFillRectangle(display, shapePixmap, shapeGC, corner_radius, 0, width - 2 * corner_radius, height);
        XFillRectangle(display, shapePixmap, shapeGC, 0, corner_radius, width, height - 2 * corner_radius);

        XShapeCombineMask(display, pixelWindow, ShapeBounding, 0, 0, shapePixmap, ShapeSet);
    }

    Atom bypassCompositor = XInternAtom(display, "_NET_WM_BYPASS_COMPOSITOR", False);
    if (bypassCompositor == None)
    {
        printf("_NET_WM_BYPASS_COMPOSITOR\n");
        XCloseDisplay(display);
        exit(1);
    }

    unsigned long value = 1;
    XChangeProperty(display, pixelWindow, bypassCompositor, XA_CARDINAL, 32, PropModeReplace, (const unsigned char *)&value, 1);

    XSetWindowAttributes attributes;
    attributes.override_redirect = True;
    XChangeWindowAttributes(display, pixelWindow, CWOverrideRedirect, &attributes);

    XMapWindow(display, pixelWindow);
    XFlush(display);
}

void
SCP_ChooseFormat(const char *format)
{
    if (outputToTerminal == true)
    {
        if (strcmp(format, "hex") == 0)
        { printf("#%06lX\n", color.pixel); }
        if (strcmp(format, "rgb") == 0)
        { printf("rgb(%d, %d, %d)\n", color.red >> 8, color.green >> 8, color.blue >> 8); }
    }
    if (outputToTerminal == false)
    {
        if (strcmp(format, "hex") == 0)
        { sprintf(hex, "#%06lX\n", color.pixel); }
        if (strcmp(format, "rgb") == 0)
        { sprintf(hex, "rgb(%d, %d, %d)\n", color.red >> 8, color.green >> 8, color.blue >> 8); }
    }
}

void
SCP_PrintPixelColor(Display *display, int x, int y, XColor *color)
{
    SCP_GetPixelColor(display, x, y, color);

    SCP_ChooseFormat(format);
}

void
SCP_Close()
{
    free(hex);

    XUngrabPointer(dpy, CurrentTime);
    XUnmapWindow(dpy, pixelWindow);

    if (XShapeQueryExtension(dpy, &eventBase, &errorBase))
    {
        XFreePixmap(dpy, shapePixmap);
        XFreeGC(dpy, shapeGC);
    }
    XFreeGC(dpy, windowGC);
    XDestroyWindow(dpy, pixelWindow);
    XFreeCursor(dpy, cursor);

    XCloseDisplay(dpy);
    exit(0);
}

void
SCP_Main(int argc, char *argv[])
{
    SCP_Init();

    SCP_CreatePixelWindow(dpy, &color);

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            const char *option = argv[i];
            const char *value = NULL;

            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                value = argv[i + 1];
                i++;
            }

            SCP_CLI_HandleArguments(option, value);
        }
    }

    XGrabPointer(dpy, root, True, PointerMotionMask | ButtonPressMask, GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);

    while (1)
    {
        XNextEvent(dpy, &event);

        switch (event.type)
        {
            case ButtonPress:
            {
                switch (event.xbutton.button)
                {
                    case Button1:
                    {
                        if (outputToTerminal == false)
                        {
                            SCP_Clipboard_CopyPixelColor(dpy, event.xbutton.x, event.xbutton.y, &color);
                            SCP_Close();
                        }
                        else
                        {
                            SCP_PrintPixelColor(dpy, event.xbutton.x, event.xbutton.y, &color);
                            SCP_Close();
                        }
                        break;
                    }
                    default:
                    {
                        SCP_Close();
                        break;
                    };
                }
                break;
            }
            case MotionNotify:
            {
                x = event.xmotion.x;
                y = event.xmotion.y;

                SCP_GetPixelColor(dpy, x, y, &color);
                XSetForeground(dpy, windowGC, color.pixel);
                XFillRectangle(dpy, pixelWindow, windowGC, 0, 0, 100, 100);
                XMoveWindow(dpy, pixelWindow, x + 10, y + 10);

                break;
            }
            default: break;
        }
    }
}
