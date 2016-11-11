#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>

#include "timer.h"
#include "rapl.h"
#include "cpuid.h"
#include "msr.h"
#include "register.h"

#define DIM 1

struct wf_register *reg;
unsigned long long val = 1;
unsigned int busy_write=0, busy_read=0,  end_write=0, end_read=0, size=0, count_write=0, duration=0, rd_id = 0,  load_reader=0, load_writer=0;
unsigned int *count_read;
bool start = false, end = false;

int _memcmp(const void *s1, const void *s2, size_t n) {
    unsigned char u1, u2;

    for ( ; n-- ; s1++, s2++) {
		u1 = * (unsigned char *) s1;
		u2 = * (unsigned char *) s2;
		if ( u1 != u2) {
			return (u1-u2);
		}
    }
    return 0;
}

static char *rand_string(char *str, size_t size) {
    size_t n;
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (size) {
        --size;
        for (n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

void printBits(size_t const size, void const * const ptr) {
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;

    for (i=size-1;i>=0;i--)
    {
        for (j=7;j>=0;j--)
        {
            byte = b[i] & (1<<j);
            byte >>= j;
            printf("%u", byte);
        }
    }
    puts(" ");
}

int busy_loop(unsigned int count){
	unsigned int i, k;
	for(i=0; i<count*1000; i++){
		k = i * i;
	}
	return k;
}

void *run_write(void *args){
	char arr[size];
	struct writer_slot *wr_slt;
	
	wr_slt = writer_init(reg);

	while(!start);
	
	while(!end || (end_write!=0 && count_write >= end_write)){
		//carico
		if(load_writer>0){
			rand_string((char *)arr, size);
		}
		//write		
		reg_write(wr_slt, arr);
		//busy loops
		if(busy_write){
			usleep(busy_write);
		}
		
		count_write++;
	}
	
	pthread_exit(NULL);

}

void *run_read(void *args){
	unsigned int id, *ll;
	struct reader_slot *rd_slt;
	
	rd_slt= reader_init(reg);
	id = __sync_fetch_and_add(&rd_id, 1);

	while(!start);

	while(!end || (end_read!=0 && count_read[id] >= end_read)){
		//read
		ll=reg_read(rd_slt);
		//carico
		if(load_reader>0){
			_memcmp(ll, ll, size);
		}
		//busy loops
		if(busy_read) 
			usleep(busy_read);
		
		count_read[id]++;
	}
	
	pthread_exit(NULL);
}

int main(int argn, char *argv[]) {
	unsigned int i, ret, readers, writers, tot_count_read = 0;
    timer exec_time;

    if(argn < 8) {
        fprintf(stderr, "Usage: %s: n_writer n_reader end_write end_read busy_write busy_read size\n", argv[0]);
        exit(EXIT_FAILURE);
    }
      
    writers = atoi(argv[1]);
    readers = atoi(argv[2]);
    size = atoi(argv[3]);
    
    if(argn == 9){
		busy_write = atoi(argv[4]);
		busy_read = atoi(argv[5]);
		duration = atoi(argv[6]);
		load_writer = atoi(argv[7]);
		load_reader = atoi(argv[8]);
	}
	else if (argn == 10){
		busy_write = atoi(argv[4]);
		busy_read = atoi(argv[5]);		
		end_write = atoi(argv[6]);
		end_read = atoi(argv[7]);
		load_writer = atoi(argv[8]);
		load_reader = atoi(argv[9]);
	}
    
    pthread_t p_tid[writers + readers];    
	reg = reg_init(writers, readers, size);
	//reg = reg_init(writers, readers, DIM*sizeof(unsigned long long));
	
	if((count_read = calloc(readers, sizeof(unsigned int))) == NULL){
		printf("malloc failed\n");
		abort();
	}
    
    for(i = 0; i < writers; i++) {
        if( (ret = pthread_create(&p_tid[i], NULL, run_write, NULL)) != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }
    }
    for(i = 0; i < readers; i++) {
        if( (ret = pthread_create(&p_tid[writers+i], NULL, run_read, NULL)) != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }
    }

	sleep(1);
	startEnergy();
	printf("\n\n+===================================================================================O\n");
	printf("START TEST on REGISTER(%u,%u) of size %u for %u seconds:\n\n", writers, readers, size, duration);
	
	timer_start(exec_time);
	start = true;
	if(duration > 0){
		sleep(duration);
		end = true;
	}
	
	for(i = 0; i < writers + readers; i++){
		pthread_join(p_tid[i], NULL);
	}

	printf("TOTAL WRTE: %u\n", count_write);
	for(i = 0; i < readers; i++)  tot_count_read +=count_read[i];
	printf("TOTAL READ: %u\n", tot_count_read);
	printf("TOTAL OPER: %u\n", tot_count_read+count_write);
	endEnergy();
	printf("+===================================================================================O\n\n\n");
	
	reg_free(reg);
	return 0;
}
