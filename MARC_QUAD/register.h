#pragma once

/* count the number of arguments */
#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, 5,4,3,2,1)
#define VA_NUM_ARGS_IMPL(_1,_2,_3,_4,_5,N,...) N

#define macro_dispatcher(func, ...) macro_dispatcher_(func, VA_NUM_ARGS(__VA_ARGS__))
#define macro_dispatcher_(func, nargs) macro_dispatcher__(func, nargs)
#define macro_dispatcher__(func, nargs) func ## nargs

#define reg_init(...) macro_dispatcher(reg_init, __VA_ARGS__)(__VA_ARGS__)
#define reg_init3(w,r,s) _reg_init(w,r,s)
#define reg_init2(w,r) _reg_init(w,r,0)

#define reg_write(...) macro_dispatcher(reg_write, __VA_ARGS__)(__VA_ARGS__)
#define reg_write3(r,v,s) _reg_write(r,v,s)
#define reg_write2(r,v) _reg_write(r,v,0)

struct wf_register;
struct reader_slot;
struct writer_slot;

extern void print_snapshot(struct wf_register *reg);

extern struct wf_register *_reg_init(unsigned int n_wrts, unsigned int reader, unsigned int size);

extern struct reader_slot *reader_init(struct wf_register *reg);
extern struct writer_slot *writer_init(struct wf_register *reg);

extern void *_reg_write(struct writer_slot *wr_slt, void *value, unsigned int size);
extern void *reg_read(struct reader_slot *rd_slt);

extern unsigned int get_size(struct reader_slot *rd_slt);

extern void reg_free(struct wf_register *reg);

