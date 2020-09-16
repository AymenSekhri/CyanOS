#include "Thread.h"
#include "Arch/x86/Context.h"
#include "Devices/Timer/Pit.h"
#include "Process.h"
#include "ScopedLock.h"
#include "VirtualMemory/Memory.h"
#include "WaitQueue.h"
#include <Assert.h>

IntrusiveList<Thread>* Thread::ready_threads = nullptr;
IntrusiveList<Thread>* Thread::sleeping_threads = nullptr;
Thread* Thread::current = nullptr;
Bitmap* Thread::m_tid_bitmap;
StaticSpinlock Thread::global_lock;

void Thread::setup()
{
	global_lock.init();
	m_tid_bitmap = new Bitmap(MAX_BITMAP_SIZE);
	ready_threads = new IntrusiveList<Thread>;
	sleeping_threads = new IntrusiveList<Thread>;
}

Thread& Thread::create_thread(Process& process, thread_function address, uintptr_t argument, ThreadPrivilege priv)
{
	ScopedLock local_lock(global_lock);

	Thread& new_thread = ready_threads->emplace_back(process, address, argument, priv);
	process.threads.emplace_back(new_thread);

	if (current == nullptr) {
		current = &new_thread;
	}

	return new_thread;
}

Thread::Thread(Process& process, thread_function address, uintptr_t argument, ThreadPrivilege priv) :
    m_tid{reserve_tid()},
    m_parent{process},
    m_state{ThreadState::RUNNABLE},
    m_privilege{priv},
    m_blocker{nullptr}
{
	void* thread_kernel_stack = Memory::alloc(STACK_SIZE, MEMORY_TYPE::WRITABLE | MEMORY_TYPE::KERNEL);

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
	m_state = ThreadState::RUNNABLE;
}

Thread::~Thread() {}

void Thread::wake_up_from_queue()
{
	ScopedLock local_lock(global_lock);

	ready_threads->push_back(*this);
	m_state = ThreadState::RUNNABLE;
	m_blocker = nullptr;
}

void Thread::wake_up_from_sleep()
{
	ScopedLock local_lock(global_lock);

	sleeping_threads->remove(*this);
	ready_threads->push_back(*this);
	m_state = ThreadState::RUNNABLE;
}

void Thread::block(WaitQueue& blocker)
{
	ScopedLock local_lock(global_lock);

	ready_threads->remove(*this);
	m_state = ThreadState::BLOCKED_QUEUE;
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

unsigned Thread::reserve_tid()
{
	unsigned id = m_tid_bitmap->find_first_unused();
	m_tid_bitmap->set_used(id);
	return id;
}

void Thread::terminate()
{
	ScopedLock local_lock(global_lock);

	switch (m_state) {
		case ThreadState::RUNNABLE:
			ready_threads->remove(*this);
			break;

		case ThreadState::BLOCKED_SLEEP:
			sleeping_threads->remove(*this);
			break;

		case ThreadState::BLOCKED_QUEUE:
			ASSERT(m_blocker);
			m_blocker->terminate_blocked_thread(*this);
			break;

		case ThreadState::SUSPENDED:
			ASSERT_NOT_REACHABLE();
			break;
	}

	// FIXME: free Thread memory, maybe static function should call this function ?
}

void Thread::sleep(unsigned ms)
{
	ScopedLock local_lock(global_lock);

	current->m_sleep_ticks = PIT::ticks + ms;
	current->m_state = ThreadState::BLOCKED_SLEEP;
	ready_threads->remove(*current);
	sleeping_threads->push_back(*current);

	local_lock.release();

	yield();
}

unsigned Thread::tid()
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

	return ready_threads->size();
}