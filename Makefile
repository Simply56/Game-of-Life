CC = gcc
CFLAGS = -Ofast # faster but might break
# CFLAGS = -O3 # production
# CFLAGS = -Og  -fsanitize=address # degubbing
# CFLAGS = -g -O0 -fsanitize=thread # thread degubbing


# CFLAGS = -g -O0 # valgrind compatible


LDFLAGS = -lGL -lGLU -lglut
TARGET = game_of_life
SOURCES = custom_render_life.c lifehashmap.c threadpool.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
