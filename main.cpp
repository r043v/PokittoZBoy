#include <algorithm>
#include "Pokitto.h"
//#include "PokittoMini.h"

//#include "stdint.h"
//#include "./PokittoLib/POKITTO_CORE/Macros_LCD.h"
using namespace Pokitto;

#include "save.hpp"

//#include <USBSerial.h>
//USBSerial serial;

extern uint32_t scaling;

extern "C" {
  #include "drv.h"

  //void asmqsort( int*f, int n );

  void cqsort( int*f, int*l ){
    std::sort(f,l);
  }

  uint8_t * btnStates = Pokitto::heldStates;

  void pollButtons( void ){
    Buttons::pollButtons();
  }

  volatile uint32_t * LCD = (uint32_t*)0xA0002188;

  void __wrap_free(void *){}

  void write_command_16(uint16_t data){
     CLR_CS; // select lcd
     CLR_CD; // clear CD = command
     SET_RD; // RD high, do not read
    *LCD = uint32_t(data) << 3;
     CLR_WR_SLOW;  // WR low
     SET_WR;  // WR low, then high = write strobe
     SET_CS; // de-select lcd
  }

  void write_data_16(uint16_t data){
    CLR_CS;
    SET_CD;
    SET_RD;
    *LCD = uint32_t(data) << 3;
    CLR_WR;
    SET_WR;
    SET_CS;
  }

  void zboymain(void);
}


// extern uint8_t *framebuffer;

extern "C" void print(char *msg) {
  Pokitto::setWindow( 0, 0, 176, 220 );
  Pokitto::Display::enableDirectPrinting(true);
  Pokitto::Display::setCursor(1, 1);
  Pokitto::Display::print(msg);
  Pokitto::setWindow( 0, 10, 176, 199+10 );
  //wait_ms(10);
}

extern "C" void SetUserMsg(char *msg) {
  Pokitto::setWindow( 0, 0, 176, 220 );
    Pokitto::Display::enableDirectPrinting(true);
    Pokitto::Display::setCursor(1, 1);
    Pokitto::Display::print(msg);
  //#ifdef SCALING
  if( scaling ){
    Pokitto::setWindow( 0, 10, 176, 199+10 );
  } else {
  //#else
    Pokitto::setWindow( 16, 30, 144+15, 159+30 );
  }
  //#endif
  wait_ms(1000);
}

extern "C" void flipPalette( void );
extern "C" void flipScaling( void );
extern "C" void checkMapper( void );

uint32_t prevTime;
extern uint32_t frameCount;

uint32_t reported = 0, changes = 0, selectbtn = 0;
/* returns the next input event in queue */
int drv_keypoll(void){

  if( (changes & (1<<UPBIT)) && !(reported & (1<<UPBIT)) ){
    reported |= 1<<UPBIT;
    if( Buttons::buttons_state & (1<<UPBIT) )
      return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_UP;
    else
      return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_UP;
  }

  if( (changes & (1<<DOWNBIT)) && !(reported & (1<<DOWNBIT)) ){
    reported |= 1<<DOWNBIT;
    if( Buttons::buttons_state & (1<<DOWNBIT) )
      return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_DOWN;
    else
      return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_DOWN;
  }

  if( (changes & (1<<LEFTBIT)) && !(reported & (1<<LEFTBIT)) ){
    reported |= 1<<LEFTBIT;
    if( Buttons::buttons_state & (1<<LEFTBIT) )
      return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_LEFT;
    else
      return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_LEFT;
  }

  if( (changes & (1<<RIGHTBIT)) && !(reported & (1<<RIGHTBIT)) ){
    reported |= 1<<RIGHTBIT;
    if( Buttons::buttons_state & (1<<RIGHTBIT) )
      return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_RIGHT;
    else
      return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_RIGHT;
  }

  if( (changes & (1<<CBIT)) && !(reported & (1<<CBIT)) ){
    reported |= 1<<CBIT;
    uint32_t btn = DRV_INPUT_KEY_RET;
      if( Buttons::buttons_state & (1<<ABIT) ){
  	     btn = DRV_INPUT_KEY_F5;
      } else if( Buttons::buttons_state & (1<<BBIT) ){
  	     btn = DRV_INPUT_KEY_F7;
      } else if( Buttons::buttons_state & (1<<UPBIT) ){
        // C + UP, switch palette
        if( Buttons::buttons_state & (1<<CBIT) ){
          flipPalette(); btn = 0;
        }
      } else if( Buttons::buttons_state & (1<<DOWNBIT) ){
        if( Buttons::buttons_state & (1<<CBIT) ){
          flipScaling(); btn = 0;
        }
      }

    if( Buttons::buttons_state & (1<<CBIT) )
      return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | btn;
    else
      return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | btn;
  }

  if( (changes & (1<<ABIT)) && !(reported & (1<<ABIT)) ){
    reported |= 1<<ABIT;
    if( Buttons::buttons_state & (1<<ABIT) )
      return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_LALT;
    else
      return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_LALT;
  }

  if( (changes & (1<<BBIT)) && !(reported & (1<<BBIT)) ){
    reported |= 1<<BBIT;
    if( Buttons::buttons_state & (1<<BBIT) )
      return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_LCTRL;
    else
      return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | DRV_INPUT_KEY_LCTRL;
  }

  if( (changes & (1<<7)) && !(reported & (1<<7)) ){
      reported |= 1<<7;
      uint32_t btn = DRV_INPUT_KEY_TAB;
      if( selectbtn )
	  return DRV_INPUT_KEYDOWN | DRV_INPUT_KEYBOARD | btn;
      else
	  return DRV_INPUT_KEYUP | DRV_INPUT_KEYBOARD | btn;
  }

  reported = 0;
  Buttons::pollButtons();

  changes = Buttons::buttons_state ^ (Buttons::buttons_held | Buttons::buttons_released);

  if( !*((uint8_t *) 0xA0000001) ){
    if( !selectbtn )
      changes |= 1<<7;
    selectbtn = 1;
  }else if( selectbtn ){
    changes |= 1<<7;
    selectbtn = 0;
  }

  return DRV_INPUT_NONE;

}

/* loads a palette of colors into zBoy. *palette must be an array of at least
 * 256 color values written in 32bits each as RGB triplets */
int drv_loadpal(uint32_t *palette){
  return 0;
}

/* returns a monotonously increasing counter that increases by 1 every
 * milisecond */
unsigned long drv_getticks(void){
  return Pokitto::Core::getTime();
}

const uint16_t border[] = {0};

extern "C" void indexRAM();
/*
//#ifndef SCALING
const uint16_t borderP[256] = {
  0xadde,
  0xefbe,
  0x0007,
  0x0000
};

const uint8_t borderT[
		       220*16
] = {
  0xDE,0xAD,
  0xBE,0xEF,
  0x03,0x00,
  0x00,0x00,
};

const uint8_t borderB[
		       220*16
] = {
  0xDE,0xAD,
  0xBE,0xEF,
  0x04,0x00,
  0x00,0x00,
};


const uint8_t borderL[
		       30*(176-16-16)
] = {
  0xDE,0xAD,
  0xBE,0xEF,
  0x05,0x00,
  0x00,0x00,
};

const uint8_t borderR[
		       30*(176-16-16)
] = {
  0xDE,0xAD,
  0xBE,0xEF,
  0x06,0x00,
  0x00,0x00,
};

void blit( int x, int y, int w, int h, const uint8_t *p ){
  Pokitto::setWindow( y, x, (y+h)-1, (x+w)-1 );
  write_command_16(0x22);
  CLR_CS_SET_CD_RD_WR;
  int max = w*h;
  for( int i=max; i; --i ){
    LPC_GPIO_PORT->MPIN[2] = uint32_t( borderP[*p++] ) << 3; CLR_WR; SET_WR;
  }
}
//#endif
*/

#define screenFullScreen Pokitto::setWindow( 0, 10, 176, 199+10 )
#define screenGbCenter Pokitto::setWindow( 16, 30, 144+15, 159+30 )

extern "C" void screenInit( void ){
  SET_MASK_P2;
  write_command_16(0x03);
  write_data_16(0x1038);

  Pokitto::lcdClear();

  scaling ? screenFullScreen : screenGbCenter;

  write_command_16(0x22);
  CLR_CS_SET_CD_RD_WR;
}

int main () {

  Core::begin();
  //Pokitto::initButtons();

  pokInitSD();

  *(uint32_t *)0x40048080 |= 3 << 26;

  checkMapper();

  indexRAM();

  screenInit();

  zboymain();

  return 0;
}
