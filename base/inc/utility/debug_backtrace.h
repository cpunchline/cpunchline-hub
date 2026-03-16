#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Debug Backtrace Module
 * 
 * This module provides crash handler functionality that captures and displays
 * stack backtraces when fatal signals occur (SIGSEGV, SIGFPE, SIGILL, SIGABRT, SIGBUS).
 * 
 * The backtrace shows:
 * - Signal information (signal number, name, address)
 * - Stack frames with function names and addresses
 * - Source file locations (requires addr2line)
 * 
 * NOTE: Only enabled in debug builds (when NDEBUG is not defined)
 */

/**
 * Initialize the backtrace signal handlers
 * 
 * This function installs signal handlers for crash-related signals.
 * When a crash occurs, it will:
 * 1. Print the signal information
 * 2. Display the backtrace (function call stack)
 * 3. Show source code locations if available
 * 4. Re-raise the signal for core dump generation
 * 
 * @return 0 on success, -1 on failure
 * 
 * Example:
 * @code
 *   if (debug_backtrace_init(NULL) != 0) {
 *       fprintf(stderr, "Failed to initialize backtrace\n");
 *       return 1;
 *   }
 *   if (debug_backtrace_init("/tmp") != 0) {
 *       fprintf(stderr, "Failed to initialize backtrace\n");
 *       return 1;
 *   }
 * @endcode
 */
extern int debug_backtrace_init(char *crash_file_path);

#ifdef __cplusplus
}
#endif
