#define return_defer_if_fail(value) do { if(!(value)) goto defer; } while (0)
#define return_defer_if(value) do { if((value)) goto defer; } while (0)