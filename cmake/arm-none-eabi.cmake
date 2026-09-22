# Toolchain file for STM32F401RE (Cortex-M4F) with the GNU Arm Embedded toolchain.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(SN_TOOLCHAIN_PREFIX "arm-none-eabi-" CACHE STRING "Cross toolchain prefix")
find_program(SN_ARM_GCC ${SN_TOOLCHAIN_PREFIX}gcc REQUIRED)
get_filename_component(_sn_bin ${SN_ARM_GCC} DIRECTORY)

set(CMAKE_C_COMPILER ${_sn_bin}/${SN_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${_sn_bin}/${SN_TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${_sn_bin}/${SN_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${_sn_bin}/${SN_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_SIZE ${_sn_bin}/${SN_TOOLCHAIN_PREFIX}size CACHE FILEPATH "size")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(SN_CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT "${SN_CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_CXX_FLAGS_INIT "${SN_CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${SN_CPU_FLAGS} --specs=nano.specs -nostartfiles -Wl,--gc-sections")
set(CMAKE_C_FLAGS_RELEASE_INIT "-Os")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Os")
set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Os")
set(CMAKE_C_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Og -g3")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
