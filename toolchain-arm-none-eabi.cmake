set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(EXISTS "D:/tools/bin/arm-none-eabi-gcc.exe")
    set(TOOLCHAIN_PATH "D:/tools/bin/")
elseif(EXISTS "D:/tools/arm-toolchain/bin/arm-none-eabi-gcc.exe")
    set(TOOLCHAIN_PATH "D:/tools/arm-toolchain/bin/")
else()
    set(TOOLCHAIN_PATH "")
endif()

set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}arm-none-eabi-gcc.exe")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PATH}arm-none-eabi-gcc.exe")
set(CMAKE_OBJCOPY "${TOOLCHAIN_PATH}arm-none-eabi-objcopy.exe")
set(CMAKE_SIZE "${TOOLCHAIN_PATH}arm-none-eabi-size.exe")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
