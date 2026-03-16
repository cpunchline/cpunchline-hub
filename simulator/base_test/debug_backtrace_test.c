#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utility/debug_backtrace.h"

/**
 * Debug Backtrace Test Suite
 * 
 * This test suite demonstrates the debug backtrace functionality by triggering
 * various types of crashes and errors. Each test will cause the program to crash,
 * but the backtrace handler will capture and display the call stack before exit.
 * 
 * Test scenarios:
 * 1. SIGSEGV - Segmentation fault (null pointer dereference)
 * 2. SIGFPE  - Floating point exception (division by zero)
 * 3. SIGILL  - Illegal instruction
 * 4. SIGABRT - Abort signal
 * 5. Deep stack - Multiple nested function calls leading to crash
 */

// Test 1: Null pointer dereference (SIGSEGV)
static void test_segfault(void)
{
    printf("\n=== Test 1: Segmentation Fault (SIGSEGV) ===\n");
    printf("Attempting to dereference NULL pointer...\n");
    int *ptr = NULL;
    *ptr = 42; // This will trigger SIGSEGV
}

// Test 2: Division by zero (SIGFPE)
static void test_division_by_zero(void)
{
    printf("\n=== Test 2: Floating Point Exception (SIGFPE) ===\n");
    printf("Attempting division by zero...\n");
    volatile int a = 10;
    volatile int b = 0;
    volatile int c = a / b; // This will trigger SIGFPE
    (void)c;
}

// Test 3: Illegal instruction (SIGILL)
static void test_illegal_instruction(void)
{
    printf("\n=== Test 3: Illegal Instruction (SIGILL) ===\n");
    printf("Attempting to execute illegal instruction...\n");
#if defined(__x86_64__) || defined(__i386__)
    __asm__("ud2"); // Undefined instruction for x86
#elif defined(__arm__) || defined(__aarch64__)
    __asm__(".word 0xe7f000f0"); // Undefined instruction for ARM
#else
    printf("Illegal instruction test not supported on this architecture\n");
    exit(0);
#endif
}

// Test 4: Abort signal (SIGABRT)
static void test_abort(void)
{
    printf("\n=== Test 4: Abort Signal (SIGABRT) ===\n");
    printf("Calling abort()...\n");
    abort();
}

// Test 5: Deep call stack for backtrace
static void level_5(void)
{
    printf("Level 5: Triggering segfault\n");
    int *ptr = NULL;
    *ptr = 1;
}

static void level_4(void)
{
    printf("Level 4\n");
    level_5();
}

static void level_3(void)
{
    printf("Level 3\n");
    level_4();
}

static void level_2(void)
{
    printf("Level 2\n");
    level_3();
}

static void level_1(void)
{
    printf("Level 1\n");
    level_2();
}

static void test_deep_stack(void)
{
    printf("\n=== Test 5: Deep Call Stack ===\n");
    printf("Creating deep call stack...\n");
    level_1();
}

static void print_usage(const char *prog_name)
{
    printf("Usage: %s <test_number>\n", prog_name);
    printf("       %s -h|--help\n\n", prog_name);
    printf("Available tests:\n");
    printf("  1 - Segmentation fault (SIGSEGV)\n");
    printf("  2 - Division by zero (SIGFPE)\n");
    printf("  3 - Illegal instruction (SIGILL)\n");
    printf("  4 - Abort signal (SIGABRT)\n");
    printf("  5 - Deep call stack\n");
    printf("\nExample:\n");
    printf("  %s 1    # Run segmentation fault test\n", prog_name);
    printf("  %s 5    # Run deep call stack test\n", prog_name);
}

int main(int argc, char *argv[])
{
    printf("=== Debug Backtrace Test ===\n");

    // Initialize backtrace handler
    if (debug_backtrace_init(NULL) != 0)
    {
        fprintf(stderr, "Failed to initialize debug backtrace\n");
        return 1;
    }

    // Show help if no arguments or help flag
    if (argc == 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
    {
        print_usage(argv[0]);
        return 0;
    }

    // Parse test number
    int test_num = atoi(argv[1]);
    if (test_num < 1 || test_num > 5)
    {
        fprintf(stderr, "Invalid test number: %d\n", test_num);
        print_usage(argv[0]);
        return 1;
    }

    printf("\nRunning test %d...\n", test_num);

    switch (test_num)
    {
        case 1:
            test_segfault();
            break;
        case 2:
            test_division_by_zero();
            break;
        case 3:
            test_illegal_instruction();
            break;
        case 4:
            test_abort();
            break;
        case 5:
            test_deep_stack();
            break;
        default:
            fprintf(stderr, "Unknown test number: %d\n", test_num);
            return 1;
    }

    // Should not reach here
    printf("Test completed (unexpected)\n");
    return 0;
}