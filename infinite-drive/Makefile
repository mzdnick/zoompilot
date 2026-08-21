NAME       := infinite-drive
SRC_DIR    := src
BUILD      := build
RAYLIB_DIR := third_party/raylib
RAYLIB_VER := 6.0

SRC := $(wildcard $(SRC_DIR)/*.c)
UNAME := $(shell uname -s)

CFLAGS := -O2 -std=c11 -Wall -Wextra -I$(SRC_DIR)
LDFLAGS :=
LDLIBS := -lm

ifeq ($(UNAME),Darwin)
  CFLAGS += -DGL_SILENCE_DEPRECATION
  LDFLAGS += -framework OpenGL -framework IOKit -framework Cocoa
  NPROC := $(shell sysctl -n hw.ncpu 2>/dev/null || echo 4)
endif
ifeq ($(UNAME),Linux)
  LDLIBS += -lGL -lpthread -ldl -lrt
  NPROC := $(shell nproc 2>/dev/null || echo 4)
endif

ifneq ($(shell pkg-config --exists raylib 2>/dev/null && echo yes),yes)
  USE_BUNDLED := 1
  RAYLIB_A := $(RAYLIB_DIR)/src/libraylib.a
  RAYLIB_H := $(RAYLIB_DIR)/src/raylib.h
  CFLAGS += -I$(RAYLIB_DIR)/src
  LDLIBS := $(RAYLIB_A) $(LDLIBS)
endif

OBJ := $(SRC:$(SRC_DIR)/%.c=$(BUILD)/%.o)
DEP := $(OBJ:.o=.d)

all: $(BUILD)/$(NAME)

$(BUILD)/$(NAME): $(OBJ) $(RAYLIB_A)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)

ifeq ($(USE_BUNDLED),1)
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(RAYLIB_H)
else
$(BUILD)/%.o: $(SRC_DIR)/%.c
endif
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)

$(RAYLIB_A): | $(RAYLIB_H)
	$(MAKE) -C $(RAYLIB_DIR)/src PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC -j$(NPROC)

$(RAYLIB_H):
	@mkdir -p $(RAYLIB_DIR)
	git clone --depth 1 --branch $(RAYLIB_VER) https://github.com/raysan5/raylib $(RAYLIB_DIR)

run: all
	./$(BUILD)/$(NAME)

clean:
	rm -rf $(BUILD)

distclean: clean
	rm -rf $(RAYLIB_DIR)

.PHONY: all run clean distclean
