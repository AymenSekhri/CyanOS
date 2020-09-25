
#pragma once
#include "../Types.h"
#include <stdlib.h>

extern "C" int main(int argc, const char* argv[]);

extern "C" int __startup()
{
	char *path, *arg;
	asm("movl %%gs:0x4,%0" : "=r"(path));
	asm("movl %%gs:0x8,%0" : "=r"(arg));
	const char* main_args[2];
	main_args[0] = path;
	main_args[1] = arg;
	main(2, main_args);
	return 1;
}