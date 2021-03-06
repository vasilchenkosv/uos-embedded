/*
 * Two-line LCD display.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include "lcd.h"

#define RS		0x01            /* 0 - command, 1 - data */
#define E		0x04            /* strobe */
#define D4		0x10		/* data */
#define D5		0x20
#define D6		0x40
#define D7		0x80

#ifdef __AVR__
/*
 * Olimex MT-128 board.
 * LCD indicator is connected to port C.
 */
static void inline __attribute__((always_inline))
set_bits (small_uint_t bits)
{
	PORTC |= bits;
}

static void inline __attribute__((always_inline))
clear_bits (small_uint_t bits)
{
	PORTC &= ~bits;
}
#endif

#ifdef MSP430
/*
 * Olimex easyWeb2 board.
 * LCD indicator is connected to port 2.
 */
#undef RS
#define RS		0x04		/* 0 - command, 1 - data */
#undef E
#define E		0x08		/* strobe */

static void inline __attribute__((always_inline))
set_bits (small_uint_t bits)
{
	P2OUT |= bits;
}

static void inline __attribute__((always_inline))
clear_bits (small_uint_t bits)
{
	P2OUT &= ~bits;
}
#endif

#ifdef PIC32MX
/*
 * SainSmart 1602 LCD Keypad Shield.
 */
#ifdef PIC32MX2                     /* Olimex Pinguino-mx220 board */
#define MASKC_LCD_DB4   (1 << 4)    /* signal D4, pin RC4 */
#define MASKC_LCD_DB5   (1 << 5)    /* signal D5, pin RC5 */
#define MASKC_LCD_DB6   (1 << 6)    /* signal D6, pin RC6 */
#define MASKC_LCD_DB7   (1 << 7)    /* signal D7, pin RC7 */
#define MASKB_LCD_RS    (1 << 7)    /* signal D8, pin RB7 */
#define MASKA_LCD_E     (1 << 10)   /* signal D9, pin RA10 */
#define MASKA_LCD_BL    (1 << 1)    /* signal D10, pin RA1 */
#endif
#ifdef PIC32MX1                     /* Firewing board */
#define MASKA_LCD_DB4   (1 << 4)    /* signal D4, pin RA4 */
#define MASKB_LCD_DB5   (1 << 8)    /* signal D5, pin RB8 */
#define MASKB_LCD_DB6   (1 << 9)    /* signal D6, pin RB9 */
#define MASKA_LCD_DB7   (1 << 3)    /* signal D7, pin RA3 */
#define MASKA_LCD_RS    (1 << 2)    /* signal D8, pin RA2 */
#define MASKB_LCD_E     (1 << 10)   /* signal D9, pin RB10 */
#define MASKB_LCD_BL    (1 << 11)   /* signal D10, pin RB11 */
#endif

static void inline __attribute__((always_inline))
set_bits (small_uint_t bits)
{
#ifdef PIC32MX2                     /* Olimex Pinguino-mx220 board */
        if (bits & (D4 | D5 | D6 | D7))
                LATCSET = bits & (D4 | D5 | D6 | D7);
        if (bits & RS)
                LATBSET = MASKB_LCD_RS;
        if (bits & E)
                LATASET = MASKA_LCD_E;
#endif
#ifdef PIC32MX1                     /* Firewing board */
        if (bits & D4)
                LATASET = MASKA_LCD_DB4;
        if (bits & D5)
                LATBSET = MASKB_LCD_DB5;
        if (bits & D6)
                LATBSET = MASKB_LCD_DB6;
        if (bits & D7)
                LATASET = MASKA_LCD_DB7;
        if (bits & RS)
                LATASET = MASKA_LCD_RS;
        if (bits & E)
                LATBSET = MASKB_LCD_E;
#endif
}

static void inline __attribute__((always_inline))
clear_bits (small_uint_t bits)
{
#ifdef PIC32MX2                     /* Olimex Pinguino-mx220 board */
        if (bits & (D4 | D5 | D6 | D7))
                LATCCLR = bits & (D4 | D5 | D6 | D7);
        if (bits & RS)
                LATBCLR = MASKB_LCD_RS;
        if (bits & E)
                LATACLR = MASKA_LCD_E;
#endif
#ifdef PIC32MX1                     /* Firewing board */
        if (bits & D4)
                LATACLR = MASKA_LCD_DB4;
        if (bits & D5)
                LATBCLR = MASKB_LCD_DB5;
        if (bits & D6)
                LATBCLR = MASKB_LCD_DB6;
        if (bits & D7)
                LATACLR = MASKA_LCD_DB7;
        if (bits & RS)
                LATACLR = MASKA_LCD_RS;
        if (bits & E)
                LATBCLR = MASKB_LCD_E;
#endif
}
#endif

/*
 * LCD commands.
 */
#define DISP_CLR	0x01
#define DISP_ON		0x0C
#define DISP_OFF	0x08
#define CUR_HOME        0x02
#define CUR_OFF 	0x0C
#define CUR_ON_UNDER    0x0E
#define CUR_ON_BLINK    0x0F
#define CUR_LEFT        0x10
#define CUR_RIGHT       0x14
#define CUR_UP  	0x80
#define CUR_DOWN	0xC0
#define DD_RAM_ADDR	0x80
#define DD_RAM_ADDR2	0xC0

static void lcd_putchar (lcd_t *line, short c);

static stream_interface_t lcd_interface = {
	(void (*) (stream_t*, short)) lcd_putchar,
	0, 0,
};

/*
 * Some symbols are missing.
 * Prepare glyphs for download.
 */
#define ROW(a,b,c,d,e) (a<<4 | b<<3 | c<<2 | d<<1 | e)

static const unsigned char backslash [8]  = {
        ROW( 0,0,0,0,0 ),
        ROW( 1,0,0,0,0 ),
        ROW( 0,1,0,0,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,0,0,1,0 ),
        ROW( 0,0,0,0,1 ),
        ROW( 0,0,0,0,0 ),
        ROW( 0,0,0,0,0 ),
};
static const unsigned char leftbrace [8]  = {
        ROW( 0,0,0,1,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,1,0,0,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,0,0,1,0 ),
        ROW( 0,0,0,0,0 ),
};
static const unsigned char rightbrace [8]  = {
        ROW( 0,1,0,0,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,0,0,1,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,0,1,0,0 ),
        ROW( 0,1,0,0,0 ),
        ROW( 0,0,0,0,0 ),
};
static const unsigned char softsign [8]  = {
        ROW( 1,0,0,0,0 ),
        ROW( 1,0,0,0,0 ),
        ROW( 1,0,0,0,0 ),
        ROW( 1,1,1,0,0 ),
        ROW( 1,0,0,1,0 ),
        ROW( 1,0,0,1,0 ),
        ROW( 1,1,1,0,0 ),
        ROW( 0,0,0,0,0 ),
};

/*
 * Convert a symbol from Unicode-16 to local encoding.
 * Olimex LED indicator K2-1602K-FSY-YBW-R has a non-standard charset,
 * which includes ASCII and cyrillic letters.
 */
static unsigned char
unicode_to_local (unsigned short val)
{
	static const unsigned char tab0 [128] = {
/* 00 - 07 */	0,    0,    0,    0,    0,    0,    0,    0,
/* 08 - 0f */	0,    0,    0,    0,    0,    0,    0,    0,
/* 10 - 17 */	0,    0,    0,    0,    0,    0,    0,    0,
/* 18 - 1f */	0,    0,    0,    0,    0,    0,    0,    0,
/*  !"#$%&' */	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
/* ()*+,-./ */	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
/* 01234567 */	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
/* 89:;<=>? */	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
/* @ABCDEFG */	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
/* HIJKLMNO */	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
/* PQRSTUVW */	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
/* XYZ[\]^_ */	0x58, 0x59, 0x5a, 0x5b, 0x04, 0x5d, 0x5e, 0x5f,
/* `abcdefg */	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
/* hijklmno */	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
/* pqrstuvw */	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
/* xyz{|}~  */	0x78, 0x79, 0x7a, 0x05, 0xd1, 0x06, 0xe9, 0xff,
	};
	static const unsigned char tab4 [128] = {
/* 400 - 407 */	0,    0xa2, 0,    0,    0,    0,    0,    0,
/* 408 - 40f */	0,    0,    0,    0,    0,    0,    0,    0,
/* 410 - 417 */	0x41, 0xa0, 0x42, 0xa1, 0xe0, 0x45, 0xa3, 0xa4,
/* 418 - 41f */	0xa5, 0xa6, 0x4b, 0xa7, 0x4d, 0x48, 0x4f, 0xa8,
/* 420 - 427 */	0x50, 0x43, 0x54, 0xa9, 0xaa, 0x58, 0xe1, 0xab,
/* 428 - 42f */	0xac, 0xe2, 0xad, 0xae, 0x07, 0xaf, 0xb0, 0xb1,
/* 430 - 437 */	0x61, 0xb2, 0xb3, 0xb4, 0xe3, 0x65, 0xb6, 0xb7,
/* 438 - 43f */	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0x6f, 0xbe,
/* 440 - 447 */	0x70, 0x63, 0xbf, 0x79, 0xe4, 0x78, 0xe5, 0xc0,
/* 448 - 44f */	0xc1, 0xe6, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
/* 450 - 457 */	0,    0xb5, 0,    0,    0,    0,    0,    0,
/* 458 - 45f */	0,    0,    0,    0,    0,    0,    0,    0,
/* 460 - 467 */	0,    0,    0,    0,    0,    0,    0,    0,
/* 468 - 46f */	0,    0,    0,    0,    0,    0,    0,    0,
/* 470 - 477 */	0,    0,    0,    0,    0,    0,    0,    0,
/* 478 - 47f */	0,    0,    0,    0,    0,    0,    0,    0,
	};
	switch (val & ~0x7f) {
	case 0x0000:
		/* ASCII */
		return flash_fetch (tab0 + val);
	case 0x0400:
		/* Cyrillic */
		return flash_fetch (tab4 + (val & 0x7f));
	}
	return 0;
}

static void lcd_pulse ()
{
	set_bits (E);
	udelay (5);
	clear_bits (E);
}

/*
 * Write command.
 */
static void lcd_write_ctl (unsigned char val)
{
	/* High nibble. Set RS port to 0. */
	clear_bits (D7 | D6 | D5 | D4 | RS);
	set_bits (val & 0xF0);
	lcd_pulse ();

	/* Low nibble. */
	clear_bits (D7 | D6 | D5 | D4);
	set_bits (val << 4);
	lcd_pulse ();

	mdelay (2);
}

/*
 * Write a byte of data.
 */
static void lcd_write_data (unsigned char val)
{
	/* High nibble. Set RS port to 1. */
	clear_bits (D7 | D6 | D5 | D4);
	set_bits ((val & 0xF0) | RS);
	lcd_pulse ();

	/* Low nibble. */
	clear_bits (D7 | D6 | D5 | D4);
	set_bits (val << 4);
	lcd_pulse ();

	mdelay (2);
}

/*
 * Initialize LCD controller. The screen contains two lines.
 * Every line could be printed independently.
 * When timer is give, then wide messages are slowly scrolled.
 */
void lcd_init (lcd_t *line1, lcd_t *line2, timer_t *timer)
{
	small_uint_t i;

	clear_bits (RS | E | D4 | D5 | D6 | D7);

        /* Set pins as outputs, initial 0. */
#ifdef __AVR__
	DDRC |= RS | E | D4 | D5 | D6 | D7;
#endif
#ifdef MSP430
	P2SEL &= ~(RS | E | D4 | D5 | D6 | D7);
	P2DIR |= RS | E | D4 | D5 | D6 | D7;
#endif
#ifdef PIC32MX
#ifdef PIC32MX2
        LATCCLR = MASKC_LCD_DB4 | MASKC_LCD_DB5 |
                  MASKC_LCD_DB6 | MASKC_LCD_DB7;
        TRISCCLR = MASKC_LCD_DB4 | MASKC_LCD_DB5 |
                  MASKC_LCD_DB6 | MASKC_LCD_DB7;
        LATBCLR = MASKB_LCD_RS;
        TRISBCLR = MASKB_LCD_RS;
        LATACLR = MASKA_LCD_E | MASKA_LCD_BL;
        TRISACLR = MASKA_LCD_E | MASKA_LCD_BL;
#endif
#ifdef PIC32MX1
        LATBCLR = MASKB_LCD_DB5 | MASKB_LCD_DB6 | MASKB_LCD_E | MASKB_LCD_BL;
        TRISBCLR = MASKB_LCD_DB5 | MASKB_LCD_DB6 | MASKB_LCD_E | MASKB_LCD_BL;
        LATACLR = MASKA_LCD_DB4 | MASKA_LCD_DB7 | MASKA_LCD_RS;
        TRISACLR = MASKA_LCD_DB4 | MASKA_LCD_DB7 | MASKA_LCD_RS;
#endif
#endif
	mdelay (110);

	/* Initialize 4-bit bus. */
	set_bits (D5 | D4);
	lcd_pulse ();
	mdelay (10);

	lcd_pulse ();
	mdelay (10);

	lcd_pulse ();
	mdelay (10);

	clear_bits (D4);
	lcd_pulse ();
	mdelay (10);

	/* Clear screen */
	lcd_write_ctl (DISP_CLR);

	/* Enable display */
	lcd_write_ctl (DISP_ON);

	line1->interface = &lcd_interface;
	line1->timer = timer;
	line1->base = DD_RAM_ADDR;
	line1->col = 0;
	line1->c1 = 0;
	line1->c2 = 0;

	line2->interface = &lcd_interface;
	line2->timer = timer;
	line2->base = DD_RAM_ADDR2;
	line2->col = 0;
	line2->c1 = 0;
	line2->c2 = 0;

	for (i=0; i<NCOL; ++i) {
		line1->data[i] = ' ';
		line2->data[i] = ' ';
	}

	/* Load missing glyphs: 4 - \, 5 - {, 6 - }, 7 - Ь */
	lcd_load_glyph (4, backslash);
	lcd_load_glyph (5, leftbrace);
	lcd_load_glyph (6, rightbrace);
	lcd_load_glyph (7, softsign);
}

/*
 * Load a symbol glyph.
 * Eight symbols with codes 0-7 are loadable.
 * Glyph data must be placed in flash memory.
 */
void lcd_load_glyph (char n, const unsigned char *data)
{
	small_uint_t i;

	lcd_write_ctl (0x40 + n * 8);		/* установка адреса */
	for (i=0; i<8; ++i)
		lcd_write_data (flash_fetch (data++));
}

/*
 * Clear a single line.
 */
void lcd_clear (lcd_t *line)
{
	unsigned char i;

	lcd_write_ctl (line->base);		/* установка адреса */
	for (i=0; i<NCOL; ++i) {
		lcd_write_data (' ');
		line->data[i] = ' ';
	}
	line->col = 0;
	line->c1 = 0;
	line->c2 = 0;
}

/*
 * Clear all screen.
 */
void lcd_clear_all (lcd_t *line1, lcd_t *line2)
{
	unsigned char i;

	/* Стираем экран */
	lcd_write_ctl (DISP_CLR);

	for (i=0; i<NCOL; ++i) {
		line1->data[i] = ' ';
		line2->data[i] = ' ';
	}
	line1->col = 0;
	line1->c1 = 0;
	line1->c2 = 0;
	line2->col = 0;
	line2->c1 = 0;
	line2->c2 = 0;
}

/*
 * Scroll a line by one character to the left.
 */
static void lcd_scroll (lcd_t *line)
{
	unsigned char i, c;

	if (line->col <= 0)
		return;

	lcd_write_ctl (line->base);		/* setup address */
	for (i=1; i<line->col; ++i) {
		c = line->data[i];
		lcd_write_data (c);
		line->data[i-1] = c;
	}
	lcd_write_data (' ');
	--line->col;
}

/*
 * Move cursor to given position.
 */
void lcd_move (lcd_t *line, int col)
{
	if (col < 0 || col >= NCOL)
		return;
	line->col = col;
}

/*
 * Print one symbol. Decode from UTF8 to local encoding (if not raw mode).
 * Some characters are handled specially.
 */
static void lcd_putchar (lcd_t *line, short c)
{
	switch (c) {
	case '\n':		/* ignore line feeds */
		return;
	case '\t':		/* tab replaced by space */
		c = ' ';
		break;
	case '\f':		/* page feed - clear line */
		lcd_clear (line);
		return;
	case '\r':		/* carriage return - go to begin of line */
		line->col = 0;
		return;
	}

	if (! line->raw) {
		/* Decode UTF-8. */
		if (! (c & 0x80)) {
			line->c1 = 0;
			line->c2 = 0;
		} else if (line->c1 == 0) {
			line->c1 = c;
			return;
		} else if (line->c2 == 0) {
			if (line->c1 & 0x20) {
				line->c2 = c;
				return;
			}
			c = (line->c1 & 0x1f) << 6 | (c & 0x3f);
			line->c1 = 0;
		} else {
			if (line->c1 == 0xEF && line->c2 == 0xBB && c == 0xBF) {
				/* Skip zero width no-break space. */
				line->c1 = 0;
				line->c2 = 0;
				return;
			}
			c = (line->c1 & 0x0f) << 12 |
				(line->c2 & 0x3f) << 6 | (c & 0x3f);
			line->c1 = 0;
			line->c2 = 0;
		}
		/* Convert to LCD encoding. */
		c = unicode_to_local (c);
		if (c == 0)
			c = 0315;
	}
	if (c < 0 || c >= 256)
		c = 0315;

	if (line->col >= NCOL) {
		/* Scrolling. */
		if (line->timer)
			timer_delay (line->timer, 150);
		lcd_scroll (line);
	}
	lcd_write_ctl (line->base | line->col);	/* setup address */
	lcd_write_data (c);
	line->data [line->col++] = c;
}
