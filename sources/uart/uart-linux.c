#include "sys/uos.h"
#include "sys/lib.h"
#include "dev/uart.h"

#if __AVR__
#   include <machine/io.h>
#   define RECEIVE_IRQ		17	/* UART receive complete */
#   define TRANSMIT_IRQ		18	/* UART transmit complete */
#endif

#if ARM_SAMSUNG
#   include <machine/io.h>
#   define RECEIVE_IRQ		5	/* UART0 receive complete */
#   define TRANSMIT_IRQ		4	/* UART0 transmit complete */
#endif

#if LINUX386
#   include <unistd.h>
#   include <signal.h>

#   define __USE_GNU
#   include <fcntl.h>

#   define RECEIVE_IRQ		SIGUSR1	/* UART receive complete */
#   define TRANSMIT_IRQ		SIGUSR2	/* UART transmit complete */

int uart_pid;
#endif

/*
 * Start transmitting a byte.
 * Assume the transmitter is stopped, and the transmit queue is not empty.
 * Return 1 when we are expecting the hardware interrupt.
 */
static int
uart_transmit_start (uart_t *u)
{
	if (u->out_first == u->out_last)
		mutex_signal (&u->transmitter, 0);
#if __AVR__
	/* Check that transmitter buffer is busy. */
	if (! testb (UDRE, USR))
		return 1;
#endif
#if ARM_SAMSUNG
	/* Check for transmitter holding register empty. */
	if (! (ARM_USTAT0 & ARM_USTAT_THE))
		return 1;
#endif
	/* Nothing to transmit or no CTS - stop transmitting. */
	if (u->out_first == u->out_last ||
	    (u->cts_query && u->cts_query (u) == 0)) {
#if __AVR__
		/* Disable `transmitter empty' interrupt. */
		clearb (UDRIE, UCR);
#endif
#if ARM_SAMSUNG
		/* Disable `transmitter holding register empty' interrupt. */
		ARM_UINTEN0 &= ~ARM_UINTEN_THEIE;
#endif
		return 0;
	}

#if __AVR__
	/* Send byte. */
	outb (*u->out_first, UDR);
#endif
#if ARM_SAMSUNG
	/* Send byte. */
	ARM_UTXBUF0 = *u->out_first;
#endif
#if LINUX386
	/* Send byte. */
	write (1, u->out_first, 1);
	kill (uart_pid, TRANSMIT_IRQ);
#endif
	++ u->out_first;
	if (u->out_first >= u->out_buf + UART_OUTBUFSZ)
		u->out_first = u->out_buf;
#if __AVR__
	/* Enable `transmitter empty' interrupt. */
	setb (UDRIE, UCR);
#endif
#if ARM_SAMSUNG
	/* Enable `transmitter holding register empty' interrupt. */
	ARM_UINTEN0 |= ARM_UINTEN_THEIE;
#endif
	return 1;
}

/*
 * Wait for transmitter to finish.
 */
void
uart_transmit_wait (uart_t *u)
{
	mutex_lock (&u->transmitter);
#if __AVR__
	/* Check that transmitter is enabled. */
	if (testb (TXEN, UCR))
#endif
#if ARM_SAMSUNG
	/* Check that transmitter is enabled. */
	if (ARM_UCON0 & ARM_UCON_TMODE_MASK)
#endif
		while (u->out_first != u->out_last)
			mutex_wait (&u->transmitter);
	mutex_unlock (&u->transmitter);
}

/*
 * CTS is active - wake up the transmitter.
 */
void
uart_cts_ready (uart_t *u)
{
	mutex_lock (&u->transmitter);
	uart_transmit_start (u);
	mutex_unlock (&u->transmitter);
}

/*
 * Register the CTS poller function.
 */
void
uart_set_cts_poller (uart_t *u, unsigned char (*func) (uart_t*))
{
	mutex_lock (&u->transmitter);
	u->cts_query = func;
	mutex_unlock (&u->transmitter);
}

/*
 * Send a byte to the UART transmitter.
 */
void
uart_putchar (uart_t *u, char c)
{
	unsigned char *newlast;

	mutex_lock (&u->transmitter);
#if __AVR__
	/* Check that transmitter is enabled. */
	if (testb (TXEN, UCR))
#endif
#if ARM_SAMSUNG
	/* Check that transmitter is enabled. */
	if (ARM_UCON0 & ARM_UCON_TMODE_MASK)
#endif
	{
again:		newlast = u->out_last + 1;
		if (newlast >= u->out_buf + UART_OUTBUFSZ)
			newlast = u->out_buf;
		while (u->out_first == newlast)
			mutex_wait (&u->transmitter);

		*u->out_last = c;
		u->out_last = newlast;
		uart_transmit_start (u);

		if (c == '\n') {
			c = '\r';
			goto again;
		}
	}
	mutex_unlock (&u->transmitter);
}

/*
 * Wait for the byte to be received and return it.
 */
unsigned char
uart_getchar (uart_t *u)
{
	unsigned char c;

	mutex_lock (&u->receiver);

	/* Wait until receive data available. */
	while (u->in_first == u->in_last)
		mutex_wait (&u->receiver);
	c = *u->in_first++;
	if (u->in_first >= u->in_buf + UART_INBUFSZ)
		u->in_first = u->in_buf;

	mutex_unlock (&u->receiver);
	return c;
}

int
uart_peekchar (uart_t *u)
{
	int c;

	mutex_lock (&u->receiver);
	c = (u->in_first == u->in_last) ? -1 : *u->in_first;
	mutex_unlock (&u->receiver);
	return c;
}

/*
 * Receive interrupt task.
 */
static void
uart_receiver (void *arg)
{
	uart_t *u = arg;
	unsigned char c, *newlast;

	/*
	 * Enable transmitter.
	 */
	mutex_lock_irq (&u->transmitter, TRANSMIT_IRQ,
		(handler_t) uart_transmit_start, u);
#if __AVR__
	setb (TXEN, UCR);
#endif
#if ARM_SAMSUNG
	ARM_UCON0 = (ARM_UCON0 & ~ARM_UCON_TMODE_MASK) | ARM_UCON_TMODE_IRQ;
#endif
	mutex_unlock (&u->transmitter);

	/*
	 * Enable receiver.
	 */
	mutex_lock_irq (&u->receiver, RECEIVE_IRQ, 0, 0);
#if __AVR__
	/* Start receiver. */
	setb (RXEN, UCR);

	/* Enable receiver interrupt. */
	setb (RXCIE, UCR);
#endif
#if ARM_SAMSUNG
	/* Start receiver. */
	ARM_UCON0 = (ARM_UCON0 & ~ARM_UCON_RMODE_MASK) | ARM_UCON_RMODE_IRQ;

	/* Enable receiver interrupt. */
	ARM_UINTEN0 |= ARM_UINTEN_RDVIE;
#endif
#if LINUX386
	/* Enable receiver interrupt. */
        fcntl (0, F_SETOWN, uart_pid);
        fcntl (0, F_SETSIG, RECEIVE_IRQ);
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | O_ASYNC);
#endif
	for (;;) {
		mutex_wait (&u->receiver);
#if __AVR__
		/* Check that receive data is available. */
		if (! testb (RXC, USR))
			continue;

		/* Get received byte. */
		c = inb (UDR);
#endif
#if ARM_SAMSUNG
		/* Check that receive data is available. */
		if (! (ARM_USTAT0 & ARM_USTAT_RDV))
			continue;

		/* Get received byte. */
		c = ARM_URXBUF0;
#endif
#if LINUX386
		/* Get received byte. */
		if (read (0, &c, 1) != 1)
			continue;
#endif
#ifndef NDEBUG
		if (c == 3) {
			/* ^C - break and run into debugger */
			abort ();
			continue;
		}
#endif
		newlast = u->in_last + 1;
		if (newlast >= u->in_buf + UART_INBUFSZ)
			newlast = u->in_buf;

		/* Ignore input on buffer overflow. */
		if (u->in_first == newlast)
			continue;

		*u->in_last = c;
		u->in_last = newlast;
	}
}

static stream_interface_t uart_interface = {
	.putc = (void (*) (stream_t*, short))	uart_putchar,
	.getc = (unsigned short (*) (stream_t*))uart_getchar,
	.peek = (int (*) (stream_t*))		uart_peekchar,
};

void
uart_init (uart_t *u, int prio, unsigned int khz, unsigned long baud)
{
	u->interface = &uart_interface;

	u->khz = khz;
	u->in_first = u->in_last = u->in_buf;
	u->out_first = u->out_last = u->out_buf;
#if __AVR__
	/* Setup baud rate generator. */
	outb (((int) (u->khz * 1000L / baud) + 8) / 16 - 1, UBRR);
#endif
#if ARM_SAMSUNG
	ARM_UCON0 = ARM_UCON_WL_8;
	ARM_UINTEN0 = 0;
	ARM_UBRDIV0 = (((u->khz * 500L / baud) + 8) / 16 - 1) << 4;
#endif
#if LINUX386
	uart_pid = getpid ();
#endif

	/* Create uart receive task. */
	task_create (uart_receiver, u, "uartr", prio,
		u->rstack, sizeof (u->rstack));
}
