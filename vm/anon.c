/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include "bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct bitmap *swap_bitmap;
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


/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1);
	swap_bitmap = bitmap_create(disk_size(swap_disk));

}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct thread *t = thread_current();
	struct anon_page *anon_page = &page->anon;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	disk_sector_t sector = page->anon.sector;

	for(int i = 0; i < 8; i++){
		disk_read(swap_disk, sector + i, kva + i * 512);
	}

	bitmap_set_multiple(swap_bitmap, sector, 8, false);
	page->anon.sector = 0;
	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);

	return true;

}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	disk_sector_t sector;

	sector = bitmap_scan_and_flip(swap_bitmap, 0, 8, false);
	page->anon.sector = sector;
	for(int i = 0; i < 8; i++){
		disk_write(swap_disk, sector + i, page->frame->kva + i * 512);	
	}

	pml4_clear_page(thread_current()->pml4, page->va);

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;


}
