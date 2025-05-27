CC = clang
CFLAGS = -Ofast -DNDEBUG -march=native -flto
LDFLAGS = -lGL -lGLU -lglut

# CFLAGS = -g  -fsanitize=address 
# CFLAGS = -g -O0 -fsanitize=thread # thread debugging
# CFLAGS = -g -O0  # valgrind compatible

TARGET = game_of_life
SOURCES = src/game_of_life.c src/lifehashmap.c src/threadpool.c src/utils.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) -Wall -Wextra $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
