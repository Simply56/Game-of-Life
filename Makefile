CC = clang
CFLAGS = -Ofast -DNDEBUG# faster but might break; DNDEBUG disables assert
# CFLAGS = -O3 # production
# CFLAGS = -Og  -fsanitize=address # debugging
# CFLAGS = -g -O0 -fsanitize=thread # thread debugging

# CFLAGS = -g -O0 # valgrind compatible

LDFLAGS = -lGL -lGLU -lglut
TARGET = game_of_life
SOURCES = custom_render_life.c lifehashmap.c threadpool.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) -Wall -Wextra $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
