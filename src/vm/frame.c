#include "threads/thread.h"
#include "vm/frame.h"
#include <list.h>
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
void
frame_init(){
	list_init(&frame_table);
	lock_init(&frame_lock);
}

void
frame_insert(struct fte *f_entry){
	list_push_back(&frame_table,&f_entry->elem);
}

struct fte *
frame_create(uint8_t *frame,struct pte *pte){
	struct fte *new_fte=malloc(sizeof(struct fte));
	new_fte->frame=frame;
	new_fte->pte=pte;
	return new_fte;
}

void
frame_remove(struct fte *f_entry){
	palloc_free_page(f_entry->frame);
	list_remove(&f_entry->elem);
	free(f_entry);
}
