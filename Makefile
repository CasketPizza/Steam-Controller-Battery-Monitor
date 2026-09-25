.PHONY: all release clean sanitize
all: scbattery-monitor
release: all
sanitize: all


CXXFLAGS = -std=c++20 -Wall -Werror

DEBUG_FLAGS := -g -Og
RELEASE_FLAGS := -O2
SANITIZER_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

ifeq ($(filter sanitize,$(MAKECMDGOALS)), sanitize)
  CXXFLAGS += $(DEBUG_FLAGS)
  CXXFLAGS += $(SANITIZER_FLAGS)
  CXXFLAGS += -DSANITIZER_BUILD
else ifeq ($(filter release,$(MAKECMDGOALS)), release)
  CXXFLAGS += $(RELEASE_FLAGS)
  IS_RELEASE = 1
else
  CXXFLAGS += $(DEBUG_FLAGS)
endif

ifeq ($(OS),Windows_NT)
  HIDAPI_PKG ?= hidapi
  WINDRES ?= windres
  WINDOWS_RESOURCE := app_icon.o

  ifeq ($(IS_RELEASE),1)
    LDFLAGS += -mwindows
  endif
else
  HIDAPI_PKG ?= hidapi-hidraw
  WINDOW_PKG ?= x11
  CXXFLAGS += -fPIC -pie
endif

QT_PKG := Qt6Widgets Qt6DBus

CXXFLAGS += $(shell pkg-config --cflags $(HIDAPI_PKG))
CXXFLAGS += $(shell pkg-config --cflags $(QT_PKG))
ifneq ($(OS),Windows_NT)
  CXXFLAGS += $(shell pkg-config --cflags $(WINDOW_PKG))
endif

LDLIBS += $(shell pkg-config --libs $(HIDAPI_PKG))
LDLIBS += $(shell pkg-config --libs $(QT_PKG))
ifneq ($(OS),Windows_NT)
  LDLIBS += $(shell pkg-config --libs $(WINDOW_PKG))
endif

TRITON_SRC := $(wildcard TritonLib/src/*.cpp TritonLib/src/*/*.cpp TritonLib/src/*.c TritonLib/src/*/*.c)
BATTERYMON_SRC := $(wildcard src/*.cpp src/*/*.cpp src/*.c src/*/*.c)


scbattery-monitor: $(BATTERYMON_SRC) $(TRITON_SRC) $(WINDOWS_RESOURCE)
	g++ -ITritonLib/include -o scbattery-monitor $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

app_icon.o: app.rc icon.ico
	$(WINDRES) app.rc -O coff -o $@

clean:
	rm -f scbattery-monitor scbattery-monitor.exe app_icon.o qrc_resources.cpp
