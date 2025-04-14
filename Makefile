CC = gcc
CFLAGS = -Wall -O2 -fPIC -Iinclude -pthread
LDFLAGS = -pthread
AR = ar
ARFLAGS = rcs

LIB_NAME = async
SRC_DIR = src
OBJ_DIR = build
INCLUDE_DIR = include
EXAMPLE_DIR = example

SRCS = $(SRC_DIR)/async.c
OBJS = $(OBJ_DIR)/async.o

STATIC_LIB = lib$(LIB_NAME).a
SHARED_LIB = lib$(LIB_NAME).so

EXAMPLE_BIN = $(EXAMPLE_DIR)/example

.PHONY: all clean

all: $(STATIC_LIB) $(SHARED_LIB) $(EXAMPLE_BIN)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(SHARED_LIB): $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

$(EXAMPLE_BIN): $(EXAMPLE_DIR)/example.c $(SHARED_LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -l$(LIB_NAME) $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) *.a *.so $(EXAMPLE_BIN)
