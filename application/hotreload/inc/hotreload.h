#ifndef HOTRELOAD_H_
#define HOTRELOAD_H_

#include <stdbool.h>
#include "plug.h"

#define PLUG(name, ...) extern name##_t *name;
LIST_OF_PLUGS
#undef PLUG
bool reload_libplug(void);

#endif // HOTRELOAD_H_