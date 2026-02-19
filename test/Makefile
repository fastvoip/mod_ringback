# mod_ringback 单元测试
CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lm

TEST_SRC = tone_detect_test.c
TEST_BIN = tone_detect_test

.PHONY: test clean

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TEST_BIN)
