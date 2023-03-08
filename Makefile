CC = gcc
CFLAGS = -I./  `pkg-config --cflags gtk+-3.0`
LIBS = -lSDL2 -lSDL2_net -lsodium `pkg-config --libs gtk+-3.0`

src_files = \
	src/err.c \
	src/init.c \
	src/net.c 
	
build_type ?= client
	
ifeq ($(build_type), client)
	src_files += src/susurrc.c
	target = susurrc
else ifeq ($(build_type), server)
	src_files += src/susurrc-server.c
	target = susurrc-server
endif

target_dir = susurrc
obj_files = $(src_files:.c=.o)

all: build run

build: $(obj_files)
	mkdir -p build/$(target_dir)
	$(CC) $(obj_files) -o build/$(target_dir)/$(target) $(CFLAGS) $(LIBS)

run: build
	cd build/$(target_dir); ./$(target); cd ../../

clean:
	rm $(obj_files)
	
