#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

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
struct supplemental_page_table {
	/* 
struct supplemental_page_table은 PintOS에서 가상 메모리 관리를 위한 데이터 구조로 사용됩니다. 
이 구조체는 현재 프로세스의 가상 주소 공간을 나타내며, 각 가상 주소에 대한 페이지 정보를 관리하는 데 사용됩니다. 
아래는 struct supplemental_page_table의 멤버 중 일부의 예시입니다:

!struct hash_table *pages:
pages는 해시 테이블로 가상 주소와 해당하는 페이지 정보를 매핑합니다.
각 항목은 해시 함수를 사용하여 빠르게 접근할 수 있습니다.

!struct lock *lock:
lock은 보충 페이지 테이블에 대한 동시 액세스를 관리하는 데 사용됩니다. 다중 스레드 환경에서 안전한 접근을 보장합니다.

struct file *executable_file:
현재 실행 중인 프로세스와 관련된 실행 파일에 대한 포인터입니다. 이를 통해 실행 파일의 내용을 메모리에 로드할 때 사용됩니다.

!bool writable:
현재 프로세스의 가상 주소 공간이 읽기 쓰기 가능한지 여부를 나타내는 플래그입니다.

size_t stack_size:
스택 영역의 크기를 나타내는 변수로, 스택 확장 및 스택 오버플로우 검출에 사용됩니다.

void *stack_limit:
스택 영역의 제한 주소를 나타내는 포인터로, 스택 확장 시에 스택 경계를 검사하는 데 사용됩니다.

size_t code_size:
코드 세그먼트의 크기를 나타내는 변수로, 코드 세그먼트의 로딩 및 언로딩에 사용됩니다.

void *code_limit:
코드 세그먼트의 제한 주소를 나타내는 포인터로, 코드 접근 및 검사에 사용됩니다.

기타 필요한 정보:
프로세스 관련 정보, 실행 파일의 메타데이터, 페이지 교체 정책에 관한 정보 등을 보관하는 데 사용될 수 있는 다양한 멤버가 포함될 수 있습니다.
	 */
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

#endif  /* VM_VM_H */
