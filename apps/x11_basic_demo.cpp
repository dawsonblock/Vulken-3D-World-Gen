#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <cmath>

int main(){
    const char* disp_name = std::getenv("DISPLAY");
    if(!disp_name){ std::fprintf(stderr, "DISPLAY not set.\n"); return 1; }
    Display* d = XOpenDisplay(nullptr);
    if(!d){ std::fprintf(stderr, "Failed to open X display %s\n", disp_name); return 2; }

    int screen = DefaultScreen(d);
    int width = 1280, height = 720;
    Window root = RootWindow(d, screen);
    XSetWindowAttributes attrs{};
    attrs.background_pixel = BlackPixel(d, screen);
    attrs.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
    Window win = XCreateWindow(
        d, root, 0, 0, (unsigned)width, (unsigned)height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWEventMask, &attrs);

    XStoreName(d, win, "VoxelVK X11 Basic Demo");
    XMapWindow(d, win);

    GC gc = XCreateGC(d, win, 0, nullptr);

    bool running = true;
    auto last = std::chrono::steady_clock::now();
    int hue = 0;

    while(running){
        while(XPending(d)){
            XEvent e; XNextEvent(d, &e);
            if(e.type == KeyPress){ running = false; }
            if(e.type == DestroyNotify){ running = false; }
        }

        // Animate background color
        hue = (hue + 1) % 360;
        // simple HSV->RGB like effect via sine waves
        auto cs = [](double t){ return (int)((0.5 + 0.5 * std::sin(t)) * 65535); };
        int r = cs(hue * 0.0174533);
        int g = cs(hue * 0.0174533 + 2.0944);
        int b = cs(hue * 0.0174533 + 4.1888);

        XColor color; color.flags = DoRed | DoGreen | DoBlue; color.red = r; color.green = g; color.blue = b;
        Colormap cmap = DefaultColormap(d, screen);
        if(XAllocColor(d, cmap, &color)){
            XSetForeground(d, gc, color.pixel);
            XFillRectangle(d, win, gc, 0, 0, (unsigned)width, (unsigned)height);
        }

        // Simple overlay rectangle
        XSetForeground(d, gc, WhitePixel(d, screen));
        XFillRectangle(d, win, gc, 40, 40, 240, 60);
        XSetForeground(d, gc, BlackPixel(d, screen));
        XDrawString(d, win, gc, 56, 78, "VoxelVK Visual GUI (X11)", 24);

        // ~60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    XFreeGC(d, gc);
    XDestroyWindow(d, win);
    XCloseDisplay(d);
    return 0;
}
