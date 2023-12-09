exec = mkv.out
sources = $(wildcard src/*.c)
test_file = ./samples/test.meks
objects = $(sources:.c=.o)
flags = -g 

$(exec): $(objects)
	gcc $(objects) $(flags) -o $(exec)

%.o: %.c %.h
	gcc -c $(flags) $< -o $@

test:
	./$(exec) $(test_file) 

debug:
	gdb $(exec) $(test_file)

install:
	make
	cp ./mkv.out /usr/local/bin/mks

clean:
	-rm *.out
	-rm *.o
	-rm src/*.o
