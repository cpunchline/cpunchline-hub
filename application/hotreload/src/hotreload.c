#include <stdio.h>
#include <dlfcn.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#endif
#include "hotreload.h"

static const char *libplug_file_name = PLUG_SO_PATH;
static void *libplug = NULL;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

bool reload_libplug(void)
{
#ifdef _WIN32
    if (libplug != NULL)
        FreeLibrary(libplug);

    libplug = LoadLibrary(libplug_file_name);
    if (libplug == NULL)
    {
        printf("HOTRELOAD: could not load %s: %s\n", libplug_file_name, GetLastError());
        return false;
    }

#define PLUG(name, ...)                                           \
    name = (void *)GetProcAddress(libplug, #name);                \
    if (name == NULL)                                             \
    {                                                             \
        printf("HOTRELOAD: could not find %s symbol in %s: %s\n", \
               #name, libplug_file_name, GetLastError());         \
        return false;                                             \
    }
    LIST_OF_PLUGS
#undef PLUG
#else
    if (libplug != NULL)
        dlclose(libplug);

    libplug = dlopen(libplug_file_name, RTLD_NOW);
    if (libplug == NULL)
    {
        printf("HOTRELOAD: could not load %s: %s\n", libplug_file_name, dlerror());
        return false;
    }

#define PLUG(name, ...)                                           \
    name = dlsym(libplug, #name);                                 \
    if (name == NULL)                                             \
    {                                                             \
        printf("HOTRELOAD: could not find %s symbol in %s: %s\n", \
               #name, libplug_file_name, dlerror());              \
        return false;                                             \
    }
    LIST_OF_PLUGS
#undef PLUG
#endif

    return true;
}