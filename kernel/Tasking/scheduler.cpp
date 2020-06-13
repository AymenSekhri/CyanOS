#include "scheduler.h"
#include "Arch/x86/atomic.h"
#include "Arch/x86/paging.h"
#include "Devices/Console/console.h"
#include "Devices/Null/null.h"
#include "VirtualMemory/heap.h"
#include "VirtualMemory/memory.h"

ThreadControlBlock* Scheduler::active_thread;
ThreadControlBlock* Scheduler::blocked_thread;
ThreadControlBlock* Scheduler::current_thread;
unsigned tid;

void hello1()
{
	for (size_t i = 0; i < 5; i++) {
		asm("CLI");
		printf("Thread1: (%d).\n", i);
		asm("STI");
	}

	while (1) {
		asm("HLT");
	}
}

void hello2()
{
	for (size_t i = 0; i < 5; i++) {
		asm("CLI");
		printf("Thread2: (%d).\n", i);
		asm("STI");
	}
	Scheduler::thread_sleep(1000);
	asm("HLT");
	for (size_t i = 5; i < 8; i++) {
		asm("CLI");
		printf("Thread2: (%d).\n", i);
		asm("STI");
	}
	while (1) {
		asm("HLT");
	}
}
void hello3()
{
	for (size_t i = 0; i < 5; i++) {
		asm("CLI");
		printf("Thread3: (%d).\n", i);
		asm("STI");
	}
	while (1) {
		asm("HLT");
	}
}

void Scheduler::setup()
{
	tid = 0;
	active_thread = nullptr;
	blocked_thread = nullptr;
	create_new_thread((uintptr_t)hello1);
	create_new_thread((uintptr_t)hello2);
	create_new_thread((uintptr_t)hello3);
	current_thread = active_thread;
}

// Create thread structure of a new thread
void Scheduler::create_new_thread(uintptr_t address)
{
	ThreadControlBlock* new_thread = (ThreadControlBlock*)Heap::kmalloc(sizeof(ThreadControlBlock), 0);
	void* thread_stack = (void*)Memory::alloc(PAGE_SIZE, MEMORY_TYPE::KERNEL, MEMORY_TYPE::WRITABLE);
	new_thread->tid = tid++;
	new_thread->context.esp = (unsigned)thread_stack + PAGE_SIZE;
	new_thread->context.eip = address;
	new_thread->context.eflags = 0x200;
	new_thread->state = ThreadState::INTIALE;
	append_to_thread_list(&active_thread, new_thread);
}

// Append a thread to thread circular list.
void Scheduler::append_to_thread_list(ThreadControlBlock** list, ThreadControlBlock* new_thread)
{
	if (*list) {
		new_thread->next = *list;
		new_thread->prev = (*list)->prev;
		(*list)->prev->next = new_thread;
		(*list)->prev = new_thread;
	} else {
		new_thread->next = new_thread->prev = new_thread;
		*list = new_thread;
	}
}

void Scheduler::delete_from_thread_list(ThreadControlBlock** list, ThreadControlBlock* thread)
{
	if (thread->prev == thread->next) {
		*list = 0;
	} else {
		thread->prev->next = thread->next;
		thread->next->prev = thread->prev;
	}

	Heap::kfree((uintptr_t)thread);
}

// Select next process, save and switch context.
void Scheduler::schedule(ContextFrame* current_context)
{
	wake_up_sleepers();
	if (current_thread->state == ThreadState::RUNNING) {
		save_context(current_context);
		current_thread->state = ThreadState::ACTIVE;
	} else if (current_thread->state == ThreadState::BLOCKED) {
		save_context(current_context);
	}

	volatile ThreadControlBlock* next_thread = select_next_thread();
	next_thread->state = ThreadState::RUNNING;
	switch_context(current_context, (ThreadControlBlock*)next_thread);
	active_thread = (ThreadControlBlock*)next_thread;
	current_thread = active_thread;
}

// Round Robinson Scheduling Algorithm.
ThreadControlBlock* Scheduler::select_next_thread()
{
	ThreadControlBlock* thread_pointer = active_thread->next;
	ThreadControlBlock* next_thread = 0;
	do {
		if (thread_pointer->next == thread_pointer) {
			next_thread = thread_pointer;
			break;
		}
		if ((thread_pointer != active_thread) &&
		    ((thread_pointer->state == ThreadState::ACTIVE) || (thread_pointer->state == ThreadState::INTIALE)) &&
		    (!next_thread)) {
			next_thread = thread_pointer;
		}
		thread_pointer = thread_pointer->next;
	} while (thread_pointer != active_thread->next);
	return next_thread;
}
void Scheduler::wake_up_sleepers()
{
	ThreadControlBlock* thread_pointer = blocked_thread;
	ThreadControlBlock* next_thread = 0;

	if (blocked_thread) {
		do {
			if (thread_pointer->sleep_ticks > 0) {
				thread_pointer->sleep_ticks--;
				if (!thread_pointer->sleep_ticks) {
					thread_pointer->state = ThreadState::ACTIVE;
					delete_from_thread_list(&blocked_thread, thread_pointer);
					append_to_thread_list(&active_thread, thread_pointer);
					current_thread = thread_pointer;
					if (!blocked_thread)
						break;
				}
			}
			thread_pointer = thread_pointer->next;
		} while (thread_pointer != blocked_thread);
	}
}

void Scheduler::thread_sleep(unsigned ms)
{
	DISABLE_INTERRUPTS();
	ThreadControlBlock* thread = active_thread;
	active_thread = active_thread->next;
	thread->sleep_ticks = ms;
	thread->state = ThreadState::BLOCKED;
	delete_from_thread_list(&active_thread, thread);
	append_to_thread_list(&blocked_thread, thread);
	ENABLE_INTERRUPTS();
}

void Scheduler::switch_context(ContextFrame* current_context, ThreadControlBlock* new_thread)
{
	current_context->eax = new_thread->context.eax;
	current_context->ebx = new_thread->context.ebx;
	current_context->ecx = new_thread->context.ecx;
	current_context->edx = new_thread->context.edx;
	current_context->esi = new_thread->context.esi;
	current_context->edi = new_thread->context.edi;
	current_context->eip = new_thread->context.eip;
	current_context->ebp = new_thread->context.ebp;
	current_context->useresp = new_thread->context.esp;
	current_context->eflags = new_thread->context.eflags;
}

void Scheduler::save_context(ContextFrame* current_context)
{
	current_thread->context.eax = current_context->eax;
	current_thread->context.ebx = current_context->ebx;
	current_thread->context.ecx = current_context->ecx;
	current_thread->context.edx = current_context->edx;
	current_thread->context.esi = current_context->esi;
	current_thread->context.edi = current_context->edi;
	current_thread->context.ebp = current_context->ebp;
	current_thread->context.eip = current_context->eip;
	current_thread->context.esp = current_context->useresp;
	current_thread->context.eflags = current_context->eflags;
}

void Scheduler::switch_page_directory(ProcessControlBlock* new_thread)
{
	Paging::load_page_directory(new_thread->page_directory);
}
