SET(triple i686-none-windows)
SET(CMAKE_C_COMPILER_TARGET ${triple})
SET(CMAKE_CXX_COMPILER_TARGET ${triple})


SET(CMAKE_C_FLAGS  "-Og -Wall -Wextra --pedantic -ffreestanding")
SET(CMAKE_CXX_FLAGS  "-Og -Wall -Wextra --pedantic -ffreestanding -fno-exceptions -fno-rtti")

SET(CMAKE_EXE_LINKER_FLAGS  "-nostdlib -Wl,-entry:__startup")

include_directories("stdlib")
add_subdirectory("systemlib")
add_subdirectory("stdlib")