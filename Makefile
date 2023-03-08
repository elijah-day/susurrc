CC = gcc
CFLAGS = -I./  `pkg-config --cflags gtk+-3.0`
LIBS = -lSDL2 -lSDL2_net -lsodium `pkg-config --libs gtk+-3.0`

src_files = \
	src/err.c \
	src/init.c \
	src/net.c \
	src/main.c
	
obj_files = $(src_files:.c=.o)
target = susurrc
target_dir = susurrc

all: build run

build: $(obj_files)
	mkdir -p build/$(target_dir)
	$(CC) $(obj_files) -o build/$(target_dir)/$(target) $(CFLAGS) $(LIBS)

run: build
	cd build/$(target_dir); ./$(target); cd ../../

clean:
	rm $(obj_files)
	
