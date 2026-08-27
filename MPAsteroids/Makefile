#**************************************************************************************************
#
#   MPAsteroids makefile
#
#   Two build targets:
#     > make web     (default) - Emscripten/WebAssembly client, output: mpasteroids.html/.js/.wasm/.data
#     > make server            - native headless dedicated server, output: mpasteroids-server
#
#   The client is browser-only; the server is meant to run on a Linux host behind a
#   reverse proxy (nginx/Caddy) that terminates TLS and forwards wss:// -> ws://.
#
#**************************************************************************************************

.PHONY: all web server clean

# Where the built web client connects. For a proxied production deploy:
#   make SERVER_HOST=asteroids.example.com SERVER_PUBLIC_PORT=443 SERVER_PATH=/ws
SERVER_HOST           ?= 127.0.0.1
# Port the server binds locally (kept private behind a reverse proxy).
SERVER_PORT           ?= 25665
# Port the browser dials, and the path it requests.
SERVER_PUBLIC_PORT    ?= $(SERVER_PORT)
SERVER_PATH           ?= /

PROJECT_NAME          ?= mpasteroids
SERVER_NAME           ?= mpasteroids-server

RAYLIB_PATH           ?= C:/raylib
BUILD_MODE            ?= RELEASE

# PLATFORM_WEB: Default properties
BUILD_WEB_SHELL       ?= shell.html
BUILD_WEB_HEAP_SIZE   ?= 134217728
BUILD_WEB_RESOURCES   ?= TRUE
BUILD_WEB_RESOURCES_PATH  ?= resources@resources

ifeq ($(OS),Windows_NT)
    PLATFORM_OS = WINDOWS
    EMSDK_PATH         ?= C:/raylib/emsdk
    EMSCRIPTEN_PATH    ?= $(EMSDK_PATH)/upstream/emscripten
    CLANG_PATH          = $(EMSDK_PATH)/upstream/bin
    PYTHON_PATH         = $(EMSDK_PATH)/python/3.13.3_64bit
    NODE_PATH           = $(EMSDK_PATH)/node/22.16.0_64bit/bin
    # Scoped to the web target only - exporting this globally would shadow the
    # native toolchain and break `make server`.
    EMSDK_PATH_PREFIX   = $(EMSDK_PATH);$(EMSCRIPTEN_PATH);$(CLANG_PATH);$(NODE_PATH);$(PYTHON_PATH);
else
    UNAMEOS = $(shell uname)
    ifeq ($(UNAMEOS),Linux)
        PLATFORM_OS = LINUX
    endif
    ifeq ($(UNAMEOS),Darwin)
        PLATFORM_OS = OSX
    endif
endif

# Web client toolchain
#------------------------------------------------------------------------------------------------
CC = em++

SERVER_DEFINES = -DSERVER_HOST='"$(SERVER_HOST)"' -DSERVER_PORT=$(SERVER_PORT) \
                 -DSERVER_PUBLIC_PORT=$(SERVER_PUBLIC_PORT) -DSERVER_PATH='"$(SERVER_PATH)"'

CFLAGS = -Wall -std=c++17 -D_DEFAULT_SOURCE -Wno-missing-braces -Wno-unused-result $(SERVER_DEFINES)

ifeq ($(BUILD_MODE),DEBUG)
    CFLAGS += -g -D_DEBUG
else
    CFLAGS += -O3
endif

INCLUDE_PATHS = -I. -Isrc/include -Isrc/include/raylib -Isrc/include/networking -I$(RAYLIB_PATH)/src -I$(RAYLIB_PATH)/src/external $(EXTRA_INCLUDE_PATHS)

# NOTE: No -sASYNCIFY. The game drives itself from emscripten_set_main_loop and
# the WebSocket API is callback-driven, so nothing blocks and Asyncify's
# stack-unwinding overhead is pure cost.
LDFLAGS = -L. -sTOTAL_MEMORY=$(BUILD_WEB_HEAP_SIZE) -sFORCE_FILESYSTEM=1 \
          -sEXPORTED_RUNTIME_METHODS=ccall,stringToUTF8 -sMINIFY_HTML=0 -sUSE_GLFW=3 \
          -sALLOW_MEMORY_GROWTH=1 -lwebsocket.js

ifeq ($(BUILD_WEB_RESOURCES),TRUE)
    LDFLAGS += --preload-file $(BUILD_WEB_RESOURCES_PATH)
endif

ifeq ($(BUILD_MODE),DEBUG)
    LDFLAGS += -sASSERTIONS=1 --profiling
endif

LDFLAGS += --shell-file $(BUILD_WEB_SHELL)

LDLIBS = lib/libraylib.web.a

# Source files
#------------------------------------------------------------------------------------------------
WEB_SOURCE_FILES = \
    src/main.cpp \
    src/GameApp.cpp \
    src/Models.cpp \
    src/Player.cpp \
    src/Sounds.cpp \
    src/World.cpp \
    src/Entity.cpp \
    src/NetClient.cpp

SERVER_SOURCE_FILES = \
    src/server_main.cpp \
    src/ServerHost.cpp \
    src/WebSocketServer.cpp

# Native server toolchain
#------------------------------------------------------------------------------------------------
SERVER_CC      ?= g++
SERVER_CFLAGS   = -Wall -std=c++17 -O2 $(SERVER_DEFINES)
SERVER_INCLUDES = -I. -Isrc/include -Isrc/include/raylib -Isrc/include/networking
SERVER_LDLIBS   =

ifeq ($(PLATFORM_OS),WINDOWS)
    SERVER_LDLIBS += -lws2_32
else
    SERVER_LDLIBS += -lpthread
endif

# Build targets
#------------------------------------------------------------------------------------------------
all: web

web: export PATH := $(EMSDK_PATH_PREFIX)$(PATH)
web:
	$(CC) -o $(PROJECT_NAME).html $(WEB_SOURCE_FILES) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS) -DPLATFORM_WEB

server:
	$(SERVER_CC) -o $(SERVER_NAME) $(SERVER_SOURCE_FILES) $(SERVER_CFLAGS) $(SERVER_INCLUDES) $(SERVER_LDLIBS)

clean:
ifeq ($(PLATFORM_OS),WINDOWS)
	del /q $(PROJECT_NAME).html $(PROJECT_NAME).js $(PROJECT_NAME).wasm $(PROJECT_NAME).data $(SERVER_NAME).exe 2>nul || exit 0
else
	rm -f $(PROJECT_NAME).html $(PROJECT_NAME).js $(PROJECT_NAME).wasm $(PROJECT_NAME).data $(SERVER_NAME)
endif
	@echo Cleaning done
