/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "include/lib/kernel/hash.h"
#include <stdio.h>

struct list frame_table; //프레임 테이블 전역변수


/*hash func*/
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/*less func*/
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

/*insert and delete*/
bool page_insert (struct hash *h, struct page *p) {
	printf("page_insert\n");
	return hash_insert (&h, &p->hash_elem);
}

bool page_delete (struct hash *h, struct page *p) {
  return hash_delete (&h, &p->hash_elem);
}

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
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);

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
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		
		//일단 spt에 upage주소가 없을것임
		//upage는 새롭게 만들 페이지의 va
		//process.c 의 load_segment에서 호출함
		
		//페이지를 하나 할당하고? 페이지가 사용할 page_iunitializer를 어캐 만들어주지?
		
		struct page *page = (struct page *)malloc(sizeof(struct page));

		typedef bool (*page_initializer) (struct page *, enum vm_type, void *kva);
		page_initializer new_initializer = NULL;

		uninit_new(page, upage, 1, type, init, aux); //uninit_new를 통해 page를 초기화해준다
		spt_insert_page(spt, page); //spt에 page를 넣어준다

		return true;
	}

err:
	return false;
}



/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	// struct page *page = (struct page *)malloc(sizeof(struct page));
	// struct hash_elem *e;

	//spt에 존재하는 페이지의 va를 찾는다
	//page하나 말록해서 그 page의 va에 인자로 받은 va를 넣고
	//spt_page에 hash_elem이 있는지 찾아본다 라는데 이게맞나

	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;
	
	page->va = pg_round_down(va); //해당 va가 속해있는 페이지 시작 주소를 갖는 page를 만든다.

	/* e와 같은 해시값을 갖는 page를 spt에서 찾은 다음 해당 hash_elem을 리턴 */
	e = hash_find(&spt->spt_page, &page->hash_elem);
	free(page);

	return e != NULL ? hash_entry(e, struct page, hash_elem): NULL;
}




/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	//page를 spt에 넣는다
	//hash_insert 인자보면 hash_elem을 넣어줘야할듯
	if(hash_insert(&spt->spt_page, &page->hash_elem) == NULL){
		succ = true;
	}
	else{
		succ = false;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	
	//palloc_get_page로 프레임 할당 malloc으로 할당??
	//반환받은 주소를 frame 구조체에 넣어줘?
	//if frame 할당 실패하면 일단 예외처리

	frame = palloc_get_page(PAL_USER);
	frame->kva = palloc_get_page(PAL_USER);

	if(frame->kva == NULL)
	{
		PANIC("palloc_get_page failed");
	}
		
	list_push_back(&frame_table, &frame->frame_elem); //프레임을 프레임테이블에 넣어준다
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	// 물리 프레임과 연결할 페이지를 spt를 통해 찾느다?
	// va를 통해 page를 찾아야함
	printf("vm_claim_page\n");
	page = spt_find_page(&thread_current()->spt, va);
	if(page == NULL) 
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame (); //프레임 할당받음
	struct thread *t = thread_current ();
	/* Set links */
	frame->page = page;
	page->frame = frame;
	//frame 과 page를 연결해준다
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	
	// page가 어떤 kva(물리주소) 와 연결되어있는지 확인
	// 없으면 va를 kva와 연결해준다
	// pml4_set_page를 써봄

	if (!pml4_set_page (t -> pml4, page -> va, frame->kva, page -> writable))
		return false;
	return swap_in (page, frame->kva);
}





/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	//해쉬 init
	hash_init(&spt->spt_page, page_hash, page_less, NULL);

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
