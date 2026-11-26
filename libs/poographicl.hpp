#ifndef POOGRAPHICL_HPP
#define POOGRAPHICL_HPP

#include <string>

namespace poographicl {

void makeWindow(int width, int height);
bool isAlive();
void bucket(const std::string& color);
void brush(const std::string& color);
void draw();

void box(int x, int y, int w, int h);
void circle(int x, int y, int r);
void line(int x1, int y1, int x2, int y2);
void writeText(int x, int y, const std::string& text);
void pic(int x, int y, const std::string& filename);

bool keyHold(const std::string& key);
bool keyPress(const std::string& key);
int mouseX();
int mouseY();
bool click();

bool touch(double x1, double y1, double w1, double h1,
           double x2, double y2, double w2, double h2);
int randomInt(int minVal, int maxVal);
void shutdown();

} // namespace poographicl

#endif // POOGRAPHICL_HPP
