#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "filesys/off_t.h"
#include <stdint.h>
#include <hash.h>
#include <stdio.h>
#include "threads/thread.h"
#include "threads/vaddr.h"

enum page_located{
	DISK, SWAP, MEMORY
};

enum page_type{
	FILE, STACK
};
struct pte{
	uint8_t *frame;
	uint8_t *vaddr;

	enum page_located located;
	enum page_type type;

	struct file *file;
	off_t ofs;
	size_t read_bytes;

	struct hash_elem h_elem;
};

unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,void *aux UNUSED);

void page_init(void);
struct pte * page_create(uint8_t *frame, uint8_t *vaddr, enum page_located located,enum page_type type, struct file *file, off_t ofs, size_t read_bytes);
void page_insert(struct pte *p_entry);
void page_remove(struct pte *p_entry);
struct pte * page_find(uint8_t *vaddr);

#endif