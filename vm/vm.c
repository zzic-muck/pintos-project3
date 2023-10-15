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

  return (uint64_t)a->va < (uint64_t)b->va;
}

/*insert and delete*/
bool page_insert (struct hash *h, struct page *p) {
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
		//페이지를 하나 할당하고? 페이지가 사용할 이니셜라이저 넣는다
		struct page *page = (struct page *)malloc(sizeof(struct page));
		upage = pg_round_down(upage);

		switch (VM_TYPE(type)) {

			case VM_ANON:
				uninit_new(page, upage, init, type, aux, anon_initializer);
				break;
			
			case VM_FILE:
				uninit_new(page, upage, init, type, aux, file_backed_initializer);
				break;
			
			default:
				break;
		}


		if(!spt_insert_page(spt, page)) {
			return false;
		} //spt에 page를 넣어준다
		
		return true;
	}
err: 
	return false;
}



/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	//spt에서 va를 찾아서 page를 반환해준다
	//hash_find를 쓰면 될듯

	struct page p;
	va = pg_round_down(va);
	p.va = va;

	struct hash_elem *e = hash_find(&spt->spt_page, &p.hash_elem);

	if(e != NULL){
		page = hash_entry(e, struct page, hash_elem);
	}
	else{
		page = NULL;
	}

	return page;
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
	victim = list_entry(list_pop_front(&frame_table), struct frame, frame_elem);
	
	//클락으로도 할 수 잇슴

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out (victim->page);

	return victim;
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

	frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO); //팔록으로 일단은

	if(frame->kva == NULL || frame == NULL)
	{
		PANIC("palloc_get_page failed");
		//희생자 로직 필요
	}
		
	// list_push_back(&frame_table, &frame->frame_elem); //프레임을 프레임테이블에 넣어준다
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	vm_alloc_page (VM_ANON, addr, true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) 
{
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */ 
	if(addr > USER_STACK && addr < KERN_BASE){
		
		return false;
	}

	if(spt_find_page(spt, pg_round_down(addr)) == NULL){

		if(pg_round_down(addr) < USER_STACK && pg_round_down(addr) > (USER_STACK - (1<<20)) && addr == f->rsp) 
		{
			vm_stack_growth(pg_round_down(addr));
		}
		else
			return false;
	}

    page = spt_find_page(spt, pg_round_down(addr));

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
	// printf("vm_claim_page\n");

	// 혹시 alloc 실패하면 여기서 뭔가 do claim 하기전에 하지 않을까??
	
	vm_alloc_page(VM_ANON, va, true);
	va = pg_round_down(va);
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

	if (!pml4_set_page (t -> pml4, page -> va, frame -> kva, true))
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
	
	struct hash_iterator i;

   	hash_first (&i, &src -> spt_page);
   	while (hash_next (&i)) {
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem); 
   		enum vm_type type = src_page -> operations -> type;
		void *upage = src_page -> va;
		bool writable = src_page -> writable;
		vm_initializer *init = src_page ->uninit.init;
		void *aux = src_page -> uninit.aux;

		// type == uninit 이라면 복사하는 페이지도 uninit
		if (type == VM_UNINIT) {
			// vm_initializer *init = src_page ->uninit.init;
			// void *aux = src_page -> uninit.aux;
			vm_alloc_page_with_initializer (VM_ANON, upage, writable, init, aux);
			// continue;
		}
		else{
		//uninit이 아니라면
			if (!vm_alloc_page(type, upage, writable)) {
				// init이랑 aux는 Lazy Loading에 필요함
				// 지금 만드는 페이지는 기다리지 않고 바로 내용을 넣어줄 것이므로 필요 없음
				return false;
			}
			//vm_claim_page로 요청한 후 매핑 + 페이지 타입에 맞게 초기화
			if (!vm_claim_page(upage)) {
				return false;
			}

			struct page *dst_page = spt_find_page(dst, upage);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
  	}
	return true;
}



void hash_page_destroy (struct hash_elem *e, void *aux) {
	struct page *page = hash_entry(e, struct page, hash_elem);
	destroy(page);
	free(page);
}
/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->spt_page, hash_page_destroy);

}
