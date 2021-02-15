#include "Thread.h"
#include "Arch/x86/Context.h"
#include "Devices/Timer/Pit.h"
#include "Process.h"
#include "ScopedLock.h"
#include "VirtualMemory/Memory.h"
#include "WaitQueue.h"
#include <Assert.h>

Thread* Thread::current = nullptr;
IntrusiveList<Thread> Thread::ready_threads;
IntrusiveList<Thread> Thread::sleeping_threads;
Bitmap<MAX_BITMAP_SIZE> Thread::tid_bitmap;
Spinlock Thread::global_lock;

Thread& Thread::create_thread(Process& process, thread_function address, uintptr_t argument, ThreadPrivilege priv)
{
	ScopedLock local_lock(global_lock);

	Thread& new_thread = ready_threads.push_back(*new Thread(process, address, argument, priv));
	process.list_new_thread(new_thread);

	return new_thread;
}

Thread& Thread::create_init_thread(Process& process)
{
	ASSERT(current == nullptr);
	auto& new_thread = create_thread(process, nullptr, 0, ThreadPrivilege::Kernel);
	current = &new_thread;
	return new_thread;
}

Thread::Thread(Process& process, thread_function address, uintptr_t argument, ThreadPrivilege priv) :
    m_tid{reserve_tid()},
    m_parent{process},
    m_state{ThreadState::Ready},
    m_privilege{priv},
    m_blocker{nullptr}
{
	void* thread_kernel_stack = valloc(0, STACK_SIZE, PAGE_READWRITE);

	uintptr_t stack_pointer = 0;
	ContextInformation info = {.stack = thread_kernel_stack,
	                           .stack_size = STACK_SIZE,
	                           .start_function = uintptr_t(address),
	                           .return_function = uintptr_t(thread_finishing),
	                           .argument = argument};

	if (priv == ThreadPrivilege::Kernel) {
		stack_pointer = Context::setup_task_stack_context(ContextType::Kernel, info);
	} else {
		stack_pointer = Context::setup_task_stack_context(ContextType::User, info);
	}

	m_kernel_stack_start = uintptr_t(thread_kernel_stack);
	m_kernel_stack_end = m_kernel_stack_start + STACK_SIZE;
	m_kernel_stack_pointer = stack_pointer;
	m_state = ThreadState::Ready;

	if (current && (m_parent.pid() != Thread::current->m_parent.pid())) {
		Memory::switch_page_directory(m_parent.page_directory());

		m_tib = reinterpret_cast<UserThreadInformationBlock*>(
		    valloc(0, sizeof(UserThreadInformationBlock), PAGE_USER | PAGE_READWRITE));
		m_tib->tid = m_tid;

		Memory::switch_page_directory(Thread::current->m_parent.page_directory());
	} else {
		m_tib = reinterpret_cast<UserThreadInformationBlock*>(
		    valloc(0, sizeof(UserThreadInformationBlock), PAGE_USER | PAGE_READWRITE));
		m_tib->tid = m_tid;
	}
}

Thread::~Thread()
{
	ASSERT(tid_bitmap.check_set(m_tid));
	tid_bitmap.clear(m_tid);
}

void Thread::wake_up_from_queue()
{
	ScopedLock local_lock(global_lock);

	ready_threads.push_back(*this);
	m_state = ThreadState::Ready;
	m_blocker = nullptr;
}

void Thread::wake_up_from_sleep()
{
	ScopedLock local_lock(global_lock);

	sleeping_threads.remove(*this);
	ready_threads.push_back(*this);
	m_state = ThreadState::Ready;
}

void Thread::block(WaitQueue& blocker)
{
	ScopedLock local_lock(global_lock);

	ready_threads.remove(*this);
	m_state = ThreadState::BlockedQueue;
	m_blocker = &blocker;
}

void Thread::yield()
{
	asm volatile("int $0x81");
}

void Thread::thread_finishing()
{
	warn() << "thread returned.";
	while (1) {
		HLT();
	}
}

size_t Thread::reserve_tid()
{
	size_t id = tid_bitmap.find_first_clear();
	tid_bitmap.set(id);
	return id;
}

void Thread::terminate()
{
	switch (m_state) {
		case ThreadState::Ready: {
			ScopedLock local_lock(global_lock);

			ready_threads.remove(*this);
			break;
		}
		case ThreadState::BlockedSleep: {
			ScopedLock local_lock(global_lock);

			sleeping_threads.remove(*this);
			break;
		}
		case ThreadState::BlockedQueue: {
			ASSERT(m_blocker);
			ScopedLock local_lock(global_lock);

			m_blocker->terminate_blocked_thread(*this);
			break;
		}
		case ThreadState::Suspended: {
			ASSERT_NOT_REACHABLE();
			break;
		}
	}
	m_parent.unlist_new_thread(*this);
	cleanup();
}

void Thread::sleep(size_t ms)
{
	ScopedLock local_lock(global_lock);

	current->m_sleep_ticks = PIT::ticks + ms;
	current->m_state = ThreadState::BlockedSleep;
	ready_threads.remove(*current);
	sleeping_threads.push_back(*current);

	local_lock.release();

	yield();
}

size_t Thread::tid()
{
	ScopedLock local_lock(global_lock);

	return m_tid;
}

Process& Thread::parent_process()
{
	ScopedLock local_lock(global_lock);

	return m_parent;
}

ThreadState Thread::state()
{
	ScopedLock local_lock(global_lock);

	return m_state;
}

size_t Thread::number_of_ready_threads()
{
	ScopedLock local_lock(global_lock);

	return ready_threads.size();
}

void Thread::cleanup()
{
	warn() << "Thread " << m_tid << " is freed from memory.";
	delete this;
}