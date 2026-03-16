#pragma once

// OS
#if defined(_WIN64)
#define PLATFORM_OS_WIN64
#elif defined(_WIN32)
#define PLATFORM_OS_WIN32
#elif defined(__ANDROID__)
#define PLATFORM_OS_ANDROID
#elif defined(__linux__)
#define PLATFORM_OS_LINUX
#elif defined(__APPLE__)
#define PLATFORM_OS_DARWIN
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define PLATFORM_OS_FREEBSD
#define PLATFORM_OS_BSD
#elif defined(__NetBSD__)
#define PLATFORM_OS_NETBSD
#define PLATFORM_OS_BSD
#elif defined(__OpenBSD__)
#define PLATFORM_OS_OPENBSD
#define PLATFORM_OS_BSD
#elif defined(__sun) || defined(__sun__)
#define PLATFORM_OS_SOLARIS
#else
#warning "Untested operating system platform!"
#endif

// ARCH
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
#define PLATFORM_ARCH_X64
#define PLATFORM_ARCH_X86_64
#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
#define PLATFORM_ARCH_X86
#define PLATFORM_ARCH_X86_32
#elif defined(__aarch64__) || defined(__ARM64__) || defined(_M_ARM64)
#define PLATFORM_ARCH_ARM64
#elif defined(__arm__) || defined(_M_ARM)
#define PLATFORM_ARCH_ARM
#elif defined(__mips64__)
#define PLATFORM_ARCH_MIPS64
#elif defined(__mips__)
#define PLATFORM_ARCH_MIPS
#elif defined(__riscv)
#define PLATFORM_ARCH_RISCV
#elif defined(__ppc64__) || defined(__powerpc64__)
#define PLATFORM_ARCH_PPC64
#elif defined(__ppc__) || defined(__powerpc__)
#define PLATFORM_ARCH_PPC
#else
#warning "Untested hardware architecture!"
#endif

// COMPILER
#if defined(__clang__)
#define PLATFORM_COMPILER_CLANG
#elif defined(_MSC_VER)
#define PLATFORM_COMPILER_MSVC
#elif defined(__GNUC__)
#define PLATFORM_COMPILER_GCC
#else
#warning "Untested compiler!"
#endif
