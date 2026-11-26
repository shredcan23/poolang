CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Ilibs
TARGET = poolang
SRC = src/main.cpp libs/poographic.cpp libs/poographicl.cpp
PREFIX ?= /usr/local
DESTDIR ?=

# Auto-detect X11 for native GUI windows in POOGRAPHIC
ifneq ($(wildcard /usr/include/X11/Xlib.h),)
    LDFLAGS += -lX11
else ifneq ($(wildcard /opt/X11/include/X11/Xlib.h),)
    CXXFLAGS += -I/opt/X11/include
    LDFLAGS += -L/opt/X11/lib -lX11
endif

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(SRC)$(CXX) $(CXXFLAGS)$(SRC) -o $(TARGET)$(LDFLAGS)

install: $(TARGET)
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	install -m 755 $(TARGET) "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

clean:
	rm -f $(TARGET)
