/*
 * Проверка работы сторожевого таймера IDWT
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <stream/stream.h>
#include <milandr/iwdt.h>

ARRAY (task, 1000);

void hello (void *arg)
{
    iwdt_init (20);
    
	for (;;) {
	    debug_printf ("*");
	    iwdt_ack ();
	}
}

void uos_init (void)
{
	debug_puts ("\nTesting IWDT\r\n\n");
	
	task_create (hello, "Hello!", "hello", 1, task, sizeof (task));
}
