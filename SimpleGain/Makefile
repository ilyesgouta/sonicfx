
CC = g++
LD = g++

WINE_INCLUDES = /usr/include/wine/windows
OUTPUT = libsimplegain.so

OBJ = SimpleGain.o

.cpp.o:
	$(CC) -c -fPIC -I. -I../Nitrane -I$(WINE_INCLUDES) -DLINUX -DDECLSPEC= $< -o $@

all: $(OBJ)
	$(LD) -shared $(OBJ) -o $(OUTPUT)
clean:
	@rm -f *.o *.so
