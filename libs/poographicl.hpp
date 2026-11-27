#include "poographicl.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace poographicl {

namespace {

struct Cell {
    char ch = ' ';
    std::string bgColor = "black";
    std::string fgColor = "white";
    bool hasText = false;
};

struct CliState {
    int width = 80;
    int height = 24;
    bool alive = false;
    bool termConfigured = false;
    termios origTermios{};
    int origFlags = 0;

    std::string currentBrush = "white";
    std::vector<std::vector<Cell>> grid;

    std::unordered_map<std::string, int> holdTimers;
    std::unordered_set<std::string> pressedKeys;

    std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
};

CliState g_cli;

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string ansiBg(const std::string& raw) {
    std::string c = toLower(raw);
    static const std::unordered_map<std::string, std::string> bgMap = {
        {"black",   "\033[40m"},
        {"red",     "\033[41m"},
        {"green",   "\033[42m"},
        {"yellow",  "\033[43m"},
        {"blue",    "\033[44m"},
        {"magenta", "\033[45m"},
        {"pink",    "\033[105m"},
        {"cyan",    "\033[46m"},
        {"white",   "\033[47m"},
        {"gray",    "\033[100m"},
        {"grey",    "\033[100m"},
        {"orange",  "\033[48;5;208m"},
        {"purple",  "\033[45m"},
        {"brown",   "\033[48;5;94m"}
    };
    auto it = bgMap.find(c);
    return (it != bgMap.end()) ? it->second : "\033[47m";
}

std::string ansiFg(const std::string& raw) {
    std::string c = toLower(raw);
    static const std::unordered_map<std::string, std::string> fgMap = {
        {"black",   "\033[30m"},
        {"red",     "\033[91m"},
        {"green",   "\033[92m"},
        {"yellow",  "\033[93m"},
        {"blue",    "\033[94m"},
        {"magenta", "\033[95m"},
        {"pink",    "\033[95m"},
        {"cyan",    "\033[96m"},
        {"white",   "\033[97m"},
        {"gray",    "\033[37m"},
        {"grey",    "\033[37m"},
        {"orange",  "\033[38;5;208m"},
        {"purple",  "\033[95m"},
        {"brown",   "\033[38;5;94m"}
    };
    auto it = fgMap.find(c);
    return (it != fgMap.end()) ? it->second : "\033[97m";
}

std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

void restoreTerminal() {
    if (g_cli.termConfigured) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_cli.origTermios);
        fcntl(STDIN_FILENO, F_SETFL, g_cli.origFlags);
        std::cout << "\033[0m\033[?25h\n" << std::flush;
        g_cli.termConfigured = false;
    }
}

void setupTerminal() {
    if (g_cli.termConfigured) return;

    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &g_cli.origTermios);
        g_cli.origFlags = fcntl(STDIN_FILENO, F_GETFL, 0);

        termios raw = g_cli.origTermios;
        raw.c_lflag &= static_cast<unsigned int>(~(ECHO | ICANON));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        fcntl(STDIN_FILENO, F_SETFL, g_cli.origFlags | O_NONBLOCK);

        std::atexit(restoreTerminal);
        g_cli.termConfigured = true;
    }

    std::cout << "\033[?25l\033[2J\033[H" << std::flush;
}

void registerKeyHit(const std::string& keyName) {
    if (keyName == "esc") {
        g_cli.alive = false;
        return;
    }
    if (g_cli.holdTimers.find(keyName) == g_cli.holdTimers.end()) {
        g_cli.pressedKeys.insert(keyName);
    }
    // Keep held for 4 frames to smooth over terminal key-repeat delay
    g_cli.holdTimers[keyName] = 4;
}

void pollKeys() {
    g_cli.pressedKeys.clear();

    for (auto it = g_cli.holdTimers.begin(); it != g_cli.holdTimers.end();) {
        it->second -= 1;
        if (it->second <= 0) {
            it = g_cli.holdTimers.erase(it);
        } else {
            ++it;
        }
    }

    if (!isatty(STDIN_FILENO)) return;

    char buf[64];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    for (ssize_t i = 0; i < n; ++i) {
        unsigned char ch = static_cast<unsigned char>(buf[i]);
        if (ch == 27) { // ESC or arrow escape sequence
            if (i + 2 < n && buf[i + 1] == '[') {
                char code = buf[i + 2];
                if (code == 'A') registerKeyHit("up");
                else if (code == 'B') registerKeyHit("down");
                else if (code == 'C') registerKeyHit("right");
                else if (code == 'D') registerKeyHit("left");
                i += 2;
            } else {
                registerKeyHit("esc");
            }
        } else if (ch == ' ') {
            registerKeyHit("space");
        } else if (ch == '\n' || ch == '\r') {
            registerKeyHit("enter");
        } else if (std::isprint(ch)) {
            std::string s(1, static_cast<char>(std::tolower(ch)));
            registerKeyHit(s);
        }
    }
}

void ensureGrid() {
    if (g_cli.grid.empty()) {
        g_cli.grid.assign(g_cli.height, std::vector<Cell>(g_cli.width));
    }
}

void setCellColor(int x, int y, const std::string& color) {
    if (x < 0 || x >= g_cli.width || y < 0 || y >= g_cli.height) return;
    g_cli.grid[y][x].bgColor = color;
    g_cli.grid[y][x].ch = ' ';
    g_cli.grid[y][x].hasText = false;
}

} // namespace

void makeWindow(int width, int height) {
    g_cli.width = std::max(1, width);
    g_cli.height = std::max(1, height);
    g_cli.grid.assign(g_cli.height, std::vector<Cell>(g_cli.width));
    g_cli.holdTimers.clear();
    g_cli.pressedKeys.clear();
    g_cli.alive = true;
    g_cli.lastFrame = std::chrono::steady_clock::now();
    setupTerminal();
    bucket("black");
}

bool isAlive() {
    pollKeys();
    return g_cli.alive;
}

void bucket(const std::string& color) {
    ensureGrid();
    for (int y = 0; y < g_cli.height; ++y) {
        for (int x = 0; x < g_cli.width; ++x) {
            g_cli.grid[y][x] = Cell{' ', color, "white", false};
        }
    }
}

void brush(const std::string& color) {
    g_cli.currentBrush = color;
}

void draw() {
    if (!g_cli.alive) return;
    ensureGrid();

    std::string frame;
    frame.reserve(static_cast<size_t>(g_cli.width * g_cli.height * 8 + 64));
    frame += "\033[H";

    std::string lastBg;
    std::string lastFg;

    for (int y = 0; y < g_cli.height; ++y) {
        for (int x = 0; x < g_cli.width; ++x) {
            const Cell& cell = g_cli.grid[y][x];
            if (cell.bgColor != lastBg) {
                frame += ansiBg(cell.bgColor);
                lastBg = cell.bgColor;
            }
            if (cell.hasText && cell.fgColor != lastFg) {
                frame += ansiFg(cell.fgColor);
                lastFg = cell.fgColor;
            }
            frame.push_back(cell.ch);
        }
        frame += "\033[0m";
        lastBg.clear();
        lastFg.clear();
        if (y + 1 < g_cli.height) {
            frame += "\r\n";
        }
    }

    std::cout << frame << std::flush;

    constexpr auto targetFrame = std::chrono::milliseconds(33); // ~30 FPS smooth terminal refresh
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - g_cli.lastFrame;
    if (elapsed < targetFrame) {
        std::this_thread::sleep_for(targetFrame - elapsed);
    }
    g_cli.lastFrame = std::chrono::steady_clock::now();
}

void box(int x, int y, int w, int h) {
    ensureGrid();
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            setCellColor(x + dx, y + dy, g_cli.currentBrush);
        }
    }
}

void circle(int cx, int cy, int r) {
    ensureGrid();
    if (r <= 0) return;
    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r * 2; x <= cx + r * 2; ++x) {
            double dx = (x - cx) / 2.0; // Compensate for ~2:1 terminal character aspect ratio
            double dy = (y - cy);
            if (dx * dx + dy * dy <= r * r) {
                setCellColor(x, y, g_cli.currentBrush);
            }
        }
    }
}

void line(int x1, int y1, int x2, int y2) {
    ensureGrid();
    int dx = std::abs(x2 - x1);
    int sx = (x1 < x2) ? 1 : -1;
    int dy = -std::abs(y2 - y1);
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setCellColor(x1, y1, g_cli.currentBrush);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void writeText(int x, int y, const std::string& text) {
    ensureGrid();
    if (y < 0 || y >= g_cli.height) return;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char raw = static_cast<unsigned char>(text[i]);
        if (raw == '\r' || raw == '\n') continue; // Prevent CRLF files from corrupting cursor
        char safeChar = std::isprint(raw) ? static_cast<char>(raw) : ' ';

        int px = x + static_cast<int>(i);
        if (px < 0 || px >= g_cli.width) continue;
        g_cli.grid[y][px].ch = safeChar;
        g_cli.grid[y][px].fgColor = g_cli.currentBrush;
        g_cli.grid[y][px].hasText = true;
    }
}

void pic(int x, int y, const std::string& filename) {
    ensureGrid();
    std::ifstream in(filename);
    if (!in) {
        writeText(x, y, "[?]");
        return;
    }
    std::string lineStr;
    int row = 0;
    while (std::getline(in, lineStr)) {
        writeText(x, y + row, lineStr);
        ++row;
    }
}

bool keyHold(const std::string& key) {
    return g_cli.holdTimers.find(toLower(key)) != g_cli.holdTimers.end();
}

bool keyPress(const std::string& key) {
    return g_cli.pressedKeys.find(toLower(key)) != g_cli.pressedKeys.end();
}

int mouseX() {
    return 0;
}

int mouseY() {
    return 0;
}

bool click() {
    return false;
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
    g_cli.alive = false;
    restoreTerminal();
}

} // namespace poographicl
