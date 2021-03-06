// NOTE:
// the original aleph/teletype screen went EOL. p/n NHD-2.7-12864UCY3
// replacement screen is p/n NHD-2.7-12864WDY3 which uses a slightly different controller
// hence the need for slightly different command configs. (also discovered the new one requires
// 20mhz.)
// revision detection is automatic. teletype has a bridge from B00 to B01, see init_teletype.c
// for get_revision()

// ASF
#include "board.h"
#include "delay.h"
#include "gpio.h"
#include "intc.h"
#include "print_funcs.h"
#include "spi.h"

// libavr32
#include "font.h"
#include "screen.h"
#include "init_teletype.h"
#include "interrupts.h"

//-----------------------------
//---- variables

// screen buffer
static u8 screenBuf[GRAM_BYTES];

// common temp vars
static u32 i, j;
static u8* pScr; // movable pointer to screen buf
static u32 nb; // count of destination bytes
static u8 _is_screen_flipped = 0;

// revision
static u8 rev;

// functions per revision
static void screen_set_rect_1(u8 x, u8 y, u8 w, u8 h);
static void screen_set_rect_2(u8 x, u8 y, u8 w, u8 h);

static void write_command(U8 c);
static void write_command(U8 c) {
  u8 irq_flags = irqs_pause();
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  // pull register select low to write a command
  gpio_clr_gpio_pin(OLED_DC_PIN);
  spi_write(OLED_SPI, c);
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);
  irqs_resume(irq_flags);
}

static void write_data(u8 d);
static void write_data(u8 d) {
  u8 irq_flags = irqs_pause();
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  // pull register select low to write a command
  gpio_set_gpio_pin(OLED_DC_PIN);
  spi_write(OLED_SPI, d);
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);
  irqs_resume(irq_flags);
}

// set_rect pointer
void (*_screen_set_rect)(u8 x, u8 y, u8 w, u8 h);

// set the current drawing area of the physical screen (hopefully)
static void screen_set_rect(u8 x, u8 y, u8 w, u8 h);
void screen_set_rect(u8 x, u8 y, u8 w, u8 h) {
  _screen_set_rect(x, y, w, h);
}

// set_rect (original)
void screen_set_rect_1(u8 x, u8 y, u8 w, u8 h) {
  // set column address
  write_command(0x15);		// command
  write_command(x);	// column start
  write_command((x+w-1));	// column end
  // set row address
  write_command(0x75);		// command
  write_command(y);	// column start
  write_command(y+h-1);	// column end
}

// set_rect (new)
void screen_set_rect_2(u8 x, u8 y, u8 w, u8 h) {
  x = x + 28; // new revision column starts at 28
  u8 irq_flags = irqs_pause();

  // set column address
  gpio_clr_gpio_pin(OLED_DC_PIN);
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  spi_write(OLED_SPI, 0x15);
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);
  
  gpio_set_gpio_pin(OLED_DC_PIN);
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  spi_write(OLED_SPI, x);
  spi_write(OLED_SPI, x+w-1);
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);

  // set row address
  gpio_clr_gpio_pin(OLED_DC_PIN);
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  spi_write(OLED_SPI, 0x75);
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);

  gpio_set_gpio_pin(OLED_DC_PIN);
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  spi_write(OLED_SPI, y);
  spi_write(OLED_SPI, y+h-1);
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);

  irqs_resume(irq_flags);
}

void (*_writeScreenBuffer)(u8 x, u8 y, u8 w, u8 h);

static void writeScreenBuffer(u8 x, u8 y, u8 w, u8 h) {
  _writeScreenBuffer(x,y,w,h);
}

static void writeScreenBuffer1(u8 x, u8 y, u8 w, u8 h) {
  // set drawing region
  screen_set_rect(x, y, w, h);

  u8 irq_flags = irqs_pause();

  // select chip for data
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  // register select high for data
  gpio_set_gpio_pin(OLED_DC_PIN);
  // send data
  for(i=0; i<(nb); i++) {
    spi_write(OLED_SPI, screenBuf[i]);
  }
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);

  irqs_resume(irq_flags);
}

static void writeScreenBuffer2(u8 x, u8 y, u8 w, u8 h) {
  // set drawing region
  screen_set_rect(x, y, w, h);

  u8 irq_flags = irqs_pause();

  // select chip for data
  gpio_clr_gpio_pin(OLED_DC_PIN);
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  spi_write(OLED_SPI, 0x5C); // start pixel data write to GDDRAM
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);

  // register select high for data
  gpio_set_gpio_pin(OLED_DC_PIN);
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  // send data
  u8 a, b;
  for(i=0; i<w*h; i++) {
    a = screenBuf[i] & 0x0F;
    b = screenBuf[i] & 0xF0;
    spi_write(OLED_SPI, (a << 4) | a);
    spi_write(OLED_SPI, b | (b >> 4));
  }
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);

  irqs_resume(irq_flags);
}

void (* _screen_clear)(void);
// clear OLED RAM and local screenbuffer
void screen_clear(void) {
  _screen_clear();
}

// clear OLED RAM and local screenbuffer
static void screen_clear1(void) {
  u8 irq_flags = irqs_pause();
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  // pull register select high to write data
  gpio_set_gpio_pin(OLED_DC_PIN);
  for(i=0; i<GRAM_BYTES; i++) { 
    screenBuf[i] = 0;
    spi_write(OLED_SPI, 0);
  }
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);
  irqs_resume(irq_flags);
}
// clear OLED RAM and local screenbuffer
static void screen_clear2(void) {
  u8 irq_flags = irqs_pause();
  // select chip for data
  gpio_clr_gpio_pin(OLED_DC_PIN);
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  spi_write(OLED_SPI, 0x5C); // start pixel data write to GDDRAM
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);

  for(i=0; i<GRAM_BYTES; i++) { screenBuf[i] = 0; }
  
  spi_selectChip(OLED_SPI, OLED_SPI_NPCS);
  // pull register select high to write data
  gpio_set_gpio_pin(OLED_DC_PIN);
  for(i=0;i<8192;i++) { 
    spi_write(OLED_SPI, 0);
    spi_write(OLED_SPI, 0);
  }
  spi_unselectChip(OLED_SPI, OLED_SPI_NPCS);
  irqs_resume(irq_flags);
}
//------------------

void init_oled(void) {
  // check rev, set function pointers
  #ifdef MOD_ALEPH 
  rev = 0;
  #else
  rev = get_revision(); 
  #endif

  print_dbg("\r\nrev ");
  print_dbg_ulong(rev);

  if(rev) {
    _screen_set_rect = &screen_set_rect_2;
    _writeScreenBuffer = &writeScreenBuffer2;
    _screen_clear = &screen_clear2;
  } else {
    _screen_set_rect = &screen_set_rect_1;
    _writeScreenBuffer = &writeScreenBuffer1;
    _screen_clear = &screen_clear1;
  }

  Disable_global_interrupt();
  // flip the reset pin
  gpio_set_gpio_pin(OLED_RES_PIN);
  delay_ms(1);
  gpio_clr_gpio_pin(OLED_RES_PIN);
  delay_ms(1);
  gpio_set_gpio_pin(OLED_RES_PIN);
  delay_ms(10);

  if(rev) { // NEW rev
    //// initialize OLED
    write_command(0xAE);	// off
    write_command(0xB3);	// clock rate
    write_data(0x91);
    write_command(0xCA);	// mux ratio
    write_data(0x3F);
    write_command(0xA2);  // set offset
    write_data(0);
    write_command(0xAB);  // internal vdd reg
    write_data(0x01);
    write_command(0xA0);	// remap
    write_data(0x16);
    write_data(0x11);
    write_command(0xC7);  // master contrast current
    write_data(0x0f);
    write_command(0xC1);  // set contrast current
    write_data(0x9F);
    write_command(0xB1);	// phase length
    write_data(0xF2);
    write_command(0xBB);  // set pre-charge voltage
    write_data(0x1F);
    write_command(0xB4);  // set vsl
    write_data(0xA0);
    write_data(0xFD);
    write_command(0xBE);  // set VCOMH
    write_data(0x04);
    write_command(0xA6);  // set normal display
} else { // ORIG rev
    //// initialize OLED
    write_command(0xAE);	// off
    write_command(0xB3);	// clock rate
    write_command(0x91);
    write_command(0xA8);	// multiplex
    write_command(0x3F);
    write_command(0x86);	// full current range
    write_command(0x81);	// contrast to full
    write_command(0x7F);
    write_command(0xB2);	// frame freq
    write_command(0x51);
    write_command(0xA8);	// multiplex
    write_command(0x3F);
    write_command(0xBC);	// precharge
    write_command(0x10);
    write_command(0xBE);	// voltage
    write_command(0x1C);
    write_command(0xAD);	// dcdc
    write_command(0x02);
    write_command(0xA0);	// remap
    write_command(0x50);	// 0b01010000
			  // a[6] : enable COM split odd/even
			  // a[4] : enable COM re-map
    write_command(0xA1);	// start
    write_command(0x0);
    write_command(0xA2);	// offset
    write_command(0x4C);
    write_command(0xB1);	// set phase
    write_command(0x55);
    write_command(0xB4);	// precharge
    write_command(0x02);
    write_command(0xB0);	// precharge
    write_command(0x28);
    write_command(0xBF);	// vsl
    write_command(0x0F);
    write_command(0xA4);	// normal display
    write_command(0xB8);	// greyscale table
    write_command(0x01);
    write_command(0x11);
    write_command(0x22);
    write_command(0x32);
    write_command(0x43);
    write_command(0x54);
    write_command(0x65);
    write_command(0x76);
  }

  screen_set_rect(0,0,128,64);

  screen_clear();
  
  write_command(0xAF);	// on
  delay_ms(10) ;
  //  cpu_irq_enable();
  Enable_global_interrupt();
}


// draw data given target rect
// assume x-offset and width are both even!
 void screen_draw_region(u8 x, u8 y, u8 w, u8 h, u8* data) {
  // 1 row address = 2 horizontal pixels
  // physical screen memory: 2px = 1byte
  w >>= 1;
  x >>= 1;
  nb = w * h;
  
  #ifdef MOD_ALEPH // aleph screen is mounted upside down...
    u8 flip = 1;
  #else
    u8 flip = _is_screen_flipped;
  #endif

  if (flip) {
    pScr = (u8*)screenBuf + nb - 1;  
    x = SCREEN_ROW_BYTES - x - w;
    y = SCREEN_COL_BYTES - y - h;
     // copy and pack into the screen buffer
    // 2 bytes input per 1 byte output
    for(j=0; j<h; j++) {
      for(i=0; i<w; i++) {
        *pScr = (0xf0 & ((*data) << 4) );
        data++;
        *pScr |= ((*data) & 0xf);
        data++;
        pScr--;
      }
    }
  } else {
    pScr = (u8*)screenBuf;
     // copy and pack into the screen buffer
    // 2 bytes input per 1 byte output
    for(j=0; j<h; j++) {
      for(i=0; i<w; i++) {
        *pScr = ((*data) & 0xf);
        data++;
        *pScr |= (0xf0 & ((*data) << 4) );
        data++;
        pScr++;
      }
    }
  }

  writeScreenBuffer(x, y, w, h);
}

// draw data at given rectangle, with starting byte offset within the region data.
// will wrap to beginning of region
// useful for scrolling buffers
void screen_draw_region_offset(u8 x, u8 y, u8 w, u8 h, u32 len, u8* data, u32 off) {
  // store region bounds  for wrapping
  // inclusive lower bound
  u8* const dataStart = data;
  // exclusive upper bound
  u8* const dataEnd = data + len - 1;
  // begin at specified offset in region
  data += off;

  // 1 row address = 2 horizontal pixels
  // physical screen memory: 2px = 1byte
  w >>= 1;
  x >>= 1;
  nb = len >> 1;

  
  #ifdef MOD_ALEPH // aleph screen is mounted upside down...
    u8 flip = 1;
  #else
    u8 flip = _is_screen_flipped;
  #endif

  if (flip) {
    pScr = (u8*)screenBuf + nb - 1;  
    x = SCREEN_ROW_BYTES - x - w;
    y = SCREEN_COL_BYTES - y - h;
     // copy and pack into the screen buffer
    // 2 bytes input per 1 byte output
    for(j=0; j<h; j++) {
      for(i=0; i<w; i++) {
  	  *pScr = (0xf0 & ((*data) << 4) );
        data++;
        if(data > dataEnd) { data = dataStart; }
        *pScr |= ((*data) & 0xf);
        data++;
        if(data > dataEnd) { data = dataStart; }
        pScr--;
      }
    }
  } else {
    pScr = (u8*)screenBuf;
     // copy and pack into the screen buffer
    // 2 bytes input per 1 byte output
    for(j=0; j<h; j++) {
      for(i=0; i<w; i++) {
        *pScr = ((*data) & 0xf);
        data++;
        if(data > dataEnd) { data = dataStart; }
        *pScr |= (0xf0 & ((*data) << 4) );
        data++;
        if(data > dataEnd) { data = dataStart; }
        pScr++;
      }
    }
  }
  
  writeScreenBuffer(x, y, w, h);
} 

// set screen orientation
void screen_set_direction(u8 flipped) {
  _is_screen_flipped = flipped;
}
