#pragma once
#include "Arch/x86/Spinlock.h"
#include "SpinLock.h"
#include <Bitmap.h>
#include <Function.h>
#include <IntrusiveList.h>
#include <IterationDecision.h>
#include <Result.h>
#include <Types.h>
#include <UniquePointer.h>

enum class ThreadState
{
	Ready,
	BlockedSleep,
	BlockedQueue,
	Suspended,
};
enum class ThreadPrivilege
{
	Kernel,
	User
};

const size_t STACK_SIZE = 0x1000;

class WaitQueue;
class Process;

class Thread : public IntrusiveListNode<Thread>
{
  public:
	typedef void (*thread_function)(uintptr_t argument);

	static Thread* current;
	static Thread& create_thread(Process&, Function<void()>, ThreadPrivilege);
	static Thread& create_init_thread(Process&);
	static void sleep(size_t ms);
	static void yield();
	static size_t number_of_ready_threads();

	void wake_up_from_queue();
	void wake_up_from_sleep();
	void block(WaitQueue& blocker);
	void terminate();
	size_t tid();
	ThreadState state();
	Process& parent_process();
	~Thread();

	template <typename Callback> static void for_each_sleeping(Callback callback)
	{
		auto&& thread = Thread::sleeping_threads.begin();
		while (thread != Thread::sleeping_threads.end()) {
			auto iterator_copy = thread++;
			auto ret = callback(*iterator_copy);
			if (ret == IterationDecision::Break) {
				break;
			}
		}
	}

	template <typename Callback> static void for_each_ready(Callback callback)
	{
		auto&& thread = Thread::ready_threads.begin();
		while (thread != Thread::ready_threads.end()) {
			auto iterator_copy = thread++;
			auto ret = callback(*iterator_copy);
			if (ret == IterationDecision::Break) {
				break;
			}
		}
	}

  private:
	struct UserThreadInformationBlock {
		size_t tid;
	};

	static Bitmap<MAX_BITMAP_SIZE> tid_bitmap;
	static IntrusiveList<Thread> ready_threads;
	static IntrusiveList<Thread> sleeping_threads;
	static Spinlock global_lock;
	static void thread_start(Thread*);

	const size_t m_tid;
	Process& m_parent;
	ThreadState m_state;
	ThreadPrivilege m_privilege;
	Function<void()> m_entry_point;
	WaitQueue* m_blocker;
	UserThreadInformationBlock* m_tib;
	size_t m_sleep_ticks;
	uintptr_t m_kernel_stack_start;
	uintptr_t m_kernel_stack_end;
	uintptr_t m_kernel_stack_pointer;
	uintptr_t m_user_stack_start;

	Thread(Process&, Function<void()>, ThreadPrivilege);
	size_t reserve_tid();
	void cleanup();

	friend class Scheduler;
	friend class WaitQueue;
	friend class IntrusiveList<Thread>;
};
