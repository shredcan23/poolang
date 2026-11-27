#include "poographic.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if __has_include(<X11/Xlib.h>)
#define POOGRAPHIC_HAS_X11 1
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#else
#define POOGRAPHIC_HAS_X11 0
#endif

namespace poographic {

namespace {

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

unsigned long parseColorRGB(const std::string& raw) {
    std::string name = toLower(raw);
    if (!name.empty() && name[0] == '#') {
        try {
            return std::stoul(name.substr(1), nullptr, 16);
        } catch (...) {
            return 0xFFFFFF;
        }
    }

    static const std::unordered_map<std::string, unsigned long> palette = {
        {"black",   0x111111},
        {"white",   0xFFFFFF},
        {"red",     0xE63946},
        {"green",   0x2A9D8F},
        {"blue",    0x3A86FF},
        {"yellow",  0xFFBE0B},
        {"cyan",    0x00F5D4},
        {"pink",    0xFF70A6},
        {"magenta", 0xD90429},
        {"purple",  0x8338EC},
        {"orange",  0xFB5607},
        {"gray",    0x8D99AE},
        {"grey",    0x8D99AE},
        {"brown",   0x7F4F24}
    };

    auto it = palette.find(name);
    return (it != palette.end()) ? it->second : 0xFFFFFF;
}

std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

void skipPpmComments(std::istream& in) {
    while (true) {
        in >> std::ws;
        if (in.peek() == '#') {
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }
}

#if POOGRAPHIC_HAS_X11

struct GuiState {
    Display* display = nullptr;
    int screen = 0;
    Window window = 0;
    Pixmap backbuffer = 0;
    GC gc = nullptr;
    Atom wmDelete = 0;

    int width = 800;
    int height = 600;
    bool alive = false;
    unsigned long currentBrush = 0xFFFFFF;

    std::unordered_set<std::string> heldKeys;
    std::unordered_set<std::string> pressedKeys;
    int mX = 0;
    int mY = 0;
    bool mouseClicked = false;

    std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
};

GuiState g_gui;

std::string normalizeKeySym(KeySym ks) {
    if (ks == XK_Escape) return "esc";
    if (ks == XK_space) return "space";
    if (ks == XK_Return || ks == XK_KP_Enter) return "enter";
    if (ks == XK_Up) return "up";
    if (ks == XK_Down) return "down";
    if (ks == XK_Left) return "left";
    if (ks == XK_Right) return "right";
    if (ks == XK_Shift_L || ks == XK_Shift_R) return "shift";
    if (ks == XK_Control_L || ks == XK_Control_R) return "ctrl";

    const char* raw = XKeysymToString(ks);
    if (!raw) return "";
    return toLower(raw);
}

void pumpEvents() {
    if (!g_gui.display || !g_gui.alive) return;

    g_gui.pressedKeys.clear();

    while (XPending(g_gui.display) > 0) {
        XEvent ev;
        XNextEvent(g_gui.display, &ev);

        if (ev.type == ClientMessage) {
            if (static_cast<Atom>(ev.xclient.data.l[0]) == g_gui.wmDelete) {
                g_gui.alive = false;
            }
        } else if (ev.type == DestroyNotify) {
            g_gui.alive = false;
        } else if (ev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            std::string name = normalizeKeySym(ks);
            if (!name.empty()) {
                if (g_gui.heldKeys.find(name) == g_gui.heldKeys.end()) {
                    g_gui.pressedKeys.insert(name);
                }
                g_gui.heldKeys.insert(name);
                if (name == "esc") {
                    g_gui.alive = false;
                }
            }
        } else if (ev.type == KeyRelease) {
            // Ignore X11 auto-repeat synthetic KeyRelease
            if (XEventsQueued(g_gui.display, QueuedAfterReading) > 0) {
                XEvent nextEv;
                XPeekEvent(g_gui.display, &nextEv);
                if (nextEv.type == KeyPress && nextEv.xkey.time == ev.xkey.time &&
                    nextEv.xkey.keycode == ev.xkey.keycode) {
                    continue;
                }
            }
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            std::string name = normalizeKeySym(ks);
            if (!name.empty()) {
                g_gui.heldKeys.erase(name);
            }
        } else if (ev.type == ButtonPress) {
            if (ev.xbutton.button == Button1) {
                g_gui.mouseClicked = true;
            }
        } else if (ev.type == ButtonRelease) {
            if (ev.xbutton.button == Button1) {
                g_gui.mouseClicked = false;
            }
        }
    }

    Window rootRet, childRet;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    if (XQueryPointer(g_gui.display, g_gui.window, &rootRet, &childRet,
                      &rootX, &rootY, &winX, &winY, &mask)) {
        g_gui.mX = winX;
        g_gui.mY = winY;
    }
}

#else

bool g_fallbackWarned = false;
bool g_fallbackAlive = false;

#endif

} // namespace

void makeWindow(int width, int height) {
#if POOGRAPHIC_HAS_X11
    if (g_gui.display) {
        shutdown();
    }

    g_gui.display = XOpenDisplay(nullptr);
    if (!g_gui.display) {
        std::cerr << "[POOGRAPHIC] Cannot open X11 display. Use POOGRAPHICL for terminal graphics.\n";
        g_gui.alive = false;
        return;
    }

    g_gui.width = std::max(1, width);
    g_gui.height = std::max(1, height);
    g_gui.screen = DefaultScreen(g_gui.display);
    g_gui.heldKeys.clear();
    g_gui.pressedKeys.clear();
    g_gui.mouseClicked = false;

    Window root = RootWindow(g_gui.display, g_gui.screen);
    g_gui.window = XCreateSimpleWindow(
        g_gui.display, root,
        100, 100,
        static_cast<unsigned int>(g_gui.width),
        static_cast<unsigned int>(g_gui.height),
        1,
        BlackPixel(g_gui.display, g_gui.screen),
        BlackPixel(g_gui.display, g_gui.screen)
    );

    XStoreName(g_gui.display, g_gui.window, "POOGRAPHIC");
    XSelectInput(g_gui.display, g_gui.window,
                 KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);

    g_gui.wmDelete = XInternAtom(g_gui.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_gui.display, g_gui.window, &g_gui.wmDelete, 1);

    int depth = DefaultDepth(g_gui.display, g_gui.screen);
    g_gui.backbuffer = XCreatePixmap(
        g_gui.display, g_gui.window,
        static_cast<unsigned int>(g_gui.width),
        static_cast<unsigned int>(g_gui.height),
        static_cast<unsigned int>(depth)
    );

    g_gui.gc = XCreateGC(g_gui.display, g_gui.backbuffer, 0, nullptr);
    XMapWindow(g_gui.display, g_gui.window);
    XFlush(g_gui.display);

    g_gui.alive = true;
    g_gui.lastFrame = std::chrono::steady_clock::now();
    bucket("black");
#else
    (void)width;
    (void)height;
    if (!g_fallbackWarned) {
        std::cerr << "[POOGRAPHIC] X11 headers (libx11) not found at compile time. "
                  << "Install libx11 and rebuild, or use POOGRAPHICL for terminal graphics.\n";
        g_fallbackWarned = true;
    }
    g_fallbackAlive = false;
#endif
}

bool isAlive() {
#if POOGRAPHIC_HAS_X11
    pumpEvents();
    return g_gui.alive;
#else
    return g_fallbackAlive;
#endif
}

void bucket(const std::string& color) {
#if POOGRAPHIC_HAS_X11
    if (!g_gui.display || !g_gui.alive) return;
    unsigned long rgb = parseColorRGB(color);
    XSetForeground(g_gui.display, g_gui.gc, rgb);
    XFillRectangle(g_gui.display, g_gui.backbuffer, g_gui.gc,
                   0, 0,
                   static_cast<unsigned int>(g_gui.width),
                   static_cast<unsigned int>(g_gui.height));
    XSetForeground(g_gui.display, g_gui.gc, g_gui.currentBrush);
#else
    (void)color;
#endif
}

void brush(const std::string& color) {
#if POOGRAPHIC_HAS_X11
    g_gui.currentBrush = parseColorRGB(color);
    if (g_gui.display && g_gui.alive) {
        XSetForeground(g_gui.display, g_gui.gc, g_gui.currentBrush);
    }
#else
    (void)color;
#endif
}

void draw() {
#if POOGRAPHIC_HAS_X11
    if (!g_gui.display || !g_gui.alive) return;

    XCopyArea(g_gui.display, g_gui.backbuffer, g_gui.window, g_gui.gc,
              0, 0,
              static_cast<unsigned int>(g_gui.width),
              static_cast<unsigned int>(g_gui.height),
              0, 0);
    XFlush(g_gui.display);

    constexpr auto targetFrame = std::chrono::milliseconds(16); // ~60 FPS
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - g_gui.lastFrame;
    if (elapsed < targetFrame) {
        std::this_thread::sleep_for(targetFrame - elapsed);
    }
    g_gui.lastFrame = std::chrono::steady_clock::now();
#endif
}

void box(int x, int y, int w, int h) {
#if POOGRAPHIC_HAS_X11
    if (!g_gui.display || !g_gui.alive || w <= 0 || h <= 0) return;
    XSetForeground(g_gui.display, g_gui.gc, g_gui.currentBrush);
    XFillRectangle(g_gui.display, g_gui.backbuffer, g_gui.gc,
                   x, y,
                   static_cast<unsigned int>(w),
                   static_cast<unsigned int>(h));
#else
    (void)x; (void)y; (void)w; (void)h;
#endif
}

void circle(int x, int y, int r) {
#if POOGRAPHIC_HAS_X11
    if (!g_gui.display || !g_gui.alive || r <= 0) return;
    XSetForeground(g_gui.display, g_gui.gc, g_gui.currentBrush);
    XFillArc(g_gui.display, g_gui.backbuffer, g_gui.gc,
             x - r, y - r,
             static_cast<unsigned int>(r * 2),
             static_cast<unsigned int>(r * 2),
             0, 360 * 64);
#else
    (void)x; (void)y; (void)r;
#endif
}

void line(int x1, int y1, int x2, int y2) {
#if POOGRAPHIC_HAS_X11
    if (!g_gui.display || !g_gui.alive) return;
    XSetForeground(g_gui.display, g_gui.gc, g_gui.currentBrush);
    XDrawLine(g_gui.display, g_gui.backbuffer, g_gui.gc, x1, y1, x2, y2);
#else
    (void)x1; (void)y1; (void)x2; (void)y2;
#endif
}

void writeText(int x, int y, const std::string& text) {
#if POOGRAPHIC_HAS_X11
    if (!g_gui.display || !g_gui.alive) return;
    XSetForeground(g_gui.display, g_gui.gc, g_gui.currentBrush);
    XDrawString(g_gui.display, g_gui.backbuffer, g_gui.gc,
                x, y + 14, text.c_str(), static_cast<int>(text.size()));
#else
    (void)x; (void)y; (void)text;
#endif
}

void pic(int x, int y, const std::string& filename) {
#if POOGRAPHIC_HAS_X11
    if (!g_gui.display || !g_gui.alive) return;

    // Supports binary P6 PPM images natively without external libraries
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        XDrawRectangle(g_gui.display, g_gui.backbuffer, g_gui.gc, x, y, 32, 32);
        return;
    }

    std::string magic;
    in >> magic;
    if (magic == "P6") {
        int imgW = 0, imgH = 0, maxVal = 255;
        skipPpmComments(in);
        in >> imgW;
        skipPpmComments(in);
        in >> imgH;
        skipPpmComments(in);
        in >> maxVal;
        in.get(); // consume single whitespace byte before binary pixels

        if (imgW > 0 && imgH > 0 && maxVal > 0) {
            for (int py = 0; py < imgH; ++py) {
                for (int px = 0; px < imgW; ++px) {
                    unsigned char rgb[3];
                    if (!in.read(reinterpret_cast<char*>(rgb), 3)) break;
                    unsigned long pixel = (static_cast<unsigned long>(rgb[0]) << 16) |
                                          (static_cast<unsigned long>(rgb[1]) << 8) |
                                          static_cast<unsigned long>(rgb[2]);
                    XSetForeground(g_gui.display, g_gui.gc, pixel);
                    XDrawPoint(g_gui.display, g_gui.backbuffer, g_gui.gc, x + px, y + py);
                }
            }
        }
        XSetForeground(g_gui.display, g_gui.gc, g_gui.currentBrush);
    } else {
        XDrawRectangle(g_gui.display, g_gui.backbuffer, g_gui.gc, x, y, 32, 32);
    }
#else
    (void)x; (void)y; (void)filename;
#endif
}

bool keyHold(const std::string& key) {
#if POOGRAPHIC_HAS_X11
    return g_gui.heldKeys.find(toLower(key)) != g_gui.heldKeys.end();
#else
    (void)key;
    return false;
#endif
}

bool keyPress(const std::string& key) {
#if POOGRAPHIC_HAS_X11
    return g_gui.pressedKeys.find(toLower(key)) != g_gui.pressedKeys.end();
#else
    (void)key;
    return false;
#endif
}

int mouseX() {
#if POOGRAPHIC_HAS_X11
    return g_gui.mX;
#else
    return 0;
#endif
}

int mouseY() {
#if POOGRAPHIC_HAS_X11
    return g_gui.mY;
#else
    return 0;
#endif
}

bool click() {
#if POOGRAPHIC_HAS_X11
    return g_gui.mouseClicked;
#else
    return false;
#endif
}

bool touch(double x1, double y1, double w1, double h1,
           double x2, double y2, double w2, double h2) {
    return (x1 < x2 + w2) &&
           (x1 + w1 > x2) &&
           (y1 < y2 + h2) &&
           (y1 + h1 > y2);
}

int randomInt(int minVal, int maxVal) {
    if (minVal > maxVal) std::swap(minVal, maxVal);
    std::uniform_int_distribution<int> dist(minVal, maxVal);
    return dist(rng());
}

void shutdown() {
#if POOGRAPHIC_HAS_X11
    if (g_gui.display) {
        if (g_gui.gc) XFreeGC(g_gui.display, g_gui.gc);
        if (g_gui.backbuffer) XFreePixmap(g_gui.display, g_gui.backbuffer);
        if (g_gui.window) XDestroyWindow(g_gui.display, g_gui.window);
        XCloseDisplay(g_gui.display);
        g_gui.display = nullptr;
        g_gui.gc = nullptr;
        g_gui.backbuffer = 0;
        g_gui.window = 0;
    }
    g_gui.alive = false;
#endif
}

} // namespace poographic
