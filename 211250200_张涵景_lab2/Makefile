compile:
	nasm -f elf32 -g my_print.asm -o my_print.o  
	g++ -std=c++11 -m32 -c main.cpp -o main.o
	g++ -m32 my_print.o main.o -o main
	rm my_print.o
	rm main.o
	./main
