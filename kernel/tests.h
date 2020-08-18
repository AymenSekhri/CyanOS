#pragma once

#include "Devices/Console/console.h"
#include "Devices/DebugPort/DebugPort.h"
#include "Devices/DeviceManager.h"
#include "Devices/Timer/pit.h"
#include "Filesystem/FileDescription.h"
#include "Filesystem/VirtualFilesystem.h"
#include "Filesystem/ustar/ustar.h"
#include "Tasking/Loader/pe.h"
#include "Tasking/scheduler.h"
#include "Tasking/semaphore.h"
#include "utils/PathParser.h"
#include "utils/SharedPointer.h"
#include "utils/UniquePointer.h"
#include "utils/bitmap.h"

Semaphore* sem_lock;
void test_semaphore_thread2(uintptr_t arg)
{
	UNUSED(arg);
	printf("Thread2:\n");
	sem_lock->acquire();
	printf("Semaphore acquired by thread2\n");
	Thread::sleep(1000);
	sem_lock->release();
	printf("Semaphore released by thread2\n");
	while (1) {
		HLT();
	}
}

void test_semaphore(uintptr_t arg)
{
	UNUSED(arg);
	printf("Thread1:\n");
	sem_lock = new Semaphore(1);
	sem_lock->acquire();
	Thread::create_thread(Thread::current->parent_process(), test_semaphore_thread2, 0);
	printf("Semaphore acquired by thread1\n");
	Thread::sleep(3000);
	printf("wakeup thread1\n");
	sem_lock->release();
	printf("Semaphore released by thread1\n");
	while (1) {
		HLT();
	}
}

/*void thread_test(uintptr_t arg)
{
    printf("Thread %d\n", arg);
    for (size_t i = 0; i < 3; i++) {
        printf("Thread %d:Hello %d\n", arg, i);
    }
}
void test_threading(uintptr_t arg)
{
    printf("Main thread: creating other threads\n");
    for (size_t i = 0; i < 3; i++) {
        printf("Main thread: Creating thread%d\n", i);
        Scheduler::create_new_thread(0, thread_test, i);
    }
}*/

void test_tar_filesystem(uintptr_t fs, size_t size)
{
	printf("tar at %X\n", fs);
	TarFS* tar_fs = new TarFS((void*)fs, size);
	VFS::mount_root(tar_fs->root_node());
	auto fd = VFS::open("/Drivers/file1.exe", OpenMode::Read, OpenFlags::OpenExisting);
	if (fd.is_error()) {
		printf("error opening the file, error: %d\n", fd.error());
		return;
	}
	char* buff = (char*)Memory::alloc(0xc00, MEMORY_TYPE::KERNEL | MEMORY_TYPE::WRITABLE);
	memset(buff, 0, 4096);
	auto result = fd.value().read(buff, 0xc00);
	if (result.is_error())
		printf("error reading the file %d\n", result.error());
	auto loader_result = PELoader::load(buff, 0xc00);
	if (loader_result.is_error())
		printf("error loading pe file %d\n", loader_result.error());
	printf("file is loaded.\n");
	Memory::free(buff, 0xc00, 0);
}

void test_pipe1(uintptr_t arg)
{
	UNUSED(arg);
	auto fd = VFS::open("/fs/my_pipe", OpenMode::Read, OpenFlags::CreateNew);
	if (fd.is_error()) {
		printf("error opening the file, error: %d\n", fd.error());
		HLT();

		return;
	}
	char* buff = (char*)Memory::alloc(0xc00, MEMORY_TYPE::KERNEL | MEMORY_TYPE::WRITABLE);
	memset(buff, 0, 4096);
	auto result = fd.value().read(buff, 12);
	printf("got it, read\n");
	printf("%s", buff);
	if (result.is_error())
		printf("error reading the file %d\n", result.error());
}

void test_pipe2(uintptr_t arg)
{
	UNUSED(arg);
	Thread::sleep(1000);
	auto fd = VFS::open("/fs/my_pipe", OpenMode::Write, OpenFlags::OpenExisting);
	if (fd.is_error()) {
		printf("error opening the file, error: %d\n", fd.error());
		HLT();
		return;
	}
	char* buff = (char*)Memory::alloc(0xc00, MEMORY_TYPE::KERNEL | MEMORY_TYPE::WRITABLE);
	memset(buff, 0, 4096);
	auto result = fd.value().write(static_cast<const void*>("Hello there"), 12);
	printf("got it, write\n");
	if (result.is_error())
		printf("error writing the file %d\n", result.error());
}

void test_keyboard(uintptr_t arg)
{
	UNUSED(arg);
	auto fd = DeviceManager::open("keybaord", 0, 0);
	if (fd.is_error()) {
		printf("error opening the file, error: %d\n", fd.error());
		HLT();

		return;
	}
	char* buff = (char*)Memory::alloc(0xc00, MEMORY_TYPE::KERNEL | MEMORY_TYPE::WRITABLE);
	memset(buff, 0, 4096);
	auto result = fd.value().receive(buff, 12);
	printf("got it, read\n");
	printf("%s", buff);
	if (result.is_error())
		printf("error reading the file %d\n", result.error());
}