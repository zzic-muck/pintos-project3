/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/lib/kernel/hash.h"
#include "threads/mmu.h"

/* frame 구조체를 관리하는 하나의 frame_table */
struct list frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	list_init(&frame_table);
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`.
 * 페이지 구조체를 할당하고, 페이지 타입에 맞는 적절한 초기화 함수를 세팅함으로써
 * 새로운 페이지를 초기화한다. 그리고 유저 프로그램으로 제어권을 넘긴다.
 * 인자로 전달한 vm_type에 맞는 적절한 초기화 함수를 가져와야 하고,
 * 이 함수를 인자로 갖는 uninit_new() 함수를 호출해야 한다. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* Create the page, fetch the initialier according to the VM type, */
		struct page *page = (struct page *)malloc(sizeof(struct page));
		upage = pg_round_down(upage);
		
		if (!page)
			return false;

		/* and then create "uninit" page struct by calling uninit_new. */
		/* You should modify the field after calling the uninit_new. */
		if (type == VM_ANON) {
			uninit_new(page, upage, init, type, aux, anon_initializer);
		}
		else if (type == VM_FILE) {
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
		}
		else {
			return false;
		}
		page->writable = writable;

		/* Insert the page into the spt. */
		if (!spt_insert_page(spt, page))
			return false;

		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL.
 * 인자로 받은 spt로부터 va에 해당하는 page 구조체를 찾아서 반환 
 * 실패했을 경우 NULL 반환 */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	// 더미 페이지 생성 및 초기화
	struct page *dumy_page = (struct page*)malloc(sizeof(struct page));
	struct hash_elem *elem;
	
	dumy_page->va = pg_round_down(va); // 주소를 인자로 가장 가까운 페이지 경계까지 내림하는 함수
	elem = hash_find(&spt->hash_table, &dumy_page->hash_elem); // 해시 함수를 사용하여 페이지 검색
	free(dumy_page); // 할당 해제

	// 페이지를 찾았으면 페이지 포인터 반환
	if (elem)
		return hash_entry(elem, struct page, hash_elem);
	else
		return NULL;
}

/* Insert PAGE into spt with validation. 
 * spt에 인자로 들어온 페이지를 삽입. spt에서 va가 존재하지 않는지 검사해야함 
 * 삽입에 성공하면 true, 실패하면 false */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;

	if (!spt || !page)
		return false;

	// 해시 함수를 사용하여 페이지를 테이블에 삽입
	struct hash_elem *elem = hash_insert(&spt->hash_table, &page->hash_elem);

	// elem이 null 아닐 때: 이미 페이지가 테이블에 존재 -> 삽입 실패
	if (!elem)
		succ = true;	// 페이지가 테이블에 존재하지 X -> 삽입 성공 !!!
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* 대체될 구조체 프레임을 가져옵니다. (희생자 찾음) */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = list_entry(list_pop_front(&frame_table), struct frame, frame_elem);
	return victim;
}

/* 한 페이지를 대체하고 해당 프레임을 반환합니다. 
 * 에러 발생 시 NULL을 반환합니다. 
 * 스왑 대상: 프레임이 아닌 프레임과 연결된 페이지!!! */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim (); // 하나의 프레임 받아옴

	if (!victim)
		return NULL;

	swap_out(victim->page);	// 페이지 스왑 아웃
	
	return victim;
}

/* 유저풀에서 palloc_get_page를 호출함으로써 새로운 물리 페이지를 가져온다.
 * palloc() 함수는 페이지 프레임을 할당하고 해당 프레임을 반환합니다.
 * 사용 가능한 페이지가 없는 경우 페이지를 대체하고 해당 페이지를 반환합니다.
 * 이 함수는 항상 유효한 주소를 반환합니다.
 * 다시 말해, 유저풀 메모리가 가득 차 있는 경우 사용 가능한 메모리 공간을 확보하기 위해 페이지를 대체합니다.*/
static struct frame *
vm_get_frame (void) {
	// anonymous case를 위해 일단 PAL_ZERO
	struct frame *new_frame = (struct frame *)malloc(sizeof(struct frame));	// 할당하기 위한 프레임 생성

	// new_frame의 kva에 user pool의 페이지 할당
	new_frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);	// 물리 메모리 할당 후 그 위치의 kva 반환
	
	if  (!new_frame || !new_frame->kva) {
		PANIC("fail~~~");
		return NULL;
	}

	// user pool로부터 물리 메모리 할당 실패 시
	if (!new_frame->kva) {
		// user pool이 다 찼다는 뜻이므로 evicted_frame으로 빈자리 만들어줌
		// 페이지 swap out -> 삭제할 페이지 디스크로 이동
		new_frame = vm_evict_frame();
		new_frame->page = NULL;	// 구조체 멤버 초기화
		return new_frame;
	}

	// 할당 받은 frame을 frame_table에 추가
	list_push_back(&frame_table, &new_frame->frame_elem);
	new_frame->page = NULL;

	ASSERT (new_frame != NULL);
	ASSERT (new_frame->page == NULL);
	
	return new_frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	// 하나 이상의 anon 페이지를 할당하여 스택 크기를 늘림
	// addr은 faulted 주소에서 유효한 주소
	vm_alloc_page(VM_ANON, pg_round_down(addr), 1);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success
 * 유효한 페이지 폴트인지 체크 후 유효하지 않은 페이지에 접근한 폴트라면 찐 페뽈.
 * 그렇지 않고 bogus fault라면 이는 페이지에서 콘텐츠를 로드하고
 * 유저 프로그램에게 제어권을 반환해야 한다.
 * f: 페폴 발생했을 때 그 순간의 레지스터 값들을 담고 있는 구조체
 * addr: 페이지 폴트를 일으킨 가상 주소
 * user: rsp값이 VM이 유저/커널 영역중 어디인지. 해당 값이 true일 경우 유저모드에서 페폴 일으켰다는 뜻 
 * write: true일 경우 해당 페폴이 쓰기 요청이고 그렇지 않을 경우 읽기 요청
 * non-present: 해당 인자가 false인 겨우 read-only 페이지에 write 하려는 상황 */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {

	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	
	// 페이지 폴트가 발생한 가상 주소 및 인자들이 유효한지 체크
	if (!is_user_vaddr(addr))
		return false;
	
	// 스택 증가로 page fault 예외를 처리할 수 있는지 확인 후 vm_stack_growth 호출
	// rsp가 유효하면 스택그로우 호출
	if (addr == f->rsp && USER_STACK > addr && addr > USER_STACK - (1<<20)) {
		vm_stack_growth(addr);
	}

	// read only page에 write 하려는 상황인지 확인
	if (not_present) {		// 접근한 메모리가 물리 페이지와 매핑 되지 않은 경우
		struct page *page = spt_find_page(spt, addr);
		if (!page)
			return false;
		if (write && !page->writable)
			return false;

		return vm_do_claim_page (page);
	}
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	if (page->frame)
		free(page->frame);
	free (page);
}

/* Claim the page that allocate on VA.
 * 인자로 주어진 va에 페이지를 할당
 * 물리 프레임과 연결할 페이지를 spt를 통해서 찾아준 뒤, do_claim() 호출 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(&thread_current()->spt, va);
	
	// 페이지 찾을 수 없는 경우
	if (!page)
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu.
 * mmu setting: 가상 주소와 물리 주소를 매핑한 정보를 페이지 테이블에 추가
 * 인자로 주어진 page에 물리 메모리 프레임을 할당한다. */
static bool
vm_do_claim_page (struct page *page) {
	// 페이지가 유효하지 않거나, 페이지가 이미 차지된 경우
	if (!page || page->frame)
		return false;
	
	struct frame *frame = vm_get_frame ();	// 새 프레임 할당
	struct thread *curr = thread_current();

	/* Set links */
	frame->page = page;
	page->frame = frame;
	/* frame과 page 연결.
	 * 페이지의 va를 프레임의 pa에 매핑하고 페이지 테이블에 추가 */
	if (!pml4_set_page(curr->pml4, page->va, frame->kva, page->writable)) {
		return false;
	}
	return swap_in (page, frame->kva);
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init (&spt->hash_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst
 * src의 spt를 dst의 spt로 복사 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
	struct supplemental_page_table *src UNUSED) {

	struct hash_iterator src_iter;

	// hash_apply()
	hash_first(&src_iter, &src->hash_table);

	while (hash_next(&src_iter)) {
		struct page *src_page = hash_entry(hash_cur(&src_iter), struct page, hash_elem);
		
		enum vm_type type= src_page->operations->type;
		void *upage = src_page->va;
		bool writable = src_page->writable;
		
		if (type == VM_UNINIT) {
			vm_alloc_page_with_initializer(VM_ANON, upage, writable, src_page->uninit.init, src_page->uninit.aux);
			continue;	
		}
		
		// 자식에게 새 페이지를 할당
		// 어차피 ANON으로 만들어주니까 init, aux NULL, NULL
		if (!vm_alloc_page(VM_ANON, upage, writable))
			return false;
		
		if (!vm_claim_page(upage))
			return false;

		struct page *dst_page = spt_find_page(dst, upage);

		// 페이지 정보 복사
		if(src_page->frame)
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
	}
	return true;
}

void
hash_bye (struct hash_elem *e, void *aux) {
	struct page *page = hash_entry(e, struct page, hash_elem);
	destroy(page);
	// free(page);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash_table, hash_bye);
}
