#pragma once
#include "Thread.h"
#include "utils/list.h"

class WaitQueue
{
  public:
	WaitQueue();
	void enqueue(Thread& thread);
	void wake_up();
	void wake_up(size_t num);
	void wake_up_all();

	WaitQueue(WaitQueue&&) = delete;
	WaitQueue(const WaitQueue&) = delete;
	WaitQueue& operator=(WaitQueue&&) = delete;
	WaitQueue& operator=(const WaitQueue&) = delete;
	~WaitQueue();

  private:
	CircularQueue<Thread*> m_threads;
};
