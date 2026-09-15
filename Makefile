#	First, WTF Makefile? Second, I ain't remembering those order anyways so...here you go

# ========================
# Configurable variables
# ========================
MAIN      ?= serverThingy.c
SOURCES   ?= src/handlers.c src/utils.c src/router.c src/database.c src/template.c src/request.c src/markdown.c
MD4C_SOURCES = vendor/md4c/md4c.c vendor/md4c/md4c-html.c vendor/md4c/entity.c
ALL_SOURCES = $(SOURCES) $(MD4C_SOURCES)
HEADERS = $(wildcard src/*.h vendor/md4c/*.h)
TARGET    ?= server
CC        ?= gcc
CFLAGS    ?= -Wall -Wextra
LDFLAGS   ?= 
LDLIBS    ?= -lsqlite3


OBJS = $(MAIN:.c=.o) $(ALL_SOURCES:.c=.o)

.PHONY: all clean run rebuild test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "→ Built $(TARGET)"

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "→ Cleaned"

rebuild: clean all

run: $(TARGET)
	./$(TARGET)

