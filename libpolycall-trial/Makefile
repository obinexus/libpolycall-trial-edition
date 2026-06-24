# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Werror -I./include -fPIC -std=c11
LDFLAGS := -pthread -lssl -lcrypto

# Version info
VERSION := 1.0.0
SOVERSION := 1

# Platform-specific settings
ifeq ($(OS),Windows_NT)
	LDFLAGS += -lws2_32
	CFLAGS += -D_WIN32
	SHARED_EXT := dll
	EXE_EXT := .exe
	RM := del /Q
	MKDIR := mkdir
else
	SHARED_EXT := so
	EXE_EXT :=
	RM := rm -rf
	MKDIR := mkdir -p
	# Add rpath for shared library
	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib'
endif

# Build type flags
DEBUG_FLAGS := -g -DDEBUG -O0 -fsanitize=address
RELEASE_FLAGS := -O2 -DNDEBUG
PROFILE_FLAGS := -pg -O2

# Directories
SRC_DIRS := src/core src/cli src/micro src/network src/parser src/protocol src/state
INC_DIR := include
BUILD_DIR := build
LIB_DIR := lib
BIN_DIR := bin
TEST_DIR := tests
DOC_DIR := docs

# Find all source files
SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# Test files
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst %.c,$(BUILD_DIR)/%,$(TEST_SRCS))

# Library and executable names
LIB_NAME := libpolycall
STATIC_LIB := $(LIB_DIR)/$(LIB_NAME).a
SHARED_LIB := $(LIB_DIR)/$(LIB_NAME).$(SHARED_EXT).$(VERSION)
SHARED_LIB_SONAME := $(LIB_DIR)/$(LIB_NAME).$(SHARED_EXT).$(SOVERSION)
SHARED_LIB_LINK := $(LIB_DIR)/$(LIB_NAME).$(SHARED_EXT)

# CLI executable
CLI_SRC := src/cli/main.c
CLI_OBJ := $(BUILD_DIR)/src/cli/main.o
EXECUTABLE := $(BIN_DIR)/polycall$(EXE_EXT)

# Installation paths
PREFIX := /usr/local
INSTALL_INC_DIR := $(PREFIX)/include/$(LIB_NAME)
INSTALL_LIB_DIR := $(PREFIX)/lib
INSTALL_BIN_DIR := $(PREFIX)/bin

# Default target
.PHONY: all
all: dirs $(STATIC_LIB) $(SHARED_LIB) $(EXECUTABLE)

# Create necessary directories
.PHONY: dirs
dirs:
	@$(MKDIR) $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR)
	@$(foreach dir,$(SRC_DIRS),$(MKDIR) $(BUILD_DIR)/$(dir);)

# Build variants
.PHONY: debug release profile
debug: CFLAGS += $(DEBUG_FLAGS)
debug: all

release: CFLAGS += $(RELEASE_FLAGS)
release: all

profile: CFLAGS += $(PROFILE_FLAGS)
profile: all

# Compilation rules
$(BUILD_DIR)/%.o: %.c
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Library creation
$(STATIC_LIB): $(filter-out $(BUILD_DIR)/src/cli/%,$(OBJS))
	@$(MKDIR) $(dir $@)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(filter-out $(BUILD_DIR)/src/cli/%,$(OBJS))
	@$(MKDIR) $(dir $@)
	$(CC) -shared -Wl,-soname,$(notdir $(SHARED_LIB_SONAME)) -o $@ $^ $(LDFLAGS)
	@cd $(LIB_DIR) && ln -sf $(notdir $(SHARED_LIB)) $(notdir $(SHARED_LIB_SONAME))
	@cd $(LIB_DIR) && ln -sf $(notdir $(SHARED_LIB_SONAME)) $(notdir $(SHARED_LIB_LINK))

# Executable creation
$(EXECUTABLE): $(CLI_OBJ) $(STATIC_LIB)
	@$(MKDIR) $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS) -L$(LIB_DIR) -l:$(notdir $(STATIC_LIB))

# Tests
.PHONY: test
test: CFLAGS += $(DEBUG_FLAGS)
test: $(TEST_BINS)
	@for test in $(TEST_BINS); do ./$$test; done

$(BUILD_DIR)/$(TEST_DIR)/%: $(BUILD_DIR)/$(TEST_DIR)/%.o $(STATIC_LIB)
	$(CC) $^ -o $@ $(LDFLAGS)

# Documentation
.PHONY: doc
doc:
	doxygen Doxyfile

# Installation
.PHONY: install uninstall
install: all doc
ifneq ($(OS),Windows_NT)
	@$(MKDIR) $(INSTALL_INC_DIR) $(INSTALL_LIB_DIR) $(INSTALL_BIN_DIR)
	cp -r $(INC_DIR)/* $(INSTALL_INC_DIR)
	cp $(STATIC_LIB) $(SHARED_LIB) $(INSTALL_LIB_DIR)
	cd $(INSTALL_LIB_DIR) && ln -sf $(notdir $(SHARED_LIB)) $(notdir $(SHARED_LIB_SONAME))
	cd $(INSTALL_LIB_DIR) && ln -sf $(notdir $(SHARED_LIB_SONAME)) $(notdir $(SHARED_LIB_LINK))
	cp $(EXECUTABLE) $(INSTALL_BIN_DIR)
	ldconfig
endif

uninstall:
ifneq ($(OS),Windows_NT)
	$(RM) $(INSTALL_INC_DIR)
	$(RM) $(INSTALL_LIB_DIR)/$(notdir $(SHARED_LIB))* $(INSTALL_LIB_DIR)/$(notdir $(STATIC_LIB))
	$(RM) $(INSTALL_BIN_DIR)/$(notdir $(EXECUTABLE))
endif

# Cleaning
.PHONY: clean distclean
clean:
	$(RM) $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR)

distclean: clean
	$(RM) $(DOC_DIR)
	$(MAKE) uninstall

# Include generated dependencies
-include $(DEPS)

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all        - Build everything (default)"
	@echo "  debug      - Build with debug flags"
	@echo "  release    - Build with release flags"
	@echo "  profile    - Build with profiling flags"
	@echo "  test       - Build and run tests"
	@echo "  doc        - Generate documentation"
	@echo "  clean      - Remove build files"
	@echo "  install    - Install libraries and headers"
	@echo "  uninstall  - Remove installed files"
	@echo "  distclean  - Remove all generated files"
	@echo "  help       - Show this help message"