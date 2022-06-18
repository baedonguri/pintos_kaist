// /* Growing the stack. */
// static void
// vm_stack_growth(void *addr UNUSED)
// {
// 	if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true) &&
// 		vm_claim_page(addr))
// 	{
// 		return;
// 	}
// }

// /* Handle the fault on write_protected page */
// static bool
// vm_handle_wp(struct page *page UNUSED)
// {
// }

// /* Return true on success */
// bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
// 						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
// {
// 	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
// 	struct page *page = NULL;
// 	/* TODO: Validate the fault */
// 	/* TODO: Your code goes here */
// 	/*
// 	스택 접근일 경우에만 페이지 expand stack
// 	만약 스택접근에 의한 fault 라면, addr은 8byte낮은 곳에서 오류가난 것.
// 	커널에서 page fault -> 사용자 스택 포인터가 아닌 정의되지 않은 값 : f->rsp
// 	syscall_handler 처음에 현재스레드의 rsp에 전달받은 rsp를 저장??
// 	해당 주소를 pg round down 후 크기에맞게 페이지 할당
// 	지금까지 얼마나 늘어났는지는 어떻게 알까? -> pg_round_down(addr+8)
// 	rsp - 8 <= addr < USER_STACK
// 	USER_STACK << 8 까지가 최대
// 	*/
// 	if (not_present)
// 	{
// 		page = spt_find_page(spt, pg_round_down(addr));

// 		if (page)
// 		{
// 			return vm_do_claim_page(page);
// 		}else{ // page가 NULL 이라는 것은 할당되어있지 않은 페이지
// 			// printf("\n-------stack growth start---------\n");
// 			uintptr_t word_size = 64;
// 			uintptr_t rsp = thread_current()->rsp;

// 			printf("\nthread rsp:%x, fix_rsp:%x, if_rsp:%x, addr: %x, pg_rnd_dwn:%x, grow_pg:%x\n", rsp, rsp-word_size, f->rsp, addr, pg_round_down(addr), pg_round_down(addr + word_size));
// 			printf("\n fix_rsp > addr true:1 false:0 %d\n",rsp - word_size > addr);

// 			if (f->rsp - word_size <= addr && addr < f->rsp)
// 			{
// 				printf("\n-------stack growth entry---------\n");
// 				// int max_addr = (int)(USER_STACK >> 8);
// 				vm_stack_growth(pg_round_down(addr));
// 				printf("\n-------stack growth end---------\n");
// 				return true;
// 			}
// 		}	
// 	}
// 	return false;
// }