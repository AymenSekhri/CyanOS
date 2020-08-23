#pragma once
#include "Arch/x86/spinlock.h"
#include "Filesystem/FSNode.h"
#include "utils/UniquePointer.h"
#include "utils/types.h"

#define VGATEXTMODE_BUFFER 0x000B8000

typedef enum { MODE_80H25V = 0 } TerminalMode;

typedef enum {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
} VGAColor;

class Console : public FSNode
{
  public:
	static UniquePointer<FSNode> alloc();

	Console();
	~Console();
	Result<void> open(OpenMode mode, OpenFlags flags) override;
	Result<void> close() override;
	Result<void> write(const void* buff, size_t offset, size_t size) override;
	Result<bool> can_write() override;

  private:
	void setMode(TerminalMode Mode);
	void setColor(VGAColor color, VGAColor backcolor);
	void putChar(const char str);
	void clearScreen();
	void initiate_console();
	void printStatus(const char* msg, bool Success);
	void removeLine();
	void pageUp();
	void updateCursor();
	void insertNewLine();
	void insertCharacter(char str);
	void insertBackSpace();
	uint8_t vga_entry_color(VGAColor fg, VGAColor bg);
	uint16_t vga_entry(unsigned char uc, uint8_t color);
};
