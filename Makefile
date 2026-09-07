#	First, WTF Makefile? Second, I ain't remembering those order anyways so...here you go

# ========================
# Configurable variables
# ========================
MAIN      ?= serverThingy.c
SOURCES   ?= src/handlers.c src/utils.c src/router.c
TARGET    ?= server
CC        ?= gcc
CFLAGS    ?= -Wall -Wextra -std=c99
LDFLAGS   ?= 

# ===================
#	THE REST
# =================== 

OBJS = $(MAIN:.c=.o) $(SOURCES:.c=.o)

.PHONY: all clean run rebuild

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "→ Built $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "→ Cleaned"

rebuild: clean all

run: $(TARGET)
	./$(TARGET)