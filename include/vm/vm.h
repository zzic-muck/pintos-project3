#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
//project 3
#include "lib/kernel/hash.h"
//project 3

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;
struct data;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */

	//project 3
	//hash.h파일의 주석에 잠재적으로 hash table의 value가 될 수 있는 page는 모두 hash_elem을 멤버로 가져야 한다는 내용이 있다.
	/* hash table을 위한 hash function이 돌아가기 위한 필수 멤버 */
	struct hash_elem hash_elem;
	bool writable;
	int mapped_page_count;
	//project 3

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
	//project 3
	struct list_elem frame_elem;
	//project 3

};

struct slot
{
	struct page *page;
	uint32_t slot_no;
	struct list_elem swap_elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */

/* struct supplemental_page_table은 PintOS에서 가상 메모리 관리를 위한 데이터 구조로 사용됩니다. 
이 구조체는 현재 프로세스의 가상 주소 공간을 나타내며, 각 가상 주소에 대한 페이지 정보를 관리하는 데 사용됩니다. 
아래는 struct supplemental_page_table의 멤버 중 일부의 예시입니다: */
struct supplemental_page_table {

	/*spt_hash는 해시 테이블로 가상 주소와 해당하는 페이지 정보를 매핑합니다.
	각 항목은 해시 함수를 사용하여 빠르게 접근할 수 있습니다.*/
	struct hash spt_hash;

};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

//project 3
unsigned page_hash_func (const struct hash_elem *p_, void *aux UNUSED);
static unsigned page_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);
//project 3

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

// struct list swap_table;
// struct list frame_table;
// struct lock swap_table_lock;
// struct lock frame_table_lock;

#endif  /* VM_VM_H */
