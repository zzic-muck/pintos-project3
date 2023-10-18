/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

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

/* The initializer of file vm */
void
vm_file_init (void) {
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
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

bool lazy_load_segment(struct page *page, void *aux) {
    
    ASSERT(page -> frame -> kva != NULL);

    struct lazy_load_aux *aux_ = (struct lazy_load_aux *)aux;

    file_seek(aux_ -> file, aux_ -> ofs);

	size_t page_read_bytes = aux_->read_bytes;
	size_t page_zero_bytes = aux_->zero_bytes;

	/* Get a page of memory. */
	if (page == NULL) {
		return false;
	}

	/* Load this page. */
	if (file_read(aux_-> file, page -> frame -> kva, page_read_bytes) != (int)page_read_bytes) {
		palloc_free_page(page->frame->kva);
		return false;
	}

	memset (page -> frame -> kva + page_read_bytes, 0, page_zero_bytes);

    return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	struct file *dup_file = file_reopen(file);
	//return 할 시작 주소
	void *start_addr = addr;
	
		size_t read_bytes = length < file_length(dup_file) ? length : file_length(dup_file);
		size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	while (read_bytes > 0 || zero_bytes > 0) {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct lazy_load_aux *aux = (struct lazy_load_aux *)malloc(sizeof(struct lazy_load_aux));

		aux -> file = dup_file;
		aux -> ofs = offset;
		aux -> read_bytes = read_bytes;
		aux -> zero_bytes = zero_bytes;
		aux -> writable = writable;

		//printf("load segment; file: %p, ofs: %d, read_bytes: %d\n", file, ofs, page_read_bytes);
		//vm_alloc_page_with_initializer의 4번째 인자가 load할 때 이용할 함수, 5번쨰 인자가 그 때 필요한 인자이다.
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux))
			return false;

		offset += page_read_bytes;
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        addr += PGSIZE;
	}
	return start_addr;

}

/* Do the munmap */
void
do_munmap (void *addr) {
}
