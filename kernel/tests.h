#include "Devices/Console/console.h"
#include "Tasking/scheduler.h"
#include "Tasking/semaphore.h"

Semaphore* lock;
void thread2()
{
	printf("Thread2:\n");
	lock->acquire();
	printf("Semaphore acquired by thread2\n");
	Scheduler::sleep(1000);
	lock->release();
	printf("Semaphore released by thread2\n");
	while (1) {
		HLT();
	}
}

void thread1()
{
	printf("Thread1:\n");
	Scheduler::sleep(500);
	lock = new Semaphore(1);
	lock->acquire();
	Scheduler::create_new_thread((void*)thread2);
	printf("Semaphore acquired by thread1\n");
	Scheduler::sleep(500);
	lock->release();
	printf("Semaphore released by thread1\n");
	while (1) {
		HLT();
	}
}

typedef struct Stuff_t {
	int value1;
	int value2;
} Stuff;
void test_lists()
{
	CircularQueue<Stuff>* list = new CircularQueue<Stuff>;
	CircularQueue<Stuff>* list2 = new CircularQueue<Stuff>;

	Stuff s1 = {1, 1};
	Stuff s2 = {2, 2};
	Stuff s3 = {3, 3};
	Stuff s4 = {4, 4};
	list->push_back(s1);
	list->push_back(s2);
	list->push_back(s3);
	list->push_back(s4);
	printf("List 1:\n");
	CircularQueue<Stuff>::Iterator itr = list->begin();
	list->move_head_to_other_list(list2);
	list->move_head_to_other_list(list2);
	// list->move_head_to_other_list(list2);
	// list->move_head_to_other_list(list2);
	list->remove(0);

	for (CircularQueue<Stuff>::Iterator i = list->begin(); i != list->end(); i++) {
		printf("%d %d\n", *i, *i);
	}
}