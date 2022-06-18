/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "include/userprog/process.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
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

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) 
{
	struct file* reopen_file = file_reopen(file); // 파일 객체를 복제하여 복제된 객체의 주소 리턴

	void *origin_addr = addr;
	size_t read_bytes = length < PGSIZE ? PGSIZE : file_length(file);
	size_t zero_bytes = PGSIZE - read_bytes%PGSIZE;

	while (read_bytes > 0 || zero_bytes > 0){
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct segment *seg = calloc(1, sizeof(struct segment));
		seg->page_read_bytes = page_read_bytes;
		seg->page_zero_bytes = page_zero_bytes;
		seg->offset = offset;
		seg->file = reopen_file;

		void *aux = seg;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable,lazy_load_segment,aux)){
			free(seg);
			return false;
		}
		
		read_bytes -= page_read_bytes;
		zero_bytes =- page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}

	return origin_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	
}


