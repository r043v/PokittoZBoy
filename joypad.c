/*
 *  Joypad emulation of a GameBoy console
 *  This file is part of the zBoy project
 *  Copyright (C) Mateusz Viste 2010,2011,2012,2013,2014,2015
 *
 * The joypad is controlled via the register at $FF00:
 *  Bit 5 - P15 out
 *  Bit 4 - P14 out
 *  Bit 3 - P13 in
 *  Bit 2 - P12 in
 *  Bit 1 - P11 in
 *  Bit 0 - P10 in
 *
 * When one of the keys gets a transition from high to low, an interrupt is fired.
 * I got the list of SDL keycodes from http://sdl.beuc.net/sdl.wiki/SDLKey
 */

#define IFLAG_JOYSTICK 1
#define IFLAG_ERROR 2

struct KeyStateMap {  /* Used for internal monitoring of key states */
  int Up;
  int Down;
  int Right;
  int Left;
  int Select;
  int Start;
  int A;
  int B;
};

struct KeyStateMap KeyState;

#define DownIsPressed(); \
  if (KeyState.Down == 0) { \
    KeyState.Down = 1<<3; \
    INT(INT_JOYPAD); \
  }

#define DownIsReleased(); \
  KeyState.Down = 0;

#define UpIsPressed(); \
  if (KeyState.Up == 0) { \
    KeyState.Up = 1<<2; \
    INT(INT_JOYPAD); \
  }

#define UpIsReleased(); \
  KeyState.Up = 0;

#define LeftIsPressed(); \
  if (KeyState.Left == 0) { \
    KeyState.Left = 1<<1; \
    INT(INT_JOYPAD); \
  }

#define LeftIsReleased(); \
  KeyState.Left = 0;

#define RightIsPressed(); \
  if (KeyState.Right == 0) { \
    KeyState.Right = 1; \
    INT(INT_JOYPAD); \
  }

#define RightIsReleased(); \
  KeyState.Right = 0;

#define StartIsPressed(); \
  if (KeyState.Start == 0) { \
    KeyState.Start = 1<<3; \
    INT(INT_JOYPAD); \
  }

#define StartIsReleased(); \
  KeyState.Start = 0;

#define SelectIsPressed(); \
  if (KeyState.Select == 0) { \
    KeyState.Select = 1<<2; \
    INT(INT_JOYPAD); \
  }

#define SelectIsReleased(); \
  KeyState.Select = 0;

#define ButtonAisPressed(); \
  if (KeyState.A == 0) { \
    KeyState.A = 1<<1; \
    INT(INT_JOYPAD); \
  }

#define ButtonAisReleased(); \
  KeyState.A = 0;

#define ButtonBisPressed(); \
  if (KeyState.B == 0) { \
    KeyState.B = 1; \
    INT(INT_JOYPAD); \
  }

#define ButtonBisReleased(); \
  KeyState.B = 0;

#define ButtonResetIsReleased(); \
  ResetEmulator();

#define ButtonSaveIsReleased(); \
  SaveGame();

#define ButtonLoadIsReleased(); \
  LoadGame();

//#define ButtonQuitIsReleased();

void riseA( void ){
  ButtonAisPressed();
}

void fallA( void ){
  ButtonAisReleased();
}

void riseB( void ){
  ButtonBisPressed();
}

void fallB( void ){
  ButtonBisReleased();
}

void riseC( void ){
  StartIsPressed();
}

void fallC( void ){
  StartIsReleased();
}

void riseU( void ){
  UpIsPressed();
}

void fallU( void ){
  UpIsReleased();
}

void riseD( void ){
  DownIsPressed();
}

void fallD( void ){
  DownIsReleased();
}

void riseL( void ){
  LeftIsPressed();
}

void fallL( void ){
  LeftIsReleased();
}

void riseR( void ){
  RightIsPressed();
}

void fallR( void ){
  RightIsReleased();
}

/*
#include "mbed.h"

#define BTN_A (1<<9)
#define BTN_B (1<<4)
#define BTN_C (1<<10)
#define BTN_UP (1<<13)
#define BTN_DOWN (1<<3)
#define BTN_LEFT (1<<25)
#define BTN_RIGHT (1<<7)

#define KEY_MASK ( BTN_A | BTN_B | BTN_C | BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT )

PortIn keysPort( PortA, KEY_MASK );

void readKeys( void ){
  u_int32_t last = 0xffffffff;
  register u_int32_t keys = keysPort.read();
}
*/

#define baseKey 0xA0000020
#define readKey(k) ( *(volatile u_int8_t*)( baseKey + k ) )

#define kDown readKey(3)
#define kB readKey(4)
#define kRight readKey(7)
#define kA readKey(9)
#define kC readKey(10)
#define kUp readKey(13)
#define kLeft readKey(25)
#define kStart kC

/*
#define BTN_UP      1
#define BTN_RIGHT   2
#define BTN_DOWN    3
#define BTN_LEFT    0
#define BTN_A       4
#define BTN_B       5
#define BTN_C       6

#define kDown readKey(BTN_DOWN)
#define kB readKey(BTN_B)
#define kRight readKey(BTN_RIGHT)
#define kA readKey(BTN_A)
#define kC readKey(BTN_C)
#define kUp readKey(BTN_UP)
#define kLeft readKey(BTN_LEFT)
#define kStart kC

extern u_int8_t * btnStates;

#define readKey(k) ( btnStates[k] )

extern void pollButtons(void);
*/

#define kDownMask 0x8
#define kUpMask 0x4
#define kLeftMask 0x2
#define kRightMask 0x1

#define kStartMask 0x8
#define kSelectMask 0x4
#define kAMask 0x2
#define kBMask 0x1

void slog( char * s );
char b[64];

void keyPadUpdate( uint32_t cycles ){
//  static uint8_t lastA = 0x3F, lastB = 0x3F;
  static u_int32_t cpt = 0;

  cpt += cycles;
  if( cpt < 80000 ) return;
  cpt = 0;

  //pollButtons();

  static u_int8_t lastA = 0x2F, lastB = 0x1F;
  //u_int8_t rr = *_IoRegisters;

  register u_int8_t r = ( *_IoRegisters ) & 0xF0;
  u_int8_t pressed = 0, last;

  switch( r ){
    case 0x20:
      last = lastA;

      //r = 0x30;

      if( !kDown )
        r |= kDownMask;
      else if( last & kDownMask ) pressed = 1;

      if( !kUp )
        r |= kUpMask;
      else if( last & kUpMask ) pressed = 1;

      if( !kLeft )
        r |= kLeftMask;
      else if( last & kLeftMask ) pressed = 1;

      if( !kRight )
        r |= kRightMask;
      else if( last & kRightMask ) pressed = 1;

      *_IoRegisters = r;

//      sprintf(b, "0x%02X 0x%02X 0x%02X 0x%02X",rr, r, lastA, lastB);
//      slog( b );

      if( r != last ) lastA = r;
      if( pressed ) INT(INT_JOYPAD);
    return;

    case 0x10:
      last = lastB;

      //r = 0x30;

      if( !kStart )
        r |= kStartMask;
      else if( last & kStartMask ) pressed = 1;

      r |= kSelectMask; // set select as not pressed

/*
      if( !kSelect )
        r |= kSelectMask;
      else if( last & kSelectMask ) pressed = 1;
*/

      if( !kA )
        r |= kAMask;
      else if( last & kAMask ) pressed = 1;

      if( !kB )
        r |= kBMask;
      else if( last & kBMask ) pressed = 1;
//if( ! r&kStartMask ){

//}

      *_IoRegisters = r;

      //sprintf(b, "0x%02X 0x%02X 0x%02X 0x%02X",rr, r, lastA, lastB);
      //slog( b );

//      sprintf(b, "0x%02X 0x%02X 0x%02X 0x%02X",rr, r, lastA, lastB); print( b );

      if( r != last ) lastB = r;
      if( pressed ) INT(INT_JOYPAD);
    return;

    default:
      *_IoRegisters = 0xF0;//r ^ 0xff;// | 0x0F;//0xF0;
      //sprintf(b, "0x%02X",*_IoRegisters);
      //slog( b );
    return;
  };
}

uint32_t JoyCheckCounter;
inline void CheckJoypad( uint32_t cycles, struct zboyparamstype *zboyparams) {
  int JoyNewReg;
  int event;

  JoyCheckCounter += cycles;

  if (JoyCheckCounter < 80000 ) {
    return;
  }

  JoyCheckCounter = 0;

  while ((event = drv_keypoll()) != DRV_INPUT_NONE) {
    switch (drv_event_gettype(event)) {
    case DRV_INPUT_KEYBOARD | DRV_INPUT_KEYDOWN:
    case DRV_INPUT_JOYSTICK | DRV_INPUT_JOYDOWN:
    case DRV_INPUT_JOYSTICK | DRV_INPUT_JOYAXDOWN:
      if (drv_event_getval(event) == zboyparams->key_start) {
	StartIsPressed();
      } else if (drv_event_getval(event) == zboyparams->key_select) {
	SelectIsPressed();
      } else if (drv_event_getval(event) == zboyparams->key_b) {
	ButtonBisPressed();
      } else if (drv_event_getval(event) == zboyparams->key_a) {
	ButtonAisPressed();
      } else if (drv_event_getval(event) == zboyparams->key_up) {
	UpIsPressed();
      } else if (drv_event_getval(event) == zboyparams->key_down) {
	DownIsPressed();
      } else if (drv_event_getval(event) == zboyparams->key_left) {
	LeftIsPressed();
      } else if (drv_event_getval(event) == zboyparams->key_right) {
	RightIsPressed();
      }
      break;
    case DRV_INPUT_KEYBOARD | DRV_INPUT_KEYUP:
    case DRV_INPUT_JOYSTICK | DRV_INPUT_JOYUP:
    case DRV_INPUT_JOYSTICK | DRV_INPUT_JOYAXUP:
      if (drv_event_getval(event) == zboyparams->key_start) {
	StartIsReleased();
      } else if (drv_event_getval(event) == zboyparams->key_select) {
	SelectIsReleased();
      } else if (drv_event_getval(event) == zboyparams->key_b) {
	ButtonBisReleased();
      } else if (drv_event_getval(event) == zboyparams->key_a) {
	ButtonAisReleased();
      } else if (drv_event_getval(event) == zboyparams->key_up) {
	UpIsReleased();
      } else if (drv_event_getval(event) == zboyparams->key_down) {
	DownIsReleased();
      } else if (drv_event_getval(event) == zboyparams->key_left) {
	LeftIsReleased();
      } else if (drv_event_getval(event) == zboyparams->key_right) {
	RightIsReleased();
      } else if (drv_event_getval(event) == zboyparams->key_save) {
	  ButtonSaveIsReleased();
      } else if (drv_event_getval(event) == zboyparams->key_load) {
	  ButtonLoadIsReleased();
      }
      break;
    }

  }

  JoyRegA = 0xF ^ (KeyState.Down | KeyState.Up | KeyState.Left | KeyState.Right);
  JoyRegB = 0xF ^ (KeyState.Start | KeyState.Select | KeyState.A | KeyState.B);
  JoypadWrite( IoRegisters[0xFF00] );
}
