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

/* Destory the file backed page. PAGE will be freed by the caller. 
 * 관련 파일을 닫아 파일 지원 페이지를 파괴한다. 
 * 내용이 dirty인 경우 변경 사항을 파일에 다시 기록해야 한다.
 * 이 함수에서 페이지 구조를 free 할 필요 X
 * file_backed_destroy의 호출자가 이를 처리해야 한다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

static bool lazy_load_file(struct page *page, void *aux_) {
	ASSERT(page->frame->kva != NULL);
	// 파일 리드만 할 수 잇게 만드러
	struct lazy_load_aux *aux = (struct lazy_load_aux *) aux_;

	size_t page_read_bytes = aux->read_bytes;
	size_t page_zero_bytes = aux->zero_bytes;

 	if (!page || !page->frame)
		return false;
	
	// 프레임에 복사
	file_read_at(aux->file, page->frame->kva, page_read_bytes, aux->ofs);

	memset(page->frame->kva + page_read_bytes, 0, page_zero_bytes);
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
	
	// 주소 범위에 파일을 매핑
	void *origin_addr = addr;	// 초기 주소 저장
	
	while (length > 0) {
		struct lazy_load_aux *aux = (struct lazy_load_aux *) malloc(sizeof(struct lazy_load_aux));

		// 메모리 할당 오류
		if (!aux)
			return false;
		
		aux->file = new_file;
		aux->ofs = offset;
		aux->read_bytes = length < PGSIZE ? length : PGSIZE;
		aux->zero_bytes = PGSIZE - aux->read_bytes;
		aux->writable = writable;
		
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file, aux)) {
			free(aux);
			return false;
		}
		length -= aux->read_bytes;
		addr += PGSIZE;
		offset += PGSIZE;
	}
	return origin_addr;		// 초기 주소 반환
}

/* Do the munmap 
*/
void
do_munmap (void *addr) {
	
}
