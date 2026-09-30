CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -Iinclude -D_GNU_SOURCE
LDFLAGS  = -lwebsockets -lcjson -lpthread -lrt -lm

SRC_DIR  = src
OBJ_DIR  = build
TARGET   = firehose_rt

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
