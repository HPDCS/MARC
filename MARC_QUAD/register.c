#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>

#define NRRD_MASK 0X00000000ffffffffLL	// mask of the reader counter
#define NRRD_LENG 32
#define EMPTY 0XffffffffffffffffLL

struct wf_register {
	unsigned long long current; 		// composed by [32 bit of index ||32 bit of counter] 
	struct register_slot *rw_space;		// curcular buffer of register slot to use for the current value
	unsigned int size_slot;				// size of the register. Only for fixed size
	unsigned int writers;				// max number of concurrent writers, fixed in the init
	unsigned int readers;				// max number of concurrent readers, fixed in the init
	unsigned int writers_registered;	// number of writers registered
	unsigned int readers_registered;	// number of readers registered
	unsigned long long next_free_slot;	// next slot to use
};

struct register_slot {
	unsigned int r_started; 			// number of read operation started on the slot
	unsigned int r_endend; 				// number of read operation ended on the slot
	unsigned int size;					// size of memory of the data associated to the slot (used for variable size register)
	void *value_loc; 					// pointer to the register value corresponding to this slot
};

struct reader_slot{
	//unsigned int id;					// ID associated to the writer in the init phase
	struct wf_register *parent;				// address of the struct of the referred register
	unsigned int current;				// index of the slot to wich the the calue_loc is referred
	//unsigned int size;					// size of memory of the data associated to the slot (used for variable size register)
	//void *value_loc; 					// pointer to the register value corresponding to this slot
};

struct writer_slot{
	//unsigned int id;					// ID associated to the writer in the init phase
	struct wf_register *parent;			// address of the struct of the referred register
};

void print_snapshot(struct wf_register *reg){
	unsigned int i;
	
	printf("\ncurrent=%llu   size=%u   mask=%llu sugg=%llu\n", reg->current>>32, reg->size_slot, reg->current&NRRD_MASK, reg->next_free_slot);
	for(i=0; i<reg->readers+2; i++){
			printf("rs[%u]=%u re[%u]=%u   size[%u]=%u\n",i, reg->rw_space[i].r_started,i, reg->rw_space[i].r_endend, i,reg->rw_space[i].size);
	}	
}


/**
* Init the register allocating the needed amount of memory and calculate the base for the current value.
*
* @author Mauro Ianni
* @param the number of writers and readers, and the size of the register
* @return a pointer to the instance of the register
* @date 24/02/2016
*/
struct wf_register *_reg_init(unsigned int n_wrts, unsigned int n_rdrs, unsigned int size){	

	unsigned int i;
	struct wf_register *reg;
 	if((reg = malloc(sizeof(struct wf_register)))==NULL){
		printf("malloc failed\n");
        abort();
	}			
	reg->readers = n_rdrs;	
	reg->writers = n_wrts;
	reg->readers_registered = 0;	
	reg->writers_registered = 0;	
	reg->size_slot = size;		
	reg->current = 0;
	reg->next_free_slot = EMPTY;
	
	if((reg->rw_space = malloc((reg->readers+2)*sizeof(struct register_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}	
	/* if the register is static, the memory is preallocated */
	for(i = 0; i < n_rdrs + 2; i++){
		reg->rw_space[i].r_started = 0;
		reg->rw_space[i].r_endend = 0;
		if((reg->rw_space[i].value_loc = malloc(size))==NULL){
			printf("malloc failed\n");
			abort();
		}
		reg->rw_space[i].size = size;
	}	

	/* At the begin the register is setted to zero */
	if(size > 0)
		memset(reg->rw_space[0].value_loc, 0, size);
	reg->current = n_rdrs; //<-Così indico che tutti i lettori partono dal registro 0
	
	return reg;
}	

/**
* Initialize the structure to make a reader working
*
* @author Mauro Ianni
* @param the pointer to the register
* @return A pointer to the structure used by the reader to work with the register
* @date 10/03/2016
*/
struct reader_slot *reader_init(struct wf_register *reg){
	struct reader_slot *rd_slt;
	/*
	unsigned int id = 0;
	
	id = __sync_fetch_and_add(&(reg->readers_registered),1);
	if(id >= reg->readers){
		__sync_fetch_and_add(&(reg->readers_registered),-1);
		return NULL;
	}*/
		
	if((rd_slt = malloc(sizeof(struct reader_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}
	rd_slt->parent = reg;
	rd_slt->current = 0; 
	
	//rd_slt->size = reg->size_slot;
	
		
	return rd_slt;
}

/**
* Initialize the structure to make a writer working
*
* @author Mauro Ianni
* @param the pointer to the register
* @return A pointer to the structure used by the writer to work with the register
* @date 10/03/2016
*/
struct writer_slot *writer_init(struct wf_register *reg){
	struct writer_slot *wr_slt;
	/*unsigned int id;
		
	id = __sync_fetch_and_add(&(reg->writers_registered),1);
	if(id >= reg->writers){
		__sync_fetch_and_add(&(reg->writers_registered),-1);
		return NULL;//return ~0;//id = 0;
	}*/
	
	if((wr_slt = malloc(sizeof(struct writer_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}
	//wr_slt->id = id;
	wr_slt->parent = reg;
	
	return wr_slt;
}

/**
* Write size bytes from the address val in the register
*
* @author Mauro Ianni
* @param the pointer to the writer register slot, the address of the value to write and it's size
* @return void
* @date 10/03/2016
* @note If is used a size!=0 with a fixed size register, the size parameter is ignored
*/
void *_reg_write(struct writer_slot *wr_slt, void *val, unsigned int size){
	//unsigned int size_slot;
	unsigned long long current_tmp, current_index, i,da_cancellare;
	struct wf_register * reg;
	
	reg = wr_slt->parent;
	
	current_index = (reg->current) >> NRRD_LENG;
	//i = (current_index + 1) % (reg->readers + 2); 
	if(reg->next_free_slot != EMPTY){
		i = da_cancellare = reg->next_free_slot;
		reg->next_free_slot = EMPTY;
		
	}
	else{
		i = (current_index + 1) % (reg->readers + 2); 
	}
	/* search a free slot to use starting from the last one*/
	while(1){ 		
		if(  ((reg->rw_space[i].r_started) == (reg->rw_space[i].r_endend)) && (current_index != i)  ){ //<- forse il controllo sul current non serve poichè DEVE trovare un registro libero prima di raggiungerlo
			break;
		}
		i = (i + 1) % (reg->readers + 2);
	}
	/* For variable size register, allocate memory for the new buffer and update the size parameter of the slot(if necessary)*/
	if(reg->size_slot==0 && reg->rw_space[i].size != size){
		reg->rw_space[i].size = size;
		free(reg->rw_space[i].value_loc);
		if((reg->rw_space[i].value_loc = malloc(size))==NULL){
			printf("malloc failed\n");
			abort();
		}
	}
	/* copy the value in the register and reset the control parameters*/	
	memcpy(reg->rw_space[i].value_loc, val, reg->rw_space[i].size);
	reg->rw_space[i].r_started = 0;
	reg->rw_space[i].r_endend = 0;
	
	/* prepare the new current swap it with the old one, making the new value visible*/
	current_tmp = __sync_lock_test_and_set(&reg->current, i << NRRD_LENG);
	
	/* update the r_started field in the old slot or free the memory used */
	reg->rw_space[current_index].r_started = (unsigned int)(current_tmp & NRRD_MASK);
	
	return reg->rw_space[i].value_loc; //forse per correttezza poi sarà meglio toglierlo
}


/**
* Read the value of the register
*
* @author Mauro Ianni
* @param the pointer to the reader register slot, the address of the value to write and it's size
* @return the pointer to a memory location with a copy of the current value
* @date 10/03/2016
*/
void *reg_read(struct reader_slot *rd_slt){
	unsigned long long my_current, tmp_current;
	unsigned int re;
	struct wf_register * reg;
	
	reg = rd_slt->parent;
	
	my_current = (unsigned long long)rd_slt->current; //<-statico
	tmp_current = (reg->current) >> NRRD_LENG;
	
	/* if the value is not changed from the last read, the function return the local copy of the reader */
	if(my_current == tmp_current){
		return reg->rw_space[my_current].value_loc; //<-qui il current potrebbe essere cambiato, ma comunque mi sta bene il vecchio perchè era ancora il current ad inizio lettura
	}
	/* if the current is changed, it is possible to free the old slot */
	re = __sync_add_and_fetch(&(reg->rw_space[my_current].r_endend), 1); //<-se il current è cambiato, libero il vecchio slot
	if(re == reg->rw_space[my_current].r_started){
		reg->next_free_slot = my_current;
	}
	
	/* get the current index and atomically increment the counter associated */
	tmp_current = (unsigned long long) __sync_add_and_fetch(&(reg->current), 1);
	tmp_current = tmp_current >> NRRD_LENG;
	rd_slt->current = (unsigned int) tmp_current;
	//rd_slt->size = reg->rw_space[my_current].size;
	//rd_slt->value_loc = reg->rw_space[my_current].value_loc;
	//Se nel frattempo il current cambia, non mi interessa, perchè tanto questo era concorente
	return reg->rw_space[tmp_current].value_loc;
}

/**
* get the size of the current value read
*
* @author Mauro Ianni
* @param the pointer to the reader register slot
* @return the size of the register as an unsigne int
* @date 10/03/2016
*/
unsigned int get_size(struct reader_slot *rd_slt){
	return rd_slt->parent->rw_space[rd_slt->current].size; //rd_slt->size;
}

/**
* Free the memory used by the register
*
* @author Mauro Ianni
* @param the pointer to the register
* @return void
* @date 24/02/2016
*/
void reg_free(struct wf_register *reg){
	free(reg->rw_space);
	free(reg);
}



