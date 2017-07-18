all: main.o midiutil.o midifile.o
	c++ main.o midiutil.o midifile.o -pthread -lIrrlicht -lIrrKlang -o drummykit; rm *.o

main.o: src/main.cpp
	c++ src/main.cpp -Wall -c

midiutil.o: src/midiutil.h src/midiutil.c
	gcc src/midiutil.c -c

midifile.o: src/midifile.h src/midifile.c
	gcc src/midifile.c -c
