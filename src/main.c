#include "../include/alsa/asoundlib.h"

#include <signal.h>

#include "getstream.h"

int main(void)
{
    signal(SIGINT, signal_handler);
    receive_rtmp();

    return 0;
}