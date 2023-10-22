/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
struct bitmap *swap_bitmap;	// 스왑 테이블 (할당받은 스왑 디스크를 관리하는 테이블)
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;	// 한 페이지가 차지하는 섹터 수 = 8
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);


/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages
 * 1. 스왑 디스크 설정
 * 2. 사용 가능한 영역과 사용된 영역을 관리하기 위한 데이터 struct 설정
 * 3. 스왑 영역 PGSIZE 단위로 관리 */
void
vm_anon_init (void) {
	swap_disk = disk_get(1, 1);		// 스왑 디스크 설정

	// 스왑 디스크에서 사용 가능한 영역과 사용된 영역을 관리하기 위해서는 bitmap 사용
	// 스왑 디스크의 각 섹터(페이지)에 대해 비트맵 상에서 해당 섹터의 사용 여부를 추적할 수 있다.
	// 비트맵은 각 섹터가 사용 중인지 여부를 나타내는 플래그로 구성된다.
	// 스왑 디스크 크기(디스크에 들어갈 수 있는 page 개수)
	size_t swap_size = disk_size(swap_disk);

	// 스왑 디스크 크기에 맞는 비트맵 초기화
	swap_bitmap = bitmap_create(swap_size);
}

/* Initialize the file mapping
 * 파일을 처음으로 가상 페이지에 load 할 때 load_segment->vm_alloc_page_with~~에서 실행됨 */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	return true;
}

/* Swap in the page by read contents from the swap disk.
 * 스왑 디스크 데이터 내용을 읽어서 익명 페이지를 디스크에서 메모리로 swap in */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	disk_sector_t sector = anon_page->start_sector_num;

	for (int i=0; i<SECTORS_PER_PAGE; i++)
		disk_read(swap_disk, sector + i, kva + (i * DISK_SECTOR_SIZE));
	
	bitmap_set_multiple(swap_bitmap, sector, 8, false);
	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);
	anon_page->start_sector_num = NULL;
}

/* Swap out the page by writing contents to the swap disk.
 * 메모리에서 디스크로 내용을 복사하여 익명 페이지를 스왑 디스크로 교체한다.
 * 스왑 테이블을 사용하여 디스크에서 사용 가능한 스왑 슬롯을 찾은 다음 데이터 페이지를 슬롯에 복사한다.
 * 데이터의 위치는 스왑 디스크에 있다.
 * 데이터 위치 -> 페이지 구조체에 저장 -> 스왑 디스크에 저장 = 스왑 테이블 업데이트
 * 데이터의 위치는 페이지 구조체에 저장되어야 한다. 디스크에 사용 가능한 슬롯이 더 이상 없으면 커널 패닉 발생 가능
 * 디스크로 옮겨주는 작업만 해주면 됨 */
static bool   
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	
	// 비트맵을 순회하며 0인 비트를 찾는다. 연속된 8개의 비트 찾아서 1로 변경
	// 찾은 스왑 슬롯의 첫번째 섹터 번호 저장
	disk_sector_t start_sector = bitmap_scan_and_flip(swap_bitmap, 0, 8, false);
	
	if (start_sector == BITMAP_ERROR)
		return false;

	// 페이지의 anon 구조체 내부에 스왑 슬롯의 시작 섹터 번호 저장
	anon_page->start_sector_num = start_sector;

	// for문 돌려서 8섹터 한번에 write 할 수 있게 해야함
	// 페이지가 8개의 디스크 섹터에 걸쳐 저장될 것 이므로 8번 반복 수행
	for (int i=0; i<SECTORS_PER_PAGE; i++)
		disk_write(swap_disk, start_sector + i, page->frame->kva + (i * DISK_SECTOR_SIZE));

	// 해당 페이지 테이블에서 페이와 관련된 pml4 항목 제거 (페이지가 물리 메모리에서 제거됨)
	pml4_clear_page(thread_current()->pml4, page->va);

	// 복사 후 프레임과 페이지 연결 해제 -> 페이지 스왑 완료
	page->frame->page = NULL;
	page->frame = NULL;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	free(page);
}
