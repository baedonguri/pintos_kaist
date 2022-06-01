#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);
void argument_stack(char **argv, int argc, struct intr_frame *_if);

// tid_t process_execute (const char *file_name);
// static void start_process (void *file_name_);


/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
/* 새 프로그램을 실행시킬 새 커널 스레드를 만드는 함수 */
/* 해당 file_name을 갖는 스레드를 만든다. 그리고 함수 initd()를 실행한다.*/
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0); 	// 하나의 가용 페이지를 할당하고 그 커널 가상 주소를 리턴 (페이지의 첫번째 주소), 0을 준 이유는 커널 쓰레드를 생성해야하기 때문.
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE); // fn_copy 주소 공간에 file_name을 저장해 넣어주고, 4kb로 길이를 한정한다. (임의로 줌)

	/* project2 : system call */
	char *token, *save_ptr;
	token = strtok_r(file_name, " ", &save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (token, PRI_DEFAULT, initd, fn_copy);
	/* project2 : system call */

	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}
/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
	return thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

	process_init ();

	/* Finally, switch to the newly created process. */
	if (succ)
		do_iret (&if_);
error:
	thread_exit ();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
/* 현재 실행되고 있는 사용자 프로세스를 새 실행 파일의 프로세스로 스위칭한다. */
/* 현재 실행 중인 쓰레드의 context를 f_name에 해당하는 명령을 실행하기 위해 context switching.*/
int
process_exec (void *f_name) { // 유저가 입력한 명령어를 수행하도록 프로그램을 메모리에 적재하고 실행하는 함수. 여기에 파일 네임 인자로 받아서 저장(문자열) => 근데 실행 프로그램 파일과 옵션이 분리되지 않은 상황.
   char *file_name = f_name; // f_name은 문자열인데 위에서 (void *)로 넘겨받음! -> 문자열로 인식하기 위해서 char * 로 변환해줘야.
   bool success;

//    char file_name_address[128]; // 스택에 저장
   // file_name_address = palloc_get_page(PAL_USER); // 이렇게는 가능 but 비효율적
//    memcpy(file_name_address, file_name, strlen(file_name)+1); // strlen에 +1? => 원래 문자열에는 \n이 들어가는데 strlen에서는 \n 앞까지만 읽고 끝내기 때문. 전체를 들고오기 위해 +1

   /* We cannot use the intr_frame in the thread structure.
    * This is because when current thread rescheduled,
    * it stores the execution information to the member. */
   struct intr_frame _if; // intr_frame 내 구조체 멤버에 필요한 정보를 담는다.
   _if.ds = _if.es = _if.ss = SEL_UDSEG;   // data_segment, more_data_seg, stack_seg
   _if.cs = SEL_UCSEG;                  // code_segment
   _if.eflags = FLAG_IF | FLAG_MBS;      // cpu_flag

   /* We first kill the current context */
   process_cleanup ();
   // 새로운 실행 파일을 현재 스레드에 담기 전에 먼저 현재 process에 담긴 context를 지워준다.
   // 지운다? => 현재 프로세스에 할당된 page directory를 지운다는 뜻.
   // context switch를 할때 위에서 저장한 _if로 원복하면서 돌아옴

   /* And then load the binary */
   success = load (file_name, &_if); // file_name, _if를 현재 프로세스에 load.
   // success는 bool type이니까 load에 성공하면 1, 실패하면 0 반환.
   // 이때 file_name: f_name의 첫 문자열을 parsing하여 넘겨줘야 한다!
//    palloc_free_page(file_name);

   if (!success){
      return -1;
   }

   // 디버깅을 위한 툴
//    hex_dump(_if.rsp, _if.rsp, USER_STACK - _if.rsp, true); // 유저 스택에 담기는 값을 확인함. 메모리 안에 있는 걸 16진수로 값을 보여줌

   /* If load failed, quit. */
   // palloc_free_page (file_name); // file_name: 프로그램 파일 받기 위해 만든 임시변수. 따라서 load 끝나면 메모리 반환.

   /* Start switched process. */
   do_iret (&_if);	// 만약 load가 실행되었다면 context_switching을 진행
   NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
/*스레드 TID가 소멸될 때까지 기다렸다가 종료 상태를 반환합니다.
* 커널에 의해 종료된 경우(예외로 인해 중단된 경우)는 -1을 반환합니다. 
* TID가 잘못되었거나 호출 프로세스의 하위 항목이 아니거나 process_wait()가 
* 이미 지정된 TID에 대해 성공적으로 호출된 경우 대기하지 않고 즉시 -1을 반환합니다.*/
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	/* 자식 프로세스가 실행되는 것을 기다렸다가 자식 프로세스가 종료되면 
	* 그 시그널을 받아 부모 스레드가 종료해야 하는데, 
	* 그냥 바로 종료되어 버리므로 자식 프로세스가 실행되지 못한다  
	* 무한 루프로 자식 프로세스가 실행될 수 있도록 함. */

	for (int i = 0; i < 100000000; i++);
	return -1;
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

	process_cleanup ();
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
/* 실행 파일의 file_name을 적재해 실행하는 함수 */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Project 2: Command_line_parsing */
	char *argv[64];
	char *token, *save_ptr;
	int argc = 0;
	for (token=strtok_r(file_name, " ", &save_ptr); token != NULL; token = strtok_r(NULL, " ", &save_ptr)) {
		argv[argc] = token;
		argc++;
	}

	// char *argv[128];
	// int argc = 0;

	// char *token, *save_ptr;
	// token = strtok_r(file_name, " ", &save_ptr); 
	// // arg_list[arg_cnt] = token;

	// while (token != NULL) {
	// 	argv[argc] = token;
	// 	token = strtok_r(NULL, " ", &save_ptr); // NULL이면 strtok_r 내부에서 NULL의 다음 칸, 즉 다음 문자가 시작하는 위치를 가리켜줌
	// 	argc++;
	// }
	
	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();	// 페이지 디렉토리 생성
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());	// 페이지 테이블 활성화

	/* Open executable file. */
	file = filesys_open(file_name); 	// load하고 싶은 파일(함수, 프로그램)을 오픈한다.
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	/* Read and verify executable header. */
	/* ELF파일의 헤더 정보를 읽어와 저장 */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					/* Code, Data 영역을 User Pool에 만듬 */
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* project2 : command parsing line */
	argument_stack(argv, argc, if_);

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
/* 각 프로세스에 할당된 stack을 세팅하는 함수 */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);	// 페이지 할당
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK; 
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	/*해당 가상 주소에 페이지가 아직 없는지 확인한 다음 해당 페이지에 매핑합니다. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */

/* 인자를 차곡차곡 쌓는 함수 */
void argument_stack(char **argv, int argc, struct intr_frame *if_) {
	int argv_len;
	char *arg_address[128];	// arg들의 주소를 담을 배열

	/* argv[0] = "args-single", argv[1] = "onearg" 
	 * stack bottom에 오른쪽 인자부터 쌓아나간다.
	 * 즉, argv[끝]부터 rsp를 감소시키면서 stack에 저장시킴
	*/

	// 1. Save argument strings (character by character)
	// 맨 처음 if_->rsp = 0x47480000(USER_STACK)
	for (int i=argc-1; i >= 0; i--) {	// 가장 idx가 큰 argv부터 쌓는다.
		argv_len = strlen(argv[i]);	// 인자의 길이
		if_->rsp -= (argv_len+1);		// 인자의 길이 + 1 ('\0'까지 고려) 만큼을 rsp에서 빼줌
		memcpy(if_->rsp, argv[i], (argv_len + 1)); // _if_rsp가 가리키고 있는 공간에 해당 인자를 복사.
		arg_address[i] = if_->rsp;		// 현재 문자열 시작 주소 위치를 저장.
	}

	// 2. Word-align padding
	// 스택을 word 정렬해주기 위해 padding 해줌. rsp의 값이 8의 배수가 될 때까지 계속해서 stack에 0을 넣어줌
	/* 	*/
	while (if_->rsp % 8 != 0) {
		if_->rsp--;	// 주소값을 1 내리고
		*(uint8_t *)(if_->rsp) = 0; // 데이터에 0 삽입 -> 8바이트 저장
		// memset(if_->rsp, 0, sizeof(uint8_t));
	}

	// 3. Pointers to the argument strings
	// 주소값 자체를 삽입. 이때 센티넬 포함해서 넣기
	for (int i = argc; i >= 0; i--) {
		if_->rsp -= 8; // 8바이트만큼 내리고
		if (i == argc) // 가장 위에는 NULL이 아닌 0을 넣기
			memset(if_->rsp, 0, sizeof(char **)); // memset (해당 메모리 주소, 초기화할 값, 그 값의 크기)
		else // 나머지에는 arg_address에 있는 값 가져오기
			memcpy(if_->rsp, &arg_address[i], sizeof(char **));
	}
	// if_->R.rsi = if_->rsp;
	// if_->R.rdi = argc;
	// 4. Return address
	/* stack의 맨 위(top)에 Caller 함수의 다음 명령어의 주소. Return Address를 삽입한다.
	 * 다만, 현재 Caller는 사용자 프로세스는 바로 종료되어야 하므로, 그 주소를 NULL pointer인 0으로 넣어준다. */
	if_->rsp -= 8 ;
	memset(if_->rsp, 0, sizeof(void *));

	if_->R.rdi = argc;
	if_->R.rsi = if_->rsp + 8; //fake_address 바로 위: arg_address 맨 앞 가리키는 주소값!
}