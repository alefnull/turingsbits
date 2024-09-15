RACK_DIR ?= ../Rack-SDK

FLAGS += -std=c++20
CFLAGS += -std=c++20
CXXFLAGS += -std=c++20

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
# DISTRIBUTABLES += presets
# DISTRIBUTABLES += selections

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk