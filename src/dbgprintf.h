#ifndef DPRINTF_MACRO
#define DPRINTF_MACRO

#ifdef COMMON_PRINTF
    #define DPRINTF(x...) printf(x)
#endif

#ifdef EE_SIO
    #include <sio.h>
    #define DPRINTF(x...) sio_printf(x)
    #define DPRINTF_INIT() sio_init(38400, 0, 0, 0, 0)
#endif

#ifndef DPRINTF
    #define DPRINTF(x...)
    #define NO_DPRINTF
#endif

#ifndef DPRINTF_INIT
    #define DPRINTF_INIT()
    #define NO_DPRINTF_INIT
#endif

#endif