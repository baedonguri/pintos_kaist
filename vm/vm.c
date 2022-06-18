/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"
#include "userprog/process.h"
#include <string.h>

// struct list frame_table;

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
	// list_init(&frame_table);
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
 * page, do not create it directly and make it through this function or `vm_alloc_page`. */
/* 이니셜라이저를 사용하여 보류 중인 페이지 개체를 만듭니다. 
 * 페이지를 만들려면 직접 만들지 말고 이 함수 또는 'vm_alloc_page'를 통해 페이지를 만드십시오.*/
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;
	upage = pg_round_down(upage);
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* 페이지를 만들고 VM 유형에 따라 initialier를 가져온 다음
		 * uninit_new를 호출하여 "uninit" 페이지 구조를 만듭니다.
		 * uninit_new를 호출한 후 필드를 수정해야 합니다. */
		struct page* new_page = (struct page*)calloc(1,sizeof(struct page));
		bool (*initializer)(struct page *, enum vm_type, void *);
		switch (VM_TYPE(type)){
			case VM_ANON :
				initializer = anon_initializer;
				break;
			case VM_FILE :
				initializer = file_backed_initializer;
				break;
			default :
				goto err;
		}
		uninit_new(new_page, upage, init, type, aux, initializer);
		/* TODO: Insert the page into the spt. */
		/* 페이지를 spt에 삽입합니다. */
		new_page->writable = writable;
		
		return spt_insert_page(spt, new_page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* va(가상 주소)에 해당하는 페이지 번호를 spt에서 검색하여 페이지 번호를 추출하는 함수 */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page page;
	struct hash_elem *e;
	page.va = pg_round_down(va);
	e = hash_find(spt->pages, &page.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}
// struct page *
// spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
// 	struct page *page = calloc(1, sizeof(struct page));
// 	page->va = pg_round_down(va);
// 	struct hash_elem *e = hash_find(spt->pages, &page->hash_elem);
// 	if (e == NULL){
// 		return NULL;
// 	}
// 	struct page *tmp = hash_entry(e, struct page, hash_elem);
// 	return tmp;
// }

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if (!hash_insert(spt->pages, &page->hash_elem)) // NULL이면 true
		succ = true;
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
/* palloc을 하고 frame을 얻는다. 
 * 사용 가능한 페이지가 없으면 페이지를 제거하고 반환합니다.
 * 이것은 항상 유효한 주소를 반환합니다.
 * 즉, 사용자 풀 메모리가 가득 찬 경우 이 함수는 프레임을 제거하여 사용 가능한 메모리 공간을 확보합니다. */
static struct frame *vm_get_frame (void) {
	struct frame *frame = (struct frame*)calloc(1,sizeof(struct frame));
	/* TODO: Fill this function. */
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	frame->kva = palloc_get_page(PAL_USER); // 물리메모리의 USER_POOL 내의 프레임을 프로세스의 커널 가상 메모리로 할당 및 매핑
	if (frame->kva == NULL){
		PANIC("to do");
	}
	// list_push_back(&frame_table, &frame->frame_elem);

	// frame->page = NULL;

	return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr UNUSED)
{
	addr = pg_round_down(addr);
	if (addr > (void *) USER_STACK-0x100000) {
		if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true)){
			vm_claim_page(addr);
		}
	}
	
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
	struct thread *t = thread_current();
	if (is_kernel_vaddr(addr))
		return false;

	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// 	rsp - 8 <= addr < USER_STACK
	
	// void *rsp_ = user ? f->rsp : thread_current()->stack_rsp;
	void* rsp_ = user ? f->rsp : t->stack_rsp;

	if (not_present){
		page = spt_find_page(spt, addr);
		if (page == NULL){
			if (rsp_- addr == 0x8 || ( addr > rsp_ && USER_STACK > addr)){
				vm_stack_growth(addr);
				return true;
			}
			return false;
		}
		return vm_do_claim_page (page);
	}
	return false;
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
	// struct thread *t = thread_current();
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, pg_round_down(va));
	if (page == NULL){
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	struct thread *t = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	if (pml4_get_page(t->pml4, page->va) == NULL &&  pml4_set_page (t->pml4, page->va, frame->kva, page->writable)){
		return swap_in(page, frame->kva);
	}
	return false;
}

/* Initialize new supplemental page table */
/* 새로운 보충 페이지 테이블을 초기화하는 함수 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// 보충 페이지 테이블 초기화
	spt->pages = calloc(sizeof(struct hash), 1); // 해시 테이블 사용을 위한 동적 할당
	hash_init(spt->pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
/* 보충 페이지 테이블을 src에서 dst로 복사하는 함수 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// TODO : src(부모) spt를 iterator를 이용하여 dst(자식) spt에 복사해주기
	// TODO : page마다 type이 다르므로, copy 방식이 달라야함
	/*	- UNINIT일때는, 그대로 복사하고 claim은 해줄 필요 없음 
		- 외에는 부모 기반으로 똑같이 만들어준 뒤, 바로 claim 해주기 ㄱㄱ*/
	struct hash_iterator i;
	hash_first(&i, src->pages);
	while (hash_next(&i)) {
		struct page *src_cur = hash_entry(hash_cur(&i), struct page, hash_elem);
		// printf("spt copy vmtype : %d\n", src_cur->operations->type);
		// 0 : VM_UNINIT, 1 : VM_ANON, 2 : VM_FILE
		void *va = src_cur->va;
		bool writable = src_cur->writable;
		enum vm_type type = src_cur->operations->type;
		struct segment *aux = calloc(1, sizeof(struct segment));
		switch (VM_TYPE(type)){
			case VM_UNINIT:
				memcpy(aux, src_cur->uninit.aux, sizeof(struct segment));
				if (!vm_alloc_page_with_initializer(src_cur->uninit.type,va,writable,src_cur->uninit.init, aux)){
					free(aux);
					return false;
				}
				break;
			case VM_ANON :
				free(aux);
				if (!(vm_alloc_page(type | VM_MARKER_0,va,writable) && vm_claim_page(va))){
					return false;
				}
				struct page* child_p = spt_find_page(dst, va);
				memcpy(child_p->frame->kva, src_cur->frame->kva, PGSIZE);	
				break;
			case VM_FILE :
				break;
			default :
				PANIC("SPT COPY PANIC!\n");
		}
	}
	return true;
}

void page_destructor(struct hash_elem* hash_elem, void* aux){
	struct page *p = hash_entry(hash_elem, struct page, hash_elem);
	vm_dealloc_page(p);
}

/* Free the resource hold by the supplemental page table */
/* 보충 페이지 테이블에서 리소스 보류를 해제하는 함수 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
   	hash_destroy(&spt->pages, page_destructor);
}

/* Returns a hash value for page p. */
/* 해시 요소에 대한 해시 값을 계산 후 리턴하는 함수의 포인터 */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}
/* Returns true if page a precedes page b. */
/* 해시 요소를 비교하는 함수의 포인터 */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) 
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
  
  return a->va < b->va;
}
