#	First, WTF Makefile? Second, I ain't remembering those order anyways so...here you go

# ========================
# Configurable variables
# ========================
MAIN      ?= serverThingy.c
SOURCES   ?= src/handlers.c src/utils.c src/router.c src/database.c src/template.c src/request.c
TARGET    ?= server
CC        ?= gcc
CFLAGS    ?= -Wall -Wextra -lsqlite3
LDFLAGS   ?= 

# ===================
#	THE REST
# =================== 

OBJS = $(MAIN:.c=.o) $(SOURCES:.c=.o)

.PHONY: all clean run rebuild test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "→ Built $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) tests/test_request
	@echo "→ Cleaned"

rebuild: clean all

run: $(TARGET)
	./$(TARGET)

test: tests/test_request
	./tests/test_request

tests/test_request: tests/test_request.c $(sort $(SOURCES)) $(wildcard src/*.h)
	$(CC) -Wall -Wextra -Werror -Isrc -o $@ tests/test_request.c $(sort $(SOURCES)) -Wl,--wrap=recv -Wl,--wrap=send -lsqlite3
