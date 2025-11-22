# Detect platform
UNAME := $(shell uname | cut -d'-' -f1)
UNAME_M := $(shell uname -m)

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
	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lggml-base -lX11 -pthread
	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled/cpu/bin'
	LDFLAGS += -Wl,-rpath-link=./lib/precompiled/cpu/bin

else ifeq ($(UNAME), Darwin)
    IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
    DEFAULT_BREW_PREFIX := $(shell if [ "$(IS_APPLE_SILICON)" = "1" ]; then echo /opt/homebrew; else echo /usr/local; fi)
    BREW_PREFIX := $(shell if command -v brew >/dev/null 2>&1; then brew --prefix; else echo $(DEFAULT_BREW_PREFIX); fi)
    QT_PREFIX := $(BREW_PREFIX)/opt/qt
    QTBASE_PREFIX := $(BREW_PREFIX)/opt/qtbase
    CURL_PREFIX := $(BREW_PREFIX)/opt/curl
    LIBFFI_PREFIX := $(BREW_PREFIX)/opt/libffi
    EXPAT_PREFIX := $(BREW_PREFIX)/opt/expat
    SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
    MACOSX_DEPLOYMENT_TARGET ?= 11.0

    export MACOSX_DEPLOYMENT_TARGET
    PATH := $(QTBASE_PREFIX)/share/qt/libexec:$(QT_PREFIX)/bin:$(CURL_PREFIX)/bin:$(PATH)
    export PATH

    export PKG_CONFIG_PATH := $(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$(LIBFFI_PREFIX)/lib/pkgconfig:$(EXPAT_PREFIX)/lib/pkgconfig:$(QT_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)

    export LDFLAGS += -L$(LIBFFI_PREFIX)/lib
    export CPPFLAGS += -I$(LIBFFI_PREFIX)/include
    PLATFORM := MacOS
    CXXFLAGS += -DMACOS -DENABLE_METAL -DGGML_USE_METAL -Wno-deprecated -Iinclude/llama -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET)
    ifneq ($(strip $(SDKROOT)),)
    export SDKROOT
    CXXFLAGS += -isysroot $(SDKROOT) -stdlib=libc++ -I$(SDKROOT)/usr/include/c++/v1
    LDFLAGS += -isysroot $(SDKROOT)
    endif
    LDFLAGS += -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET)
    TARGET := $(BIN_DIR)/aifilesorter
    INSTALL_DIR := /usr/local/bin
	INSTALL_LIB_DIR := /usr/local/lib

    SPDLOG_PATH := $(BREW_PREFIX)/include
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

    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lggml-base -lintl -pthread
	LDFLAGS += -framework Metal -framework Foundation
    LDFLAGS += -Wl,-rpath,@loader_path/lib -Wl,-rpath,@loader_path/../lib/precompiled


endif

WRAPPED_BINARY := $(notdir $(TARGET))

# Compiler and flags
ifeq ($(UNAME), Darwin)
CXX := clang++
else
CXX := g++
endif
CXXFLAGS += -std=c++20 -Wall -O2 -fPIC
CXX_VERSION_INFO := $(shell $(CXX) --version 2>/dev/null)
# Suppress GCC-only diagnostics; clang (macOS) does not support some of them
ifneq (,$(findstring clang,$(CXX_VERSION_INFO)))
CXXFLAGS += -Wno-array-bounds
else
CXXFLAGS += -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread
endif
INCLUDE_DIRS = -I./include -I./include/llama
LIB_DIRS =
ifeq ($(UNAME), Linux)
LIB_DIRS += -L./lib/precompiled/cpu/bin
else
LIB_DIRS += -L./lib/precompiled
endif
ifeq ($(UNAME), Darwin)
LIB_DIRS += -L$(BREW_PREFIX)/lib
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
ifeq ($(strip $(RCC)),)
RCC := $(wildcard /usr/lib/qt6/libexec/rcc)
endif
ifeq ($(strip $(RCC)),)
RCC := $(wildcard /opt/homebrew/opt/qtbase/share/qt/libexec/rcc)
endif
ifeq ($(strip $(RCC)),)
RCC := $(wildcard /usr/local/opt/qtbase/share/qt/libexec/rcc)
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
	@python3 scripts/gen_run_wrapper.py \
		--mode dev \
		--binary '$(WRAPPED_BINARY)' \
		--output '$(BIN_DIR)/run_aifilesorter.sh'
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
	@python3 scripts/gen_run_wrapper.py \
		--mode install \
		--install-app-dir '$(INSTALL_LIB_DIR)' \
		--binary '$(WRAPPED_BINARY)' \
		--output '$(INSTALL_DIR)/run_aifilesorter.sh'
	chmod 755 $(INSTALL_DIR)/run_aifilesorter.sh
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
