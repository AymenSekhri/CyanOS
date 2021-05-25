#pragma once

#include "FSNode.h"
#include "Tasking/SpinLock.h"
#include <List.h>
#include <PathView.h>
#include <Result.h>
#include <Types.h>
#include <UniquePointer.h>

class FileDescription;
class VFS
{
  public:
	static Result<void> mount(UniquePointer<FSNode>&& fs_root);
	static Result<void> unmount();
	static Result<void> remove();
	static Result<void> make_directory();
	static Result<void> remove_directory();
	static Result<void> rename();
	static Result<void> chown();
	static Result<void> make_link();
	static Result<void> remove_link();
	static bool check_exitsts(PathView path);
	static PathView resolve_path(PathView path);

  private:
	static List<UniquePointer<FSNode>> fs_roots;
	static Spinlock lock;

	static Result<FSNode&> traverse_node(PathView);
	static Result<FSNode&> get_node(PathView, OpenMode mode, OpenFlags flags);
	static Result<FSNode&> create_new_node(PathView, OpenMode mode, OpenFlags flags);
	static Result<FSNode&> open_existing_node(PathView);
	static FSNode* get_root_node(StringView root_name);

	friend class FileDescription;
};
