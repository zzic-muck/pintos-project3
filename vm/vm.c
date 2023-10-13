/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "include/lib/kernel/hash.h"
#include <stdio.h>
//project 3
struct list frame_table; //프레임 테이블 전역변수
//project 3

// /*hash func*/
// unsigned
// page_hash (const struct hash_elem *p_, void *aux UNUSED) {
//   const struct page *p = hash_entry (p_, struct page, hash_elem);
//   return hash_bytes (&p->va, sizeof p->va);
// }

// /*less func*/
// bool
// page_less (const struct hash_elem *a_,
//            const struct hash_elem *b_, void *aux UNUSED) {
//   const struct page *a = hash_entry (a_, struct page, hash_elem);
//   const struct page *b = hash_entry (b_, struct page, hash_elem);

//   return a->va < b->va;
// }

// /*insert and delete*/
// bool page_insert (struct hash *h, struct page *p) {
// 	printf("page_insert\n");
// 	return hash_insert (&h, &p->hash_elem);
// // }




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

//project 3
//page의 멤버인 hash_elem을 통해 page를 찾고, 그 page의 virtual address에 hash값을 넣는 함수(hash_hash_func 역할)
unsigned page_hash_func (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

//해시 태이블 내 두 페이지 요소에 대해 페이지 주소 값을 비교하는 함수(hash_less_func 역할)
static unsigned page_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	const struct page *p_a = hash_entry(a, struct page, hash_elem);
	const struct page *p_b = hash_entry(b, struct page, hash_elem);
	return p_a -> va < p_b -> va;
}

//해당 페이지에 들어있는 hash_elem 구조체를 인자로 받은 해시 테이블에 삽입하는 함수
bool page_insert(struct hash *h, struct page *p) {
	if (!hash_insert (h, &p -> hash_elem)) {
		return true;
	} else {
		return false;
	}
}

bool page_delete (struct hash *h, struct page *p) {
  return hash_delete (&h, &p->hash_elem);
}
//project 3

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

	//project 3
	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;
	// /include/threads/vaddr.h에 있는 '주소를 인자로 가장 가까운 페이지 경계까지 내림하는 함수'
	page->va = pg_round_down(va);
	e = hash_find(&spt -> spt_hash, &page -> hash_elem);
	free(page);

	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
	//project 3

}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {

	//project 3
	return page_insert(&spt -> spt_hash, page);
	//project 3

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
	//project 3
	struct thread *t = thread_current();
	struct list_elem *e, *start;
	//CLOCK policy
	//pseudo code generated by chat GPT
	// 1. Check if the frame at the current position has been accessed.
	// 2. If accessed, clear the accessed bit and move to the next frame.
	// 3. If not accessed, mark this frame as the victim and break out of the loop.

	// Example pseudocode:
	// 1. Get the accessed bit of the current frame.
	// 2. If accessed, clear the accessed bit.
	// 3. If not accessed, set this frame as the victim.

	//clock policy
	//list 맨 뒤에서부터 하나씩 accessed bit 를 확인하면서 할당되었다면(1이라면) 이를 0으로 변경
	//할당되지 않았다면 (이미 0이라면) 이를 반환
	for (start = list_end(&frame_table); start != list_begin(&frame_table); start = list_prev(start)) {
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(t -> pml4, victim -> page -> va)) {
			pml4_set_accessed(t -> pml4, victim -> page -> va, 0);
		} else {
			return victim;
		}
	}

	// for (start = e; start != list_end(&frame_table); start = list_next(start)) {
	// 	victim = list_entry(start, struct frame, frame_elem);
	// 	if (pml4_is_accessed(t -> pml4, victim -> page -> va)) {
	// 		pml4_set_accessed(t -> pml4, victim -> page -> va, 0);
	// 	} else {
	// 		return victim;
	// 	}
	// }

	// for (start = list_begin(&frame_table); start != e; start = list_next(start)) {
	// 	victim = list_entry(start, struct frame, frame_elem);
	// 	if (pml4_is_accessed(t -> pml4, victim -> page -> va)) {
	// 		pml4_set_accessed(t -> pml4, victim -> page -> va, 0);
	// 	} else {
	// 		return victim;
	// 	}
	// }
	
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	//project 3.
	swap_out (victim -> page);
	//project 3.
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {

	//project 3
	//malloc으로 빈 frame 할당
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	//frame의 kva에 user pool의 페이지 할당
	//anonymous case를 위해 일단 PAL_ZERO
	frame -> kva = palloc_get_page(PAL_USER | PAL_ZERO);
	//할당이 안 됐을 때 예외처리
	if (frame -> kva == NULL) {
		//user pool 이 다 찼다는 뜻이므로 evict_frame 으로 빈자리를 만든다.
		frame = vm_evict_frame();
		frame ->page = NULL;
		return frame;
	}
	//frame_table linked list에 frame_elem 추가
	list_push_back(&frame_table, &frame->frame_elem);
	frame->page = NULL;
	//project 3
	
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
	//project 3
	struct page *page = NULL;
	//spt_find_page 함수 내에서 malloc으로 페이지를 할당해준다.
	//이후 va를 가지고 page를 찾아낸다.
	struct supplemental_page_table *spt = &thread_current ()->spt;
	page = spt_find_page (spt, va);
	if (page == NULL) {
		return false;
	}
	
	return vm_do_claim_page (page);
	//project 3
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	//project 3
	struct frame *frame = vm_get_frame (); //프레임 할당받음
	struct thread *t = thread_current ();
	/* Set links */
	frame->page = page;
	page->frame = frame;
	//frame 과 page를 연결해준다
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!pml4_set_page (t -> pml4, page -> va, frame->kva, page -> writable))
		return false;
	return swap_in (page, frame->kva);
	//project 3
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {

	//project 3
	hash_init(&spt -> spt_hash, page_hash_func, page_less_func, NULL );
	//project 3

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
