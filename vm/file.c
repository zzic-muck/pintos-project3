/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,	// 페이지 삭제 함수
	.type = VM_FILE,
};

/* The initializer of file vm 
 * 파일 지원 페이지 하위 시스템 초기화.
 * 파일 백업 페이지와 관련된 모든 것을 설정할 수 있다. */
void
vm_file_init (void) {
}

/* Initialize the file backed page
 * 파일 지원 페이지 초기화.
 * 이 함수는 먼저 page->operations에서 파일 지원 페이지에 대한 핸들러를 설정한다.
 * 메모리를 지원하는 파일과 같은 페이지 구조에 대한 일부 정보를 업데이트 */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file.
 * kva 위치에 있는 페이지를 파일에서 읽어와 페이지를 swap in.
 * 파일 시스템과 동기화가 필요하다.
 * 잘 쓰고 잘 지워 */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
	file_read_at(file_page->file, kva, file_page->read_bytes, file_page->offset);
	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);
}

/* Swap out the page by writeback contents to the file.
 * 내용을 다시 파일에 기록하여 swap out한다.
 * 우선 페이지가 dirty인지 확인하는 것이 좋다. dirty=0이면 파일의 내용을 수정할 필요가 없다.
 * 페이지를 교체한 후에는 페이지의 dirty bit를 꺼야 한다. */
static bool
file_backed_swap_out (struct page *page) {
	struct thread *curr = thread_current();
	struct file_page *file_page = &page->file;

	if (pml4_is_dirty(curr->pml4, page->va)) {	
		file_write_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->offset);
		pml4_set_dirty(curr->pml4, page->va, 0);
	}
	pml4_clear_page(curr->pml4, page->va);

	page->frame->page = NULL;
	page->frame = NULL;
}

/* Destory the file backed page. PAGE will be freed by the caller. 
 * 관련 파일을 닫아 파일 지원 페이지를 파괴한다. 
 * 내용이 dirty인 경우 변경 사항을 파일에 다시 기록해야 한다.
 * 이 함수에서 페이지 구조를 free 할 필요 X
 * file_backed_destroy의 호출자가 이를 처리해야 한다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	if (pml4_is_dirty(thread_current()->pml4, page->va) == 1)
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset);
	// 스왑 인 된것만 팔록프리
	// pml4 클리어 페이지
	// pml4_clear_page();
}

static bool lazy_load_file (struct page *page, void *aux_) {
	ASSERT(page->frame->kva != NULL);

 	if (!page || !page->frame)
		return false;

	struct lazy_load_aux_file *aux = aux_;	// void로 들어온 aux_ 형변환

	size_t page_read_bytes = aux->read_bytes;
	size_t page_zero_bytes = aux->zero_bytes;
	
	// frame에 복사
	file_read_at(aux->file, page->frame->kva, page_read_bytes, aux->ofs);

	// swap in/out 과정에서 내용이 변경될 수 있기 때문에 명시적으로 다시 초기화
	memset(page->frame->kva + page_read_bytes, 0, page_zero_bytes);
	
	// munmap할 때 디스크에 내용 반영해주기 위해 파일 페이지에 저장
	page->file.file = aux->file;
	page->file.offset = aux->ofs;
	page->file.read_bytes = page_read_bytes;
	page->file.page_cnt = aux->page_cnt;
	free(aux);
	
	return true;
}

/* Do the mmap
 * 성공적으로 페이지를 생성하면 addr을 반환한다.
 * 파일 타입이 FILE인 UNINIT 페이지를 생성한 후 page-fault가 발생하면
 * FILE 타입의 페이지로 초기화되며 물리프레임과 연결된다.
 * length만큼 할당 */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	if (!addr)
		return false;

	struct file *new_file = file_reopen(file);	// mmap을 하는 동안 외부에서 해당 파일을 close()하는 불상사를 예외처리 하기 위함
	void *origin_addr = addr;		// 초기 주소 저장
	size_t origin_length = length;	// 초기 사이즈 저장

	while (length > 0) {
		struct lazy_load_aux_file *aux = malloc(sizeof(struct lazy_load_aux_file));

		// 메모리 할당 오류
		if (!aux)
			return false;
		
		aux->file = new_file;
		aux->ofs = offset;
		aux->read_bytes = length < PGSIZE ? length : PGSIZE;
		aux->zero_bytes = PGSIZE - aux->read_bytes;
		aux->writable = writable;
		aux->page_cnt = origin_length / PGSIZE;
		
		addr = pg_round_down(addr);
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file, aux)) {
			free(aux);
			return false;
		}
		length -= aux->read_bytes;
		offset += aux->read_bytes;
		addr += PGSIZE;
	}
	return origin_addr;		// 초기 주소 반환
}

/* Do the munmap
 * 연결된 물리프레임과의 연결을 끊어주는 함수
 * FILE 타입의 페이지는 file-backed 페이지이기에 디스크에 존재하는 파일과 연결된 페이지이다.
 * 해당 페이지에 수정사항이 있을 경우 이를 감지하여 변경사항을 디스크의 파일에 써줘야한다. */
void
do_munmap (void *addr) {
	struct thread *curr = thread_current();
	struct page *page = spt_find_page(&curr->spt, addr);
	struct file *file = page->file.file;
	struct file_page *f_page = &page->file;

	int cnt = f_page->page_cnt;

	while (cnt > 0) {
		page = spt_find_page(&curr->spt, addr);
		
		if (!page)
			return false;

		// 페이지 수정됐을 경우 (dirty bit = 1) -> 디스크의 file에 write
		if (pml4_is_dirty(curr->pml4, addr))
			file_write_at(file, addr, f_page->read_bytes, f_page->offset);
		
		pml4_clear_page(curr->pml4, addr);
		if (page->frame) {
			palloc_free_page(page->frame->kva);
		}
		hash_delete(&curr->spt.hash_table, &page->hash_elem);
		free(page);
		
		cnt--;
		addr += PGSIZE;
	}
	file_close(file);
}