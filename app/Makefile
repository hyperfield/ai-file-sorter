# Detect platform
UNAME := $(shell uname | cut -d'-' -f1)

BIN_DIR := ./bin
OBJ_DIR := ./obj
SRC_DIR := ./lib

ifeq ($(UNAME), Linux)
    PLATFORM := Linux
    CXXFLAGS += -DLINUX
    TARGET := $(BIN_DIR)/aifilesorter-bin
    INSTALL_DIR := /usr/local/bin
    INSTALL_LIB_DIR := /usr/local/lib/aifilesorter
	LD_CONF_FILE := /etc/ld.so.conf.d/aifilesorter.conf

    # --- Qt6 setup ---
    QT_INCLUDE_BASE ?= /usr/include/x86_64-linux-gnu/qt6
    QT_LIB_BASE     ?= /usr/lib/x86_64-linux-gnu
    CXXFLAGS += -I$(QT_INCLUDE_BASE) -I$(QT_INCLUDE_BASE)/QtCore -I$(QT_INCLUDE_BASE)/QtGui -I$(QT_INCLUDE_BASE)/QtWidgets
    LDFLAGS  += -L$(QT_LIB_BASE) -lQt6Widgets -lQt6Gui -lQt6Core

    # --- Other dependencies ---
	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lX11 -pthread
	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled/cpu/bin'
	LDFLAGS += -Wl,-rpath-link=./lib/precompiled/cpu/bin

else ifeq ($(UNAME), Darwin)
    PATH := /opt/homebrew/opt/qt/bin:/opt/homebrew/opt/curl/bin:$(PATH)
    export PATH

    export PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig:/opt/homebrew/opt/libffi/lib/pkgconfig:/opt/homebrew/opt/expat/lib/pkgconfig:/opt/homebrew/opt/qt/lib/pkgconfig:$(PKG_CONFIG_PATH)

    export LDFLAGS += -L/opt/homebrew/opt/libffi/lib
    export CPPFLAGS += -I/opt/homebrew/opt/libffi/include

    PLATFORM := MacOS
    CXXFLAGS += -DMACOS -DENABLE_METAL -DGGML_USE_METAL -Wno-deprecated -Iinclude/llama
    TARGET := $(BIN_DIR)/aifilesorter
    INSTALL_DIR := /usr/local/bin
	INSTALL_LIB_DIR := /usr/local/lib

    IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
    SPDLOG_PATH := $(shell if [ "$(IS_APPLE_SILICON)" = "1" ]; then echo "/opt/homebrew/include"; else echo "/usr/local/include"; fi)
    CXXFLAGS += -I$(SPDLOG_PATH)

    PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)
ifeq ($(strip $(PKG_CONFIG)),)
$(error pkg-config is required to locate Qt6 frameworks on macOS. Please install it (e.g. brew install pkg-config))
endif

    QT_PACKAGES := Qt6Widgets Qt6Gui Qt6Core
    QT_CXXFLAGS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" $(PKG_CONFIG) --cflags $(QT_PACKAGES))
    QT_LDFLAGS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" $(PKG_CONFIG) --libs $(QT_PACKAGES))
ifeq ($(strip $(QT_CXXFLAGS)),)
$(error Could not retrieve Qt6 flags via pkg-config. Ensure Qt6 is installed (brew install qt) and PKG_CONFIG_PATH includes its pkgconfig directory.)
endif
    CXXFLAGS += $(QT_CXXFLAGS)
    LDFLAGS += $(QT_LDFLAGS)

    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lintl -pthread
	LDFLAGS += -framework Metal -framework Foundation
    LDFLAGS += -Wl,-rpath,@loader_path/lib -Wl,-rpath,@loader_path/../lib/precompiled


endif

WRAPPED_BINARY := $(notdir $(TARGET))

# Compiler and flags
CXX = g++
CXXFLAGS += -std=c++20 -Wall -O2 -fPIC
# Suppress GCC 13 fmt bigint false positives
CXXFLAGS += -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread
INCLUDE_DIRS = -I./include -I./include/llama
LIB_DIRS =
ifeq ($(UNAME), Linux)
LIB_DIRS += -L./lib/precompiled/cpu/bin
else
LIB_DIRS += -L./lib/precompiled
endif
ifeq ($(UNAME), Darwin)
LIB_DIRS += -L/opt/homebrew/lib
endif

QRC_FILE := resources/app.qrc
QRC_CPP := $(OBJ_DIR)/qrc_app.cpp
QRC_OBJ := $(OBJ_DIR)/qrc_app.o

ifndef RCC
RCC := $(shell command -v qt6-rcc 2>/dev/null)
ifeq ($(strip $(RCC)),)
RCC := $(wildcard /usr/lib/qt6/libexec/rcc)
endif
ifeq ($(strip $(RCC)),)
RCC := $(shell command -v rcc 2>/dev/null)
endif
ifeq ($(strip $(RCC)),)
RCC := $(wildcard /opt/homebrew/opt/qt/bin/qt6-rcc)
endif
endif
ifeq ($(strip $(RCC)),)
$(error Could not find Qt resource compiler (qt6-rcc or rcc))
endif

# Source files
SRCS = main.cpp $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))

.PHONY: all clean install uninstall

# Main rules
all: $(TARGET)
	@printf "\nFinished building AI File Sorter for %s\n" "$(PLATFORM)"

$(TARGET): $(OBJS) $(QRC_OBJ)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_DIRS) $(LDFLAGS)
ifeq ($(PLATFORM), Linux)
	@$(MAKE) create_run_wrapper
endif

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(OBJ_DIR)/main.o: main.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(QRC_CPP): $(QRC_FILE)
	mkdir -p $(OBJ_DIR)
	$(RCC) -o $@ $<

$(QRC_OBJ): $(QRC_CPP)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

create_run_wrapper:
	@mkdir -p $(BIN_DIR)
	@{ \
		echo '#!/bin/sh'; \
		echo 'SCRIPT_DIR="$$(cd "$$(dirname "$$0")" && pwd)"'; \
		echo 'APP_DIR="$$(cd "$$SCRIPT_DIR/.." && pwd)"'; \
		echo 'CPU_LIB_DIR="$$APP_DIR/lib/precompiled/cpu/bin"'; \
		echo 'CUDA_LIB_DIR="$$APP_DIR/lib/precompiled/cuda/bin"'; \
		echo 'PLATFORM_CANDIDATES="/usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/qt6/plugins /lib/x86_64-linux-gnu/qt6/plugins"'; \
		echo ''; \
		echo 'select_cuda_dir() {'; \
		echo '    if [ -d "$$CUDA_LIB_DIR" ]; then'; \
		echo '        if command -v ldconfig >/dev/null 2>&1 && ldconfig -p 2>/dev/null | grep -q libcudart; then'; \
		echo '            echo "$$CUDA_LIB_DIR"'; \
		echo '            return'; \
		echo '        fi'; \
		echo '        for candidate in /usr/local/cuda*/targets/x86_64-linux/lib/libcudart.so*; do'; \
		echo '            if [ "$$candidate" = "/usr/local/cuda*/targets/x86_64-linux/lib/libcudart.so*" ]; then'; \
		echo '                break'; \
		echo '            fi'; \
		echo '            if [ -e "$$candidate" ]; then'; \
		echo '                echo "$$CUDA_LIB_DIR"'; \
		echo '                return'; \
		echo '            fi'; \
		echo '        done'; \
		echo '    fi'; \
		echo '    echo ""'; \
		echo '}'; \
		echo ''; \
		echo 'SELECTED_DIR="$$(select_cuda_dir)"'; \
		echo 'LIB_PATH="$$CPU_LIB_DIR"'; \
		echo 'if [ -n "$$SELECTED_DIR" ] && [ -d "$$SELECTED_DIR" ] && [ "$$SELECTED_DIR" != "$$CPU_LIB_DIR" ]; then'; \
		echo '    LIB_PATH="$$SELECTED_DIR:$$LIB_PATH"'; \
		echo 'fi'; \
		echo 'if [ -n "$$LD_LIBRARY_PATH" ]; then'; \
		echo '    export LD_LIBRARY_PATH="$$LIB_PATH:$$LD_LIBRARY_PATH"'; \
		echo 'else'; \
		echo '    export LD_LIBRARY_PATH="$$LIB_PATH"'; \
		echo 'fi'; \
		echo ''; \
		echo 'if [ -z "$$QT_QPA_PLATFORM_PLUGIN_PATH" ]; then'; \
		echo '    for candidate in $$PLATFORM_CANDIDATES; do'; \
		echo '        if [ -d "$$candidate/platforms" ]; then'; \
		echo '            export QT_QPA_PLATFORM_PLUGIN_PATH="$$candidate/platforms"'; \
		echo '            break'; \
		echo '        fi'; \
		echo '    done'; \
		echo 'fi'; \
		echo 'if [ -n "$$QT_QPA_PLATFORM_PLUGIN_PATH" ] && [ ! -f "$$QT_QPA_PLATFORM_PLUGIN_PATH/libqxcb.so" ]; then'; \
		echo '    if [ -n "$$WAYLAND_DISPLAY" ] || [ "$$XDG_SESSION_TYPE" = "wayland" ]; then'; \
		echo '        export QT_QPA_PLATFORM="wayland"'; \
		echo '    else'; \
		echo '        echo "Qt xcb platform plugin (libqxcb.so) not found in $$QT_QPA_PLATFORM_PLUGIN_PATH ($$QT_QPA_PLATFORM_PLUGIN_PATH)." >&2'; \
		echo '        echo "Install a Qt 6 XCB platform plugin or run under a Wayland session." >&2'; \
		echo '        exit 1'; \
		echo '    fi'; \
		echo 'fi'; \
		echo ''; \
		echo 'exec "$$SCRIPT_DIR/'"$(WRAPPED_BINARY)"'" "$$@"'; \
	} > $(BIN_DIR)/run_aifilesorter.sh
	@chmod 755 $(BIN_DIR)/run_aifilesorter.sh

install: $(TARGET)
ifeq ($(PLATFORM), Linux)
	@echo "Installing runtime payload into $(INSTALL_LIB_DIR)..."
	mkdir -p $(INSTALL_LIB_DIR)/bin
	mkdir -p $(INSTALL_LIB_DIR)/lib
	cp $(TARGET) $(INSTALL_LIB_DIR)/bin/$(WRAPPED_BINARY)
	rm -rf $(INSTALL_LIB_DIR)/lib/precompiled
	cp -a lib/precompiled $(INSTALL_LIB_DIR)/lib/
	if [ -f resources/certs/cacert.pem ]; then \
		mkdir -p $(INSTALL_LIB_DIR)/certs; \
		cp resources/certs/cacert.pem $(INSTALL_LIB_DIR)/certs/cacert.pem; \
	fi
	@echo "Installing launcher script to $(INSTALL_DIR)..."
	mkdir -p $(INSTALL_DIR)
	@{ \
		echo '#!/bin/sh'; \
		echo 'APP_DIR="'$(INSTALL_LIB_DIR)'"'; \
		echo 'CPU_LIB_DIR="$$APP_DIR/lib/precompiled/cpu/bin"'; \
		echo 'CUDA_LIB_DIR="$$APP_DIR/lib/precompiled/cuda/bin"'; \
		echo 'PLATFORM_CANDIDATES="/usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/qt6/plugins /lib/x86_64-linux-gnu/qt6/plugins"'; \
		echo ''; \
		echo 'select_cuda_dir() {'; \
		echo '    if [ -d "$$CUDA_LIB_DIR" ]; then'; \
		echo '        if command -v ldconfig >/dev/null 2>&1 && ldconfig -p 2>/dev/null | grep -q libcudart; then'; \
		echo '            echo "$$CUDA_LIB_DIR"'; \
		echo '            return'; \
		echo '        fi'; \
		echo '        for candidate in /usr/local/cuda*/targets/x86_64-linux/lib/libcudart.so*; do'; \
		echo '            if [ "$$candidate" = "/usr/local/cuda*/targets/x86_64-linux/lib/libcudart.so*" ]; then'; \
		echo '                break'; \
		echo '            fi'; \
		echo '            if [ -e "$$candidate" ]; then'; \
		echo '                echo "$$CUDA_LIB_DIR"'; \
		echo '                return'; \
		echo '            fi'; \
		echo '        done'; \
		echo '    fi'; \
		echo '    echo ""'; \
		echo '}'; \
		echo ''; \
		echo 'SELECTED_DIR="$$(select_cuda_dir)"'; \
		echo 'LIB_PATH="$$CPU_LIB_DIR"'; \
		echo 'if [ -n "$$SELECTED_DIR" ] && [ -d "$$SELECTED_DIR" ] && [ "$$SELECTED_DIR" != "$$CPU_LIB_DIR" ]; then'; \
		echo '    LIB_PATH="$$SELECTED_DIR:$$LIB_PATH"'; \
		echo 'fi'; \
		echo 'if [ -n "$$LD_LIBRARY_PATH" ]; then'; \
		echo '    export LD_LIBRARY_PATH="$$LIB_PATH:$$LD_LIBRARY_PATH"'; \
		echo 'else'; \
		echo '    export LD_LIBRARY_PATH="$$LIB_PATH"'; \
		echo 'fi'; \
		echo ''; \
		echo 'if [ -z "$$QT_QPA_PLATFORM_PLUGIN_PATH" ]; then'; \
		echo '    for candidate in $$PLATFORM_CANDIDATES; do'; \
		echo '        if [ -d "$$candidate/platforms" ]; then'; \
		echo '            export QT_QPA_PLATFORM_PLUGIN_PATH="$$candidate/platforms"'; \
		echo '            break'; \
		echo '        fi'; \
		echo '    done'; \
		echo 'fi'; \
		echo 'if [ -n "$$QT_QPA_PLATFORM_PLUGIN_PATH" ] && [ ! -f "$$QT_QPA_PLATFORM_PLUGIN_PATH/libqxcb.so" ]; then'; \
		echo '    if [ -n "$$WAYLAND_DISPLAY" ] || [ "$$XDG_SESSION_TYPE" = "wayland" ]; then'; \
		echo '        export QT_QPA_PLATFORM="wayland"'; \
		echo '    else'; \
		echo '        echo "Qt xcb platform plugin (libqxcb.so) not found in $$QT_QPA_PLATFORM_PLUGIN_PATH ($$QT_QPA_PLATFORM_PLUGIN_PATH)." >&2'; \
		echo '        echo "Install a Qt 6 XCB platform plugin or run under a Wayland session." >&2'; \
		echo '        exit 1'; \
		echo '    fi'; \
		echo 'fi'; \
		echo ''; \
		echo 'exec "$$APP_DIR/bin/'"$(WRAPPED_BINARY)"'" "$$@"'; \
	} > $(INSTALL_DIR)/aifilesorter
	chmod 755 $(INSTALL_DIR)/aifilesorter
	@echo "Installation complete."

else ifeq ($(PLATFORM), MacOS)
	@echo "Installing binary to $(INSTALL_DIR)..."
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/aifilesorter

	@echo "Installing libraries to $(INSTALL_LIB_DIR)..."
	mkdir -p $(INSTALL_LIB_DIR)
	cp lib/precompiled/libggml-base.dylib $(INSTALL_LIB_DIR)
	cp lib/precompiled/libggml-blas.dylib $(INSTALL_LIB_DIR)
	cp lib/precompiled/libggml-cpu.dylib $(INSTALL_LIB_DIR)
	cp lib/precompiled/libggml-metal.dylib $(INSTALL_LIB_DIR)
	cp lib/precompiled/libggml.dylib $(INSTALL_LIB_DIR)
	cp lib/precompiled/libmtmd.dylib $(INSTALL_LIB_DIR)
	cp lib/precompiled/libllama.dylib $(INSTALL_LIB_DIR)

	install_name_tool -add_rpath $(INSTALL_LIB_DIR) $(INSTALL_DIR)/aifilesorter

	@echo "macOS installation complete."
endif

uninstall:
ifeq ($(PLATFORM), Linux)
	@echo "Uninstalling aifilesorter binary and libraries..."

	@echo "Removing binary from /usr/local/bin..."
	rm -f /usr/local/bin/aifilesorter

	@echo "Removing libraries from /usr/local/lib/aifilesorter..."
	rm -rf /usr/local/lib/aifilesorter

	@echo "Removing ld config file..."
	rm -f /etc/ld.so.conf.d/aifilesorter.conf

	@echo "Running ldconfig..."
	ldconfig

	@echo "Core uninstallation complete."

	@bash -c 'read -p "Do you also want to delete the downloaded local LLM models in ~/.local/share/aifilesorter/llms/? [y/N] " ans; \
		if [ "$$ans" = "y" ] || [ "$$ans" = "Y" ]; then \
			echo "Deleting ~/.local/share/aifilesorter/llms/..."; \
			rm -rf "$$HOME/.local/share/aifilesorter/llms"; \
		else \
			echo "Keeping downloaded models."; \
		fi'


else ifeq ($(PLATFORM), MacOS)
	@echo "Uninstalling aifilesorter binary and libraries on macOS..."

	@echo "Removing binary from $(INSTALL_DIR)..."
	rm -f $(INSTALL_DIR)/aifilesorter

	@echo "Removing installed libraries..."
	rm -f $(INSTALL_LIB_DIR)/libggml-base.dylib
	rm -f $(INSTALL_LIB_DIR)/libggml-blas.dylib
	rm -f $(INSTALL_LIB_DIR)/libggml-cpu.dylib
	rm -f $(INSTALL_LIB_DIR)/libggml-metal.dylib
	rm -f $(INSTALL_LIB_DIR)/libggml.dylib
	rm -f $(INSTALL_LIB_DIR)/libmtmd.dylib
	rm -f $(INSTALL_LIB_DIR)/libllama.dylib

	@echo "Core uninstallation complete."

	@read -p "Do you also want to delete the downloaded local LLM models in ~/Library/Application\ Support/aifilesorter/llms/? [y/N] " ans; \
	if [ "$$ans" = "y" ] || [ "$$ans" = "Y" ]; then \
		echo "Deleting ~/Library/Application Support/aifilesorter/llms/..."; \
		rm -rf "$$HOME/Library/Application Support/aifilesorter/llms"; \
	else \
		echo "Keeping downloaded models."; \
	fi
endif
