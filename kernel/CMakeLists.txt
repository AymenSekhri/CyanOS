include_directories(".")
include_directories("../libraries/stdlib")


SET(triple i686-none-windows)
SET(CMAKE_C_COMPILER_TARGET ${triple})
SET(CMAKE_CXX_COMPILER_TARGET ${triple})
SET(CMAKE_C_FLAGS  "-O0 -ggdb -Wall -Wextra --pedantic -ffreestanding")
SET(CMAKE_CXX_FLAGS  "-O0 -ggdb -Wall -Wextra --pedantic -ffreestanding -fno-exceptions -fno-rtti")
SET(CMAKE_EXE_LINKER_FLAGS  "-Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -nostdlib")



add_compile_definitions("CYANOS_KENREL")

file(GLOB_RECURSE SOURCES "*.cpp" "*.asm" "../libraries/stdlib/*.cpp")
add_executable(${KERNEL_EXE} ${SOURCES})
add_custom_command(
    TARGET ${KERNEL_EXE}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ARGS -O binary $<TARGET_FILE:${KERNEL_EXE}> ${KERNEL_IMAGE}
    COMMENT "Striping the kernel output file..."
)