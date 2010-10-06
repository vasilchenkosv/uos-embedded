/*
 * Ethernet controller driver for Milandr 5600ВГ1.
 * Copyright (c) 2010 Serge Vakulenko.
 */
#ifndef __5600BG1_ETH_H
#define __5600BG1_ETH_H

#include <net/netif.h>
#include <buf/buf-queue.h>

#ifndef K5600BG1_STACKSZ
#   define K5600BG1_STACKSZ	1000
#endif

#define K5600BG1_MTU		1518	/* maximum ethernet frame length */

struct _mem_pool_t;
struct _stream_t *stream;

typedef struct _k5600bg1_t {
	netif_t netif;			/* common network interface part */
	mutex_t tx_lock;		/* get tx interrupts here */
	struct _mem_pool_t *pool;	/* memory pool for allocating packets */

	buf_queue_t inq;		/* queue of received packets */
	struct _buf_t *inqdata[8];

	buf_queue_t outq;		/* queue of packets to transmit */
	struct _buf_t *outqdata[8];

	unsigned long intr;		/* interrupt counter */
	unsigned char rxbuf_data [K5600BG1_MTU + 8];
	unsigned char txbuf_data [K5600BG1_MTU + 8];
	unsigned char *rxbuf;		/* aligned rxbuf[] */
	unsigned char *txbuf;		/* aligned txbuf[] */

	ARRAY (stack, K5600BG1_STACKSZ); /* stack for irq task */
} k5600bg1_t;

void k5600bg1_init (k5600bg1_t *u, const char *name, int prio,
	struct _mem_pool_t *pool, arp_t *arp, const unsigned char *macaddr);
void k5600bg1_debug (k5600bg1_t *u, struct _stream_t *stream);
void k5600bg1_start_negotiation (k5600bg1_t *u);
int k5600bg1_get_carrier (k5600bg1_t *u);
long k5600bg1_get_speed (k5600bg1_t *u, int *duplex);
void k5600bg1_set_loop (k5600bg1_t *u, int on);
void k5600bg1_set_promisc (k5600bg1_t *u, int station, int group);
void k5600bg1_poll (k5600bg1_t *u);

#endif /* __5600BG1_ETH_H */