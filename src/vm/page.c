
#include "vm/page.h"

#include "threads/malloc.h"
#include <stdbool.h>


/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct pte *p = hash_entry (p_, struct pte, h_elem);
  return hash_bytes (&p->vaddr, sizeof p->vaddr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct pte *a = hash_entry (a_, struct pte, h_elem);
  const struct pte *b = hash_entry (b_, struct pte, h_elem);

  return a->vaddr < b->vaddr;
}

void page_init(){
	struct thread *t = thread_current();
	hash_init(&t->page_table, page_hash, page_less, NULL);
}

struct pte * page_create(uint8_t *frame, uint8_t *vaddr, enum page_located located,enum page_type type, struct file *file, off_t ofs, size_t read_bytes){
	struct pte *new_pte = malloc(sizeof(struct pte));
	new_pte->frame=frame;
	new_pte->vaddr=vaddr;

	new_pte->located = located;
	new_pte->type = type;

	new_pte->file=file;
	new_pte->ofs=ofs;
	new_pte->read_bytes = read_bytes;

	return new_pte;
}


void page_insert(struct pte *p_entry){
	hash_insert(&thread_current()->page_table,&p_entry->h_elem);
}
void page_remove(struct pte *p_entry){
	hash_delete(&thread_current()->page_table,&p_entry->h_elem);
}
struct pte * page_find(uint8_t *vaddr){
	struct pte temp;
	void* page_start = pg_round_down((void*)vaddr);
	temp.vaddr = page_start;

	struct hash_elem *e = hash_find(&thread_current()->page_table, &temp.h_elem);

	if (!e){
      return NULL;
    }
	else{
		return hash_entry (e, struct pte, h_elem);
	}
}
