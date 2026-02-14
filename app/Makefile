# Detect platform
UNAME := $(shell uname | cut -d'-' -f1)
UNAME_M := $(shell uname -m)

BIN_DIR := ./bin
OBJ_DIR := ./obj
SRC_DIR := ./lib
APP_NAME ?= AI File Sorter

# Optional: build llama.cpp with multi-variant CPU backends on macOS.
# Usage: make LLAMA_MULTI_VARIANT=1
LLAMA_MULTI_VARIANT ?= 0
GGML_PRECOMPILED_SUBDIR ?= precompiled

ifeq ($(UNAME), Linux)
    PLATFORM := Linux
    CXXFLAGS += -DLINUX
    CXXFLAGS += -DAI_FILE_SORTER_HAS_MTMD
    TARGET := $(BIN_DIR)/aifilesorter-bin
    INSTALL_DIR := /usr/local/bin
    INSTALL_LIB_DIR := /usr/local/lib/aifilesorter
	LD_CONF_FILE := /etc/ld.so.conf.d/aifilesorter.conf

    # --- Qt6 setup ---
    PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)
    QT_PACKAGES := Qt6Widgets Qt6Gui Qt6Core
    QT_CXXFLAGS :=
    QT_LDFLAGS :=
ifneq ($(strip $(PKG_CONFIG)),)
    QT_CXXFLAGS := $(shell $(PKG_CONFIG) --cflags $(QT_PACKAGES) 2>/dev/null)
    QT_LDFLAGS := $(shell $(PKG_CONFIG) --libs $(QT_PACKAGES) 2>/dev/null)
endif
ifeq ($(strip $(QT_CXXFLAGS)),)
    QT_INCLUDE_BASE ?= /usr/include/x86_64-linux-gnu/qt6
    QT_LIB_BASE     ?= /usr/lib/x86_64-linux-gnu
    CXXFLAGS += -I$(QT_INCLUDE_BASE) -I$(QT_INCLUDE_BASE)/QtCore -I$(QT_INCLUDE_BASE)/QtGui -I$(QT_INCLUDE_BASE)/QtWidgets
    LDFLAGS  += -L$(QT_LIB_BASE) -lQt6Widgets -lQt6Gui -lQt6Core
else
    CXXFLAGS += $(QT_CXXFLAGS)
    LDFLAGS  += $(QT_LDFLAGS)
endif

    # --- Other dependencies ---
	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lggml-base -lmtmd -lX11 -pthread
	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled/cpu/bin' -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
	LDFLAGS += -Wl,-rpath-link=./lib/precompiled/cpu/bin -Wl,-rpath-link=./lib/precompiled

else ifeq ($(UNAME), Darwin)
    MACOS_ARCH ?= $(UNAME_M)
    DEFAULT_BREW_PREFIX := $(shell if [ "$(MACOS_ARCH)" = "arm64" ]; then echo /opt/homebrew; else echo /usr/local; fi)
ifeq ($(origin BREW_PREFIX),undefined)
    ifeq ($(MACOS_ARCH),x86_64)
        BREW_PREFIX := /usr/local
    else
        BREW_PREFIX := $(shell if command -v brew >/dev/null 2>&1; then brew --prefix; else echo $(DEFAULT_BREW_PREFIX); fi)
    endif
endif
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

ifeq ($(MACOS_ARCH),x86_64)
    export PKG_CONFIG_PATH := $(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$(LIBFFI_PREFIX)/lib/pkgconfig:$(EXPAT_PREFIX)/lib/pkgconfig:$(QT_PREFIX)/lib/pkgconfig
    CPPFLAGS := $(filter-out -I/opt/homebrew/%,$(CPPFLAGS))
    LDFLAGS := $(filter-out -L/opt/homebrew/% -F/opt/homebrew/%,$(LDFLAGS))
else
    export PKG_CONFIG_PATH := $(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$(LIBFFI_PREFIX)/lib/pkgconfig:$(EXPAT_PREFIX)/lib/pkgconfig:$(QT_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
endif

    export LDFLAGS += -L$(LIBFFI_PREFIX)/lib
    export CPPFLAGS += -I$(LIBFFI_PREFIX)/include
    PLATFORM := MacOS
    CXXFLAGS += -DMACOS -DENABLE_METAL -DGGML_USE_METAL -DAI_FILE_SORTER_HAS_MTMD -Wno-deprecated -Iinclude/llama -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET)
    ifneq ($(strip $(MACOS_ARCH)),)
    CXXFLAGS := $(filter-out -arch arm64 -arch x86_64,$(CXXFLAGS))
    LDFLAGS := $(filter-out -arch arm64 -arch x86_64,$(LDFLAGS))
    CXXFLAGS += -arch $(MACOS_ARCH)
    LDFLAGS += -arch $(MACOS_ARCH)
    endif
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

ifeq ($(origin PKG_CONFIG),undefined)
    ifeq ($(MACOS_ARCH),x86_64)
        ifneq ($(wildcard /usr/local/bin/pkg-config),)
            PKG_CONFIG := /usr/local/bin/pkg-config
        else
            PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)
        endif
    else
        PKG_CONFIG := $(shell command -v pkg-config 2>/dev/null)
    endif
endif
ifeq ($(strip $(PKG_CONFIG)),)
$(error pkg-config is required to locate Qt6 frameworks on macOS. Please install it (e.g. brew install pkg-config))
endif

    QT_PACKAGES := Qt6Widgets Qt6Gui Qt6Core
    QT_CXXFLAGS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" $(PKG_CONFIG) --cflags $(QT_PACKAGES))
    QT_LDFLAGS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" $(PKG_CONFIG) --libs $(QT_PACKAGES))
ifeq ($(strip $(QT_CXXFLAGS)),)
$(error Could not retrieve Qt6 flags via pkg-config. Ensure Qt6 is installed (brew install qt) and PKG_CONFIG_PATH includes its pkgconfig directory.)
endif
ifeq ($(MACOS_ARCH),x86_64)
ifneq ($(findstring /opt/homebrew,$(QT_CXXFLAGS) $(QT_LDFLAGS)),)
$(error x86_64 build resolved Qt flags from /opt/homebrew. Install x86_64 Qt under /usr/local (Intel Homebrew) or set BREW_PREFIX/PKG_CONFIG accordingly.)
endif
endif
    CXXFLAGS += $(QT_CXXFLAGS)
    LDFLAGS += $(QT_LDFLAGS)

    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -lggml-base -lmtmd -lintl -pthread
	LDFLAGS += -framework Metal -framework Foundation
    LDFLAGS += -Wl,-rpath,@loader_path/lib -Wl,-rpath,@loader_path/../lib/$(GGML_PRECOMPILED_SUBDIR)
    BIN_SUBDIR := $(patsubst ./bin/%,%,$(BIN_DIR))
    ifneq ($(BIN_SUBDIR),$(BIN_DIR))
    ifneq ($(strip $(BIN_SUBDIR)),)
    LDFLAGS += -Wl,-rpath,@loader_path/../../lib -Wl,-rpath,@loader_path/../../lib/$(GGML_PRECOMPILED_SUBDIR)
    endif
    endif


endif

ifeq ($(UNAME), Darwin)
ifneq ($(strip $(MACOS_ARCH)),)
OBJ_DIR := ./obj/$(MACOS_ARCH)
endif
endif

WRAPPED_BINARY := $(notdir $(TARGET))

# Compiler and flags
ifeq ($(UNAME), Darwin)
CXX := clang++
else
CXX := g++
endif
CXXFLAGS += -std=c++20 -Wall -O2 -fPIC '-DAI_FILE_SORTER_APP_NAME="$(APP_NAME)"' '-DAI_FILE_SORTER_GGML_SUBDIR="$(GGML_PRECOMPILED_SUBDIR)"'
CXX_VERSION_INFO := $(shell $(CXX) --version 2>/dev/null)
# Suppress GCC-only diagnostics; clang (macOS) does not support some of them
ifneq (,$(findstring clang,$(CXX_VERSION_INFO)))
CXXFLAGS += -Wno-array-bounds
else
CXXFLAGS += -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread
endif
INCLUDE_DIRS = -I./include -I./include/external/llama.cpp/include -I./include/external/llama.cpp/ggml/include -I./include/external/llama.cpp/tools/mtmd
LIB_DIRS =
ifeq ($(UNAME), Linux)
LIB_DIRS += -L./lib/precompiled/cpu/bin -L./lib/precompiled
else
LIB_DIRS += -L./lib/$(GGML_PRECOMPILED_SUBDIR)
endif
ifeq ($(UNAME), Darwin)
LIB_DIRS += -L$(BREW_PREFIX)/lib
endif

# --- Vendored document analysis dependencies ---
DOC_DEPS_DIR := ../external
LIBZIP_DIR := $(DOC_DEPS_DIR)/libzip
PUGIXML_DIR := $(DOC_DEPS_DIR)/pugixml
PDFIUM_DIR := $(DOC_DEPS_DIR)/pdfium

LIBZIP_CMAKE := $(LIBZIP_DIR)/CMakeLists.txt
LIBZIP_BUILD_DIR := $(LIBZIP_DIR)/build
LIBZIP_LIB := $(LIBZIP_BUILD_DIR)/lib/libzip.a
LIBZIP_CONF := $(LIBZIP_BUILD_DIR)/zipconf.h
LIBZIP_CMAKE_ARGS :=
ifeq ($(UNAME), Darwin)
ifneq ($(strip $(MACOS_ARCH)),)
LIBZIP_BUILD_DIR := $(LIBZIP_DIR)/build-$(MACOS_ARCH)
LIBZIP_LIB := $(LIBZIP_BUILD_DIR)/lib/libzip.a
LIBZIP_CMAKE_ARGS += -DCMAKE_OSX_ARCHITECTURES=$(MACOS_ARCH)
endif
endif

PUGIXML_HDR := $(PUGIXML_DIR)/src/pugixml.hpp

PDFIUM_PLATFORM_DIR :=
PDFIUM_INC :=
PDFIUM_LIB :=
ifeq ($(UNAME), Linux)
PDFIUM_PLATFORM_DIR := $(PDFIUM_DIR)/linux-x64
PDFIUM_INC := $(PDFIUM_PLATFORM_DIR)/include
PDFIUM_LIB := $(PDFIUM_PLATFORM_DIR)/lib/libpdfium.so
endif
ifeq ($(UNAME), Darwin)
PDFIUM_PLATFORM_DIR := $(PDFIUM_DIR)/macos-arm64
ifeq ($(MACOS_ARCH),x86_64)
PDFIUM_PLATFORM_DIR := $(PDFIUM_DIR)/macos-x64
endif
PDFIUM_INC := $(PDFIUM_PLATFORM_DIR)/include
PDFIUM_LIB := $(PDFIUM_PLATFORM_DIR)/lib/libpdfium.dylib
endif

DOC_LIBS :=
DOC_DEPS_HEADERS :=
ifneq ($(wildcard $(PUGIXML_HDR)),)
CXXFLAGS += -DPUGIXML_NO_EXCEPTIONS
CXXFLAGS += -DAI_FILE_SORTER_USE_PUGIXML
INCLUDE_DIRS += -I$(PUGIXML_DIR)/src
endif
ifneq ($(wildcard $(LIBZIP_CMAKE)),)
CXXFLAGS += -DAI_FILE_SORTER_USE_LIBZIP
INCLUDE_DIRS += -I$(LIBZIP_DIR)/lib -I$(LIBZIP_BUILD_DIR)
LDFLAGS += -lz
DOC_LIBS += $(LIBZIP_LIB)
DOC_DEPS_HEADERS += $(LIBZIP_CONF)
endif
ifneq ($(wildcard $(PDFIUM_LIB)),)
CXXFLAGS += -DAI_FILE_SORTER_USE_PDFIUM
INCLUDE_DIRS += -I$(PDFIUM_INC)
DOC_LIBS += $(PDFIUM_LIB)
endif

# Enable mtmd progress callback only when the linked libmtmd provides it.
MTMD_PROGRESS_CALLBACK ?= auto
MTMD_LIB_CANDIDATES :=
ifeq ($(UNAME), Linux)
MTMD_LIB_CANDIDATES := ./lib/precompiled/cpu/bin/libmtmd.so ./lib/precompiled/libmtmd.so
endif
ifeq ($(UNAME), Darwin)
MTMD_LIB_CANDIDATES := ./lib/$(GGML_PRECOMPILED_SUBDIR)/libmtmd.dylib $(BREW_PREFIX)/lib/libmtmd.dylib
endif
ifeq ($(MTMD_PROGRESS_CALLBACK),auto)
MTMD_PROGRESS_CALLBACK := $(shell \
	if command -v nm >/dev/null 2>&1; then \
		for lib in $(MTMD_LIB_CANDIDATES); do \
			if [ -e "$$lib" ] && nm -D --defined-only "$$lib" 2>/dev/null | grep -q "mtmd_helper_set_progress_callback"; then \
				echo 1; exit 0; \
			fi; \
			if [ -e "$$lib" ] && nm -g "$$lib" 2>/dev/null | grep -q "[Tt] _mtmd_helper_set_progress_callback"; then \
				echo 1; exit 0; \
			fi; \
		done; \
	elif command -v objdump >/dev/null 2>&1; then \
		for lib in $(MTMD_LIB_CANDIDATES); do \
			if [ -e "$$lib" ] && objdump -T "$$lib" 2>/dev/null | grep -q "mtmd_helper_set_progress_callback"; then \
				echo 1; exit 0; \
			fi; \
		done; \
	fi; \
	echo 0)
endif
ifeq ($(MTMD_PROGRESS_CALLBACK),1)
CXXFLAGS += -DAI_FILE_SORTER_MTMD_PROGRESS_CALLBACK
else
CXXFLAGS += -UAI_FILE_SORTER_MTMD_PROGRESS_CALLBACK
endif

# Enable mtmd log callback only when the linked libmtmd provides it.
MTMD_LOG_CALLBACK ?= auto
ifeq ($(MTMD_LOG_CALLBACK),auto)
MTMD_LOG_CALLBACK := $(shell \
	if command -v nm >/dev/null 2>&1; then \
		for lib in $(MTMD_LIB_CANDIDATES); do \
			if [ -e "$$lib" ] && nm -D --defined-only "$$lib" 2>/dev/null | grep -q "mtmd_helper_log_set"; then \
				echo 1; exit 0; \
			fi; \
			if [ -e "$$lib" ] && nm -g "$$lib" 2>/dev/null | grep -q "[Tt] _mtmd_helper_log_set"; then \
				echo 1; exit 0; \
			fi; \
		done; \
	elif command -v objdump >/dev/null 2>&1; then \
		for lib in $(MTMD_LIB_CANDIDATES); do \
			if [ -e "$$lib" ] && objdump -T "$$lib" 2>/dev/null | grep -q "mtmd_helper_log_set"; then \
				echo 1; exit 0; \
			fi; \
		done; \
	fi; \
	echo 0)
endif
ifeq ($(MTMD_LOG_CALLBACK),1)
CXXFLAGS += -DAI_FILE_SORTER_MTMD_LOG_CALLBACK
else
CXXFLAGS += -UAI_FILE_SORTER_MTMD_LOG_CALLBACK
endif

QRC_FILE := resources/app.qrc
QRC_CPP := $(OBJ_DIR)/qrc_app.cpp
QRC_OBJ := $(OBJ_DIR)/qrc_app.o
QRC_DIR := $(dir $(QRC_FILE))
QRC_RESOURCES := $(shell sed -n -E 's|.*<file>([^<]*)</file>.*|\1|p' $(QRC_FILE))
QRC_RESOURCES := $(addprefix $(QRC_DIR),$(QRC_RESOURCES))

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

PRECOMPILED_LLAMA :=
ifeq ($(UNAME), Darwin)
ifneq ($(LLAMA_MULTI_VARIANT),0)
PRECOMPILED_LLAMA := precompiled_llama
endif
endif

.PHONY: all clean install uninstall doc_runtime_libs precompiled_llama MACOS_LLAMA_M1 MACOS_LLAMA_M2 MACOS_LLAMA_INTEL

# Main rules
all: $(PRECOMPILED_LLAMA) $(TARGET) doc_runtime_libs
	@printf "\nFinished building AI File Sorter for %s\n" "$(PLATFORM)"

$(TARGET): $(OBJS) $(QRC_OBJ) $(DOC_LIBS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_DIRS) $(LDFLAGS)
ifeq ($(PLATFORM), Linux)
	@$(MAKE) create_run_wrapper
endif

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(DOC_DEPS_HEADERS)
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(LIBZIP_CONF): $(LIBZIP_LIB)

$(LIBZIP_LIB):
	@echo "Building libzip (vendored)..."
	mkdir -p $(LIBZIP_BUILD_DIR)
	LDFLAGS= cmake -S $(LIBZIP_DIR) -B $(LIBZIP_BUILD_DIR) \
		$(LIBZIP_CMAKE_ARGS) \
		-DCMAKE_EXE_LINKER_FLAGS= \
		-DCMAKE_SHARED_LINKER_FLAGS= \
		-DCMAKE_MODULE_LINKER_FLAGS= \
		-DBUILD_SHARED_LIBS=OFF \
		-DENABLE_BZIP2=OFF \
		-DENABLE_LZMA=OFF \
		-DENABLE_ZSTD=OFF \
		-DENABLE_OPENSSL=OFF \
		-DENABLE_GNUTLS=OFF \
		-DENABLE_MBEDTLS=OFF \
		-DENABLE_COMMONCRYPTO=OFF \
		-DENABLE_WINDOWS_CRYPTO=OFF
	LDFLAGS= cmake --build $(LIBZIP_BUILD_DIR)

doc_runtime_libs:
ifneq ($(wildcard $(PDFIUM_LIB)),)
	@echo "Staging PDFium runtime library..."
	mkdir -p ./lib/$(GGML_PRECOMPILED_SUBDIR)
	cp $(PDFIUM_LIB) ./lib/$(GGML_PRECOMPILED_SUBDIR)/
endif

precompiled_llama:
ifeq ($(UNAME), Darwin)
	@echo "Checking llama.cpp precompiled libraries (multi-variant=$(LLAMA_MULTI_VARIANT))..."
	@if [ -f "./lib/$(GGML_PRECOMPILED_SUBDIR)/libllama.dylib" ]; then \
		echo "Using existing precompiled libs in ./lib/$(GGML_PRECOMPILED_SUBDIR)"; \
	else \
		echo "Building llama.cpp precompiled libraries (multi-variant=$(LLAMA_MULTI_VARIANT))..."; \
		LLAMA_MACOS_MULTI_VARIANT=$(LLAMA_MULTI_VARIANT) LLAMA_PRECOMPILED_DIR=./lib/$(GGML_PRECOMPILED_SUBDIR) ./scripts/build_llama_macos.sh; \
	fi
else
	@echo "precompiled_llama is only supported on macOS."
endif

MACOS_LLAMA_M1:
ifeq ($(UNAME), Darwin)
	@echo "Checking llama.cpp for M1-safe multi-variant CPU backends..."
	@if [ -f "./lib/precompiled-m1/libllama.dylib" ]; then \
		echo "Using existing precompiled libs in ./lib/precompiled-m1"; \
	else \
		echo "Building llama.cpp for M1-safe multi-variant CPU backends..."; \
		LLAMA_MACOS_MULTI_VARIANT=1 LLAMA_PRECOMPILED_DIR=./lib/precompiled-m1 ./scripts/build_llama_macos.sh; \
	fi
	$(MAKE) LLAMA_MULTI_VARIANT=0 GGML_PRECOMPILED_SUBDIR=precompiled-m1 APP_NAME="AI File Sorter for Mac M1" BIN_DIR=./bin/m1 all
else
	@echo "MACOS_LLAMA_M1 is only supported on macOS."
endif

MACOS_LLAMA_M2:
ifeq ($(UNAME), Darwin)
	@echo "Checking llama.cpp for native (fast) Apple Silicon backends..."
	@if [ -f "./lib/precompiled-m2/libllama.dylib" ]; then \
		echo "Using existing precompiled libs in ./lib/precompiled-m2"; \
	else \
		echo "Building llama.cpp for native (fast) Apple Silicon backends..."; \
		LLAMA_MACOS_MULTI_VARIANT=0 LLAMA_PRECOMPILED_DIR=./lib/precompiled-m2 ./scripts/build_llama_macos.sh; \
	fi
	$(MAKE) LLAMA_MULTI_VARIANT=0 GGML_PRECOMPILED_SUBDIR=precompiled-m2 APP_NAME="AI File Sorter for Mac M2/M3" BIN_DIR=./bin/m2 all
else
	@echo "MACOS_LLAMA_M2 is only supported on macOS."
endif

MACOS_LLAMA_INTEL:
ifeq ($(UNAME), Darwin)
	@echo "Checking llama.cpp for Intel (x86_64) macOS backends..."
	@if [ -f "./lib/precompiled-intel/libllama.dylib" ]; then \
		echo "Using existing precompiled libs in ./lib/precompiled-intel"; \
	else \
		echo "Building llama.cpp for Intel (x86_64) macOS backends..."; \
		LLAMA_MACOS_ARCH=x86_64 LLAMA_MACOS_ENABLE_METAL=1 LLAMA_MACOS_MULTI_VARIANT=0 LLAMA_PRECOMPILED_DIR=./lib/precompiled-intel ./scripts/build_llama_macos.sh; \
	fi
	$(MAKE) LLAMA_MULTI_VARIANT=0 GGML_PRECOMPILED_SUBDIR=precompiled-intel APP_NAME="AI File Sorter for Mac Intel" BIN_DIR=./bin/intel MACOS_ARCH=x86_64 all
else
	@echo "MACOS_LLAMA_INTEL is only supported on macOS."
endif

$(OBJ_DIR)/main.o: main.cpp $(DOC_DEPS_HEADERS)
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(QRC_CPP): $(QRC_FILE) $(QRC_RESOURCES)
	mkdir -p $(OBJ_DIR)
	$(RCC) -o $@ $<

$(QRC_OBJ): $(QRC_CPP) $(DOC_DEPS_HEADERS)
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
	rm -rf $(INSTALL_LIB_DIR)/lib/$(GGML_PRECOMPILED_SUBDIR)
	cp -a lib/$(GGML_PRECOMPILED_SUBDIR) $(INSTALL_LIB_DIR)/lib/
	if [ -f ../external/pdfium/linux-x64/lib/libpdfium.so ]; then \
		cp ../external/pdfium/linux-x64/lib/libpdfium.so $(INSTALL_LIB_DIR)/lib/$(GGML_PRECOMPILED_SUBDIR)/; \
	fi
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
	ln -sf $(INSTALL_DIR)/run_aifilesorter.sh $(INSTALL_DIR)/aifilesorter
	@echo "Installation complete."

else ifeq ($(PLATFORM), MacOS)
	@echo "Installing binary to $(INSTALL_DIR)..."
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/aifilesorter

	@echo "Installing libraries to $(INSTALL_LIB_DIR)..."
	mkdir -p $(INSTALL_LIB_DIR)
	cp lib/$(GGML_PRECOMPILED_SUBDIR)/libggml-base.dylib $(INSTALL_LIB_DIR)
	cp lib/$(GGML_PRECOMPILED_SUBDIR)/libggml-blas.dylib $(INSTALL_LIB_DIR)
	cp lib/$(GGML_PRECOMPILED_SUBDIR)/libggml-cpu.dylib $(INSTALL_LIB_DIR)
	cp lib/$(GGML_PRECOMPILED_SUBDIR)/libggml-metal.dylib $(INSTALL_LIB_DIR)
	cp lib/$(GGML_PRECOMPILED_SUBDIR)/libggml.dylib $(INSTALL_LIB_DIR)
	cp lib/$(GGML_PRECOMPILED_SUBDIR)/libmtmd.dylib $(INSTALL_LIB_DIR)
	cp lib/$(GGML_PRECOMPILED_SUBDIR)/libllama.dylib $(INSTALL_LIB_DIR)
	if [ -n "$(PDFIUM_LIB)" ] && [ -f "$(PDFIUM_LIB)" ]; then \
		cp "$(PDFIUM_LIB)" $(INSTALL_LIB_DIR); \
	fi

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
