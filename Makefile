CXX ?= g++
CXXFLAGS ?= -O2
TARGET = poolang
SRC = src/main.cpp libs/poographic.cpp libs/poographicl.cpp
PREFIX ?= /usr/local
DESTDIR ?=

ALL_CXXFLAGS = -std=c++17 -Ilibs $(CXXFLAGS)

# Auto-detect X11 for native GUI windows in POOGRAPHIC
ifneq ($(wildcard /usr/include/X11/Xlib.h),)
    LDFLAGS += -lX11
else ifneq ($(wildcard /opt/X11/include/X11/Xlib.h),)
    ALL_CXXFLAGS += -I/opt/X11/include
    LDFLAGS += -L/opt/X11/lib -lX11
endif

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(ALL_CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

install: $(TARGET)
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	install -m 755 $(TARGET) "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

clean:
	rm -f $(TARGET)
