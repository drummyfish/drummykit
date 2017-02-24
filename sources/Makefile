all: main.o midiutil.o midifile.o
	c++ main.o midiutil.o midifile.o -lirrlicht -lirrklang -o drummykit

main.o: main.cpp
	c++ main.cpp -Wall -c

midiutil.o: midiutil.h midiutil.c
	gcc midiutil.c -c

midifile.o: midifile.h midifile.c
	gcc midifile.c -c