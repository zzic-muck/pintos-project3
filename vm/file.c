/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include <stdio.h>
#include <string.h>

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

struct lock file_lock;

/* The initializer of file vm */
void
vm_file_init (void) {
	lock_init(&file_lock);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;

	return true;

}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	int cnt = file_page->tolength / PGSIZE;
	void *addr = page->addr;

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	file_page->file = NULL;

}

bool lazy_do_mmap(struct page *page, void *aux){
	struct lazy_mmap *lazy_mmap = aux;
	struct file *file = lazy_mmap->file;
	off_t offset = lazy_mmap->offset;
	size_t page_read_bytes = lazy_mmap->page_read_bytes;
	
	file_seek(file, offset);
	file_read(file, page->frame->kva, page_read_bytes);
	
	page->file.file = file;
	page->file.page_read_bytes = page_read_bytes;
	page->file.offset = offset;
	page->file.tolength = lazy_mmap->tolegnth;
	page->addr = lazy_mmap->addr;
	free(lazy_mmap);
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	void *old_addr = addr;
	struct file	*m_file = file_reopen(file);
    size_t tolength = length;
	// printf("length : %d\n", length);
 	while(length > 0){
		struct lazy_mmap *lazy_mmap = malloc(sizeof(struct lazy_mmap));
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
        size_t page_zero_bytes = PGSIZE - length;

		lazy_mmap->file = m_file;
		lazy_mmap->offset = offset;
		lazy_mmap->page_read_bytes = page_read_bytes;
		lazy_mmap->tolegnth = tolength;
		lazy_mmap->writable = writable;
		lazy_mmap->addr = addr;

		addr = pg_round_down(addr);
		vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_do_mmap, lazy_mmap);
		
		length -= page_read_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}

	return old_addr;
}

/* Do the munmap */
void 
do_munmap (void *addr) {
	int cnt;

	addr = pg_round_down(addr);
	struct page *page = spt_find_page(&thread_current()->spt, addr);
	cnt = page->file.tolength / PGSIZE;

	while(cnt > 0){
	page = spt_find_page(&thread_current()->spt, addr);

	if(!pml4_is_dirty(thread_current()->pml4, addr)){
		
		pml4_clear_page(thread_current()->pml4, addr);
		palloc_free_page(page->frame->kva);
		hash_delete(&thread_current()->spt, &page->hash_elem);
		free(page);

	}
	else{
		file_seek(page->file.file, page->file.offset);
		file_write(page->file.file ,page->frame->kva, page->file.page_read_bytes);
		pml4_set_dirty(thread_current()->pml4, addr, false);

		pml4_clear_page(thread_current()->pml4, addr);
		palloc_free_page(page->frame->kva);
		hash_delete(&thread_current()->spt, &page->hash_elem);
		free(page);
	}
	cnt --;
	addr += PGSIZE;
}

}
