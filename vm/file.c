/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
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

	struct lazy_load_aux *lazy_load_aux = (struct lazy_load_aux *)page -> uninit.aux;
	file_page -> file = lazy_load_aux -> file;
	file_page -> ofs = lazy_load_aux -> ofs;
	file_page -> read_bytes = lazy_load_aux -> read_bytes;
	file_page -> zero_bytes = lazy_load_aux -> zero_bytes;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
	
	ASSERT (page -> frame -> kva == kva);

	void *addr = page -> va;
	struct file *file = file_page -> file;
	size_t length = file_page -> read_bytes;
	off_t offset = file_page -> ofs;

	file_read_at (file, kva, length, offset);

	pml4_set_page(thread_current() -> pml4, addr, kva, page -> writable);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;

	if (pml4_is_dirty(thread_current() -> pml4, page -> frame -> kva)) {
		file_write_at (file_page -> file, page -> frame -> kva, file_page -> read_bytes, file_page -> ofs);
		pml4_set_dirty (thread_current() -> pml4, page -> frame -> kva, 0);
	}

	pml4_clear_page (thread_current () -> pml4, page -> va);
	page -> frame -> page = NULL;
	page -> frame = NULL;
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	//file_backed_destroy 전에 뭔가 기록했는지 확인하고
	if (pml4_is_dirty(thread_current() -> pml4, page -> va)) {
		//뭔가 기록되었다면 disk에 기록한 후 dirty bit을 0으로 되돌림
		file_write_at(file_page -> file, page -> va, file_page -> read_bytes, file_page -> ofs);
		// pml4_set_dirty(thread_current() -> pml4, page -> va, 0);
	}
	pml4_clear_page(thread_current() -> pml4, page -> va);
}

// bool lazy_load_segment(struct page *page, void *aux) {
    
//     ASSERT(page -> frame -> kva != NULL);

//     struct lazy_load_aux *aux_ = (struct lazy_load_aux *)aux;

//     file_seek(aux_ -> file, aux_ -> ofs);

// 	size_t page_read_bytes = aux_->read_bytes;
// 	size_t page_zero_bytes = aux_->zero_bytes;

// 	/* Get a page of memory. */
// 	if (page == NULL) {
// 		return false;
// 	}

// 	/* Load this page. */
// 	if (file_read(aux_-> file, page -> frame -> kva, page_read_bytes) != (int)page_read_bytes) {
// 		palloc_free_page(page->frame->kva);
// 		return false;
// 	}

// 	memset (page -> frame -> kva + page_read_bytes, 0, page_zero_bytes);

//     return true;
// }

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	// printf("mmap_start\n");
	// printf("thread status : %d\n", thread_current() -> status);
	struct file *dup_file = file_reopen(file);

	//return 할 시작 주소
	void *start_addr = addr;
	
	//매핑을 위해서 사용하는 총 페이지 수
	//length가 PGSIZE 이하라면 1 페이지
	//length % PGSIZE 가 0이 아니라면 (length / PGSIZE) + 1
	//length % PGSIZE 가 0이면 (length / PGSIZE)

	int total_page_count = NULL;
	if (length <= PGSIZE) {
		total_page_count = 1;
	} else if (length % PGSIZE == 0) {
		total_page_count = (length / PGSIZE);
	} else {
		total_page_count = (length / PGSIZE) + 1;
	}
	// printf("total_page_count : %d\n", total_page_count);
	size_t read_bytes = length < file_length(dup_file) ? length : file_length(dup_file);
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	//내용이 PGSIZE에 align 되어있는지 확인
	if ((read_bytes + zero_bytes) % PGSIZE != 0) {
		return NULL;
	}
	//upage가 PGSIZE align되어있는지 확인
	if (pg_ofs(addr) != 0) {
		return NULL;
	}
	//ofs가 PGSIZE align 되어있는지 확인
	if (offset % PGSIZE != 0) {
		return NULL;
	}

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
			return NULL;

		struct page *p = spt_find_page (&thread_current() -> spt, start_addr);
		// ASSERT (total_page_count != NULL);
		p -> mapped_page_count = total_page_count;

		offset += page_read_bytes;
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        addr += PGSIZE;
	}
	// printf("mmap end\n");
	// printf("thread status : %d\n", thread_current() -> status);
	return start_addr;

}

/* Do the munmap */
void
do_munmap (void *addr) {
	// 연결된 물리 프레임과의 연결을 끊어야 한다.
	// 주어진 addr을 통해서 spt로부터 page를 하나 찾는다.
	struct supplemental_page_table *spt = &thread_current() -> spt;
	struct page *target_page = spt_find_page(spt, addr);
	struct file_page *file_page = &target_page->file;
	//mapped_page_count == 반복문을 순회할 횟수
	int count = target_page -> mapped_page_count;
	// printf("count = %d", count);
	struct lazy_load_aux *aux = (struct lazy_load_aux *) target_page -> uninit.aux;
	if (target_page == NULL) {
		return NULL;
	}

	for (int i = 0; i < count; i++) {
		
		// if (target_page) {

		// 	if (pml4_is_dirty(thread_current() -> pml4, target_page -> va)) {
		// 	//뭔가 기록되었다면 disk에 기록한 후 dirty bit을 0으로 되돌림 
		// 		file_write_at(file_page -> file, target_page -> va, file_page -> read_bytes, file_page -> ofs);
		// 		pml4_set_dirty(thread_current() -> pml4, target_page -> va, 0);
		// 	}
			
			destroy (target_page);
		// }
		pml4_clear_page(thread_current() -> pml4, target_page -> va);
		if (target_page->frame)
			palloc_free_page(target_page->frame->kva);
		hash_delete(spt, &target_page ->hash_elem);
		addr += PGSIZE;
		target_page = spt_find_page(spt,addr);
	}
}
