CC=gcc
FLAGS=-g -Wall -pthread -lm
#INCLUDE= -I include/ -I mm/ -I core/ -Istatistics/
EXECUTABLE=test

all: test

test: main.o register.o rapl.o cpuid.o msr.o
	$(CC) $(FLAGS) register.o main.o rapl.o cpuid.o msr.o -o $(EXECUTABLE)
	
main.o : main.c
	$(CC) $(FLAGS) main.c -o main.o -c

register.o : register.c register.h timer.h	
	$(CC) $(FLAGS) register.c -o register.o -c
	
	
rapl.o: rapl.c rapl.h
	$(CC) $(FLAGS) rapl.c -o rapl.o -c
cpuid.o: cpuid.c cpuid.h 
	$(CC) $(FLAGS) cpuid.c -o cpuid.o -c
msr.o: msr.c msr.h
	$(CC) $(FLAGS) msr.c -o msr.o -c
clean:
	rm *.o test
