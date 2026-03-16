#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> // needed for sigaction()

#include "hotreload.h"
#include <unistd.h>

int main(void)
{
    struct sigaction act = {0};
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);

    if (!reload_libplug())
        return 1;

    {
        const char *file_path = "/tmp/";
        size_t data_size;
        void *data = plug_load_resource(file_path, &data_size);
        plug_free_resource(data);
    }

    plug_init();

    while (1)
    {
        void *state = plug_pre_reload();
        if (!reload_libplug())
            return 1;
        plug_post_reload(state);
        plug_update();
        sleep(3);
    }

    return 0;
}