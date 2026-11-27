# Cross-compile MONOLITH for Windows x64 from macOS or Linux with clang-cl + lld-link and an xwin-splatted
# Microsoft CRT / Windows SDK (https://github.com/Jake-Shadle/xwin; downloading it means accepting Microsoft's license).
#
#   xwin --accept-license --arch x86_64 splat --output <dir>
#   cmake -G Ninja -B build/windows -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x64-clang-cl.cmake -DXWIN_DIR=<dir>
#
# XWIN_DIR must not contain spaces (the flags below are plain strings). LLVM_BIN defaults to Homebrew's llvm.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(XWIN_DIR "$ENV{XWIN_DIR}" CACHE PATH "xwin splat output directory (contains crt/ and sdk/)")
if(NOT XWIN_DIR OR NOT EXISTS "${XWIN_DIR}/crt/include")
    message(FATAL_ERROR "Set XWIN_DIR to an xwin splat output directory (contains crt/ and sdk/)")
endif()
set(LLVM_BIN "/opt/homebrew/opt/llvm/bin" CACHE PATH "Directory holding clang-cl, llvm-lib, llvm-rc, llvm-mt")
find_program(LLD_LINK NAMES lld-link HINTS "${LLVM_BIN}" /opt/homebrew/bin /usr/bin REQUIRED)

set(CMAKE_C_COMPILER   "${LLVM_BIN}/clang-cl")
set(CMAKE_CXX_COMPILER "${LLVM_BIN}/clang-cl")
set(CMAKE_LINKER       "${LLD_LINK}")
set(CMAKE_AR           "${LLVM_BIN}/llvm-lib")
set(CMAKE_RC_COMPILER  "${LLVM_BIN}/llvm-rc")
set(CMAKE_MT           "${LLVM_BIN}/llvm-mt")
set(CMAKE_C_COMPILER_TARGET   x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

set(_inc "/imsvc ${XWIN_DIR}/crt/include /imsvc ${XWIN_DIR}/sdk/include/ucrt /imsvc ${XWIN_DIR}/sdk/include/um /imsvc ${XWIN_DIR}/sdk/include/shared /imsvc ${XWIN_DIR}/sdk/include/winrt /imsvc ${XWIN_DIR}/sdk/include/cppwinrt")
set(CMAKE_C_FLAGS_INIT   "${_inc}")
set(CMAKE_CXX_FLAGS_INIT "${_inc} /EHsc")
set(CMAKE_RC_FLAGS_INIT  "/I ${XWIN_DIR}/sdk/include/um /I ${XWIN_DIR}/sdk/include/shared /I ${XWIN_DIR}/crt/include /I ${XWIN_DIR}/sdk/include/ucrt")

set(_lib "/libpath:${XWIN_DIR}/crt/lib/x86_64 /libpath:${XWIN_DIR}/sdk/lib/um/x86_64 /libpath:${XWIN_DIR}/sdk/lib/ucrt/x86_64")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_lib}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lib}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lib}")

# xwin ships the release CRT only: compile checks in Release, and link the CRT statically so the plugin needs no
# Visual C++ redistributable on the user's machine.
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
