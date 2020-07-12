/* ----------------------------------------
   Video emulation core of a GameBoy device
   This file is part of the zBoy project
   Copyright (C) Mateusz Viste 2010, 2011
   © 2020 noferi mickaël ~ r043v/dph ~ noferov@gmail.com
   ----------------------------------------
*/

#define FORCE_INLINE __attribute__((always_inline)) inline

unsigned int VideoClkCounterMode, VideoClkCounterVBlank;
uint8_t CurLY, LastLYdraw;   /* used by VideoSysUpdate */

#define lcdControlRegister (IoRegisters[0xFF40])

#define pal_BGP 0xFF47
#define pal_OBP0 0xFF48
#define pal_OBP1 0xFF49

/*
u_int32_t palColorTable[ 256 ] = { 0 };

void computePalColorTable( void ){
  u_int8_t i, *p = (u_int8_t*)palColorTable;
  for( i = 0; i <= 255; i++ ){
    *p++ = i & 3;
    *p++ = (i >> 4) & 3;
    *p++ = (i >> 2) & 3;
    *p++ = (i >> 6) & 3;
  }
}
*/

/*
#define getBgPalette( i ) palColorTable[ i ]
#define getPalette0( i ) pal0ColorTable[ i ]
#define getPalette1( i ) pal1ColorTable[ i ]
*/

uint8_t framebuffer[160*4];

const u_int32_t palettes[] = {
  0x73ae0,0x4e7a0,0x25460,0x8518,0x3fff0,0x330b0,0x15650,0x4718,0x3bfa8,0x26b78,0x15438,0x7fff8,0x7db68,0x59558,0x14510,0x7fff8,
  0xce08,0x1d620,0x53428,0x7bc68,0xce08,0x1d620,0x53428,0x7bc68,0xce08,0x1d620,0x53428,0x7bc68,0xce08,0x1d620,0x53428,0x7bc68,
  0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x3cc00,0x7b0a0,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,
  0x0,0x44e38,0x7e080,0x7fff8,0x0,0x6c000,0x7e080,0x7fff8,0x0,0x2000,0x3bf28,0x7fff8,0x0,0xf0,0x32890,0x7fff8,
  0x2cc08,0x41a20,0x66680,0x7f9b8,0x0,0x35428,0x52048,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,
  0x0,0x52080,0x29560,0x7fd90,0x0,0x6c000,0x7e080,0x7ff00,0x0,0x2000,0x6e068,0x7fff8,0x0,0x14a40,0x2e2a8,0x7fff8,
  0x7fff8,0x7e930,0x12500,0x21830,0x0,0x6c000,0x7e080,0x7fff8,0x7ff00,0x3cc10,0x3bf28,0x7fff8,0x0,0x75b08,0x7fff8,0x7fff8,
  0x625c8,0x21440,0x4c980,0x4310,0x625c8,0x18c88,0x4c980,0x4310,0x0,0x10820,0xa668,0x7ebd0,0x0,0x1c6b0,0x74000,0x7fff8,
  0x0,0x2800,0x7c000,0x7f610,0x7df00,0x47680,0x10920,0x0,0x54ff8,0x0,0x7ff78,0x7df00,0x2bf08,0x75b08,0x7fff8,0x7fff8,
  0x1408,0x7cc00,0x7ff70,0x1edf0,0x0,0x7ff00,0x5d800,0x7fff8,0x5c000,0x7c000,0x7b7d0,0x7fff8,0x0,0x75b08,0x7fff8,0x7fff8,
  0x20e18,0x31628,0x56760,0x736a0,0x0,0x6c000,0x7e080,0x7fff8,0x0,0x2000,0x56760,0x7fff8,0x0,0x78700,0x7fc00,0x7fff8,
  0x10038,0x49c00,0x7e540,0x7f8b0,0x0,0x1cd0,0xe6f8,0x7fdc0,0x10038,0x2c0b0,0x514f8,0x7fff8,0x1480,0x11fb8,0x370f8,0x7fff8,
  0x0,0x5a248,0x73270,0x5bae8,0x0,0x6c000,0x7e080,0x7fff8,0x70718,0x110b8,0x77690,0x7fff8,0x1da00,0x63918,0x7bf98,0x7fff8
};

u_int8_t scaling = 1;

void screenInit( void );

void flipScaling( void ){
  scaling ^= 1;
  screenInit();
}

u_int32_t * palette = palettes;
u_int8_t currentPalette = 0;

void flipPalette( void ){
  if( ++currentPalette == 13 ) currentPalette = 0;
  palette = &palettes[ 16*currentPalette ];
}

#define setPixel(x,y,c) framebuffer[x]=(c)
#define getPixel(x,y) framebuffer[x]

u_int8_t bgColorTable[4] = { 0,0,0,0 }, bgCurrentPal = 0;
u_int8_t spriteColorTable[2][4] = { { 0,0,0,0 }, { 0,0,0,0 } }, spriteCurrentPal[2] = { 0, 0 };

uint8_t tileTempBuffer[64];

/*FORCE_INLINE*/ void DrawBackground( void ){
  u_int32_t TilesDataAddr, BgTilesMapAddr, LastDisplayedTile = 0xffffffff;
  uint8_t y, z ;

  //u_int8_t * bgColorTable = (u_int8_t*)&palColorTable[ IoRegisters[ pal_BGP ] ];

  TilesDataAddr = lcdControlRegister & 16 ? 0x8000 : 0x8800;

  y = CurLY + IoRegisters[ 0xFF42 ]; // scanline + scroll y
  z = ( y & 7 ) << 1 ; // tile offset ( 2B tile )
  u_int8_t * tileBfPtrOffset = tileTempBuffer + ( z << 2 ) ; // out tile offset ( 2B -> 8B tile )

  u_int8_t TileNum = IoRegisters[0xFF43] >> 3;  /* 0xFF43 is the SCX register */

  BgTilesMapAddr = ( lcdControlRegister & 8 ? 0x9C00 : 0x9800 ) + ( ( y >> 3 ) << 5 ) ;

  register u_int8_t * frameBfPtr = framebuffer - ( IoRegisters[0xFF43] & 7 ) ; // - scrolling % 8

  u_int8_t *tilePtr = &VideoRAM[ BgTilesMapAddr + TileNum ], *lastTile = &tilePtr[ 32 - TileNum ];
  u_int8_t *tileDataPtr = &VideoRAM[ TilesDataAddr + z ]; // add line offset

  while( 1 ){
    u_int8_t TileToDisplay = TilesDataAddr == 0x8000 ? *tilePtr : UbyteToByte( *tilePtr ) + 128 ;

    if( ++tilePtr == lastTile ) tilePtr -= 32;

    register u_int8_t * tileBfPtr = tileBfPtrOffset ;

    if( TileToDisplay != LastDisplayedTile ){
      LastDisplayedTile = TileToDisplay;

      u_int8_t * tileDataTempPtr = tileDataPtr + ( TileToDisplay << 4 );

      register u_int32_t
        b1 = (*tileDataTempPtr++) << 1,
        b2 = *tileDataTempPtr ;

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx100000000 )
      | ( b2 &  bx10000000 ) ) >> 7
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx10000000 )
      | ( b2 & bx01000000 ) ) >> 6
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx01000000 )
      | ( b2 & bx00100000 ) ) >> 5
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00100000 )
      | ( b2 & bx00010000 ) ) >> 4
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00010000 )
      | ( b2 & bx00001000 ) ) >> 3
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00001000 )
      | ( b2 & bx00000100 ) ) >> 2
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00000100 )
      | ( b2 & bx00000010 ) ) >> 1
      ];

      *tileBfPtr = bgColorTable[
        ( b1 & bx00000010 )
      | ( b2 & bx00000001 )
      ];

      tileBfPtr -= 7;
    }

    // crop right
    if( frameBfPtr > &framebuffer[ 151 ] ){
      while( frameBfPtr != &framebuffer[160] ) *frameBfPtr++ = *tileBfPtr++;
      return;
    }

    u_int8_t * frameBfPtrEnd = &frameBfPtr[ 8 ];

    // crop left
    if( frameBfPtr < framebuffer ){
      register u_int8_t n = framebuffer - frameBfPtr;
      frameBfPtr += n; tileBfPtr += n;
    }

    while( frameBfPtr != frameBfPtrEnd ) *frameBfPtr++ = *tileBfPtr++;
  }
}

/*FORCE_INLINE*/ void DrawWindow(void){
  if( CurLY > 143 // out of screen bottom
  || IoRegisters[0xFF4B] > 166 // out of screen right
  || IoRegisters[0xFF4B] < 7 // out of screen left
  || IoRegisters[0xFF4A] > CurLY // show after current scanline
  ) return;

//  u_int8_t * bgColorTable = (u_int8_t*)&palColorTable[ IoRegisters[ pal_BGP ] ];

  u_int32_t LastDisplayedTile = 0xffffffff;
  u_int32_t TilesDataAddr = lcdControlRegister & 16 ? 0x8000 : 0x8800 ;

  u_int8_t pixrow = CurLY - IoRegisters[ 0xFF4A ] ; // window line to draw
  u_int8_t TileNum = ( pixrow >> 3 ) << 5 ; /* tile data index */
  u_int8_t z = ( pixrow & bx00000111 ) << 1; // offset y in gb tile (2B)

  u_int32_t TilesMapAddr = ( lcdControlRegister & 64 ? 0x9C00 : 0x9800 ) ;

  u_int8_t * tileBfPtrOffset = tileTempBuffer + ( z << 2 ) ; // out tile offset ( 2B -> 8B tile )

  register u_int8_t * frameBfPtr = &framebuffer[ IoRegisters[ 0xFF4B ] - 7 ];
  u_int8_t *tilePtr = &VideoRAM[ TilesMapAddr + TileNum ] ;
  u_int8_t *tileDataPtr = &VideoRAM[ TilesDataAddr + z ];

  while(1){
    u_int8_t TileToDisplay = TilesDataAddr == 0x8000 ? *tilePtr : UbyteToByte( *tilePtr ) + 128 ;
    tilePtr += 1;

    register u_int8_t * tileBfPtr = tileBfPtrOffset ;

    if( TileToDisplay != LastDisplayedTile ){
      LastDisplayedTile = TileToDisplay;

      u_int8_t * tileDataTempPtr = tileDataPtr + ( TileToDisplay << 4 );

      register u_int32_t b1 = ( *tileDataTempPtr++ ) << 1 ;
      register u_int32_t b2 = *tileDataTempPtr ;

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx100000000 )
      | ( b2 &  bx10000000 ) ) >> 7
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx10000000 )
      | ( b2 & bx01000000 ) ) >> 6
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx01000000 )
      | ( b2 & bx00100000 ) ) >> 5
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00100000 )
      | ( b2 & bx00010000 ) ) >> 4
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00010000 )
      | ( b2 & bx00001000 ) ) >> 3
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00001000 )
      | ( b2 & bx00000100 ) ) >> 2
      ];

      *tileBfPtr++ = bgColorTable[
      ( ( b1 & bx00000100 )
      | ( b2 & bx00000010 ) ) >> 1
      ];

      *tileBfPtr = bgColorTable[
        ( b1 & bx00000010 )
      | ( b2 & bx00000001 )
      ];

      tileBfPtr -= 7;
    }

    // crop right
    if( frameBfPtr > &framebuffer[ 151 ] ){
      while( frameBfPtr != &framebuffer[160] ) *frameBfPtr++ = *tileBfPtr++;
      return;
    }

    register u_int8_t * frameBfPtrEnd = &frameBfPtr[ 8 ] ;
    while( frameBfPtr != frameBfPtrEnd ) *frameBfPtr++ = *tileBfPtr++;
  }
}

#define u32 u_int32_t
#define u8 u_int8_t

#pragma pack(push, 1)

union sprite {
  u32 raw;
  struct {
    u8 y;
    u8 x;
    u8 data;
    union {
      u8 flag;
      struct {
        u8 z:1;
        u8 yflip:1;
        u8 xflip:1;
        u8 palette:1;
        u8:4;
      };
    }
  };
};

#pragma pack(pop)

union sprite * sprites = &SpriteOAM[0xFE00];
union sprite * spritesEnd = &SpriteOAM[0xFE00 + 40*4];//sprites + 40;

void DrawSprites( void ){// int32_t CurScanline ) {


  int PatternNum, SpriteFlags, UbyteBuff2, x, y, z, t, xx;
  int SpritePosX, SpritePosY;
  u_int32_t SpriteMaxY;
  u_int8_t NumberOfSpritesToDisplay = 0;
  static uint8_t SpriteBuff[128];
  static int ListOfSpritesToDisplay[40] = { 0 };

    SpriteMaxY = IoRegisters[0xFF40] & bx00000100 ? 15 : 7;

    union sprite *sprite = sprites;
    u_int32_t * display = (u_int32_t*)ListOfSpritesToDisplay;
    u_int32_t scanline = CurLY + 16;

    while( sprite != spritesEnd ){
      if( !sprite->x || sprite->x >= 168 // on screen x
       || !sprite->y //|| sprite->y >= 160 // on screen y
       || sprite->y > scanline // on scanline
       || sprite->y + SpriteMaxY < scanline
      ){ sprite++; continue; } // no!

      // visible at this scanline
      *display++ = ( sprite->x << 6 ) | ( sprite - sprites ) ;
      sprite++;
    };

    NumberOfSpritesToDisplay = display - (u_int32_t*)ListOfSpritesToDisplay;
    if( !NumberOfSpritesToDisplay ) return;

    // Sort the list of sprites to display
    if( NumberOfSpritesToDisplay > 1 )
      cqsort( ListOfSpritesToDisplay, &ListOfSpritesToDisplay[NumberOfSpritesToDisplay - 1] );

    // And here we are going to display each sprite (in the right order, of course)
    for (xx = 0; xx < NumberOfSpritesToDisplay; xx++) {
          x = ListOfSpritesToDisplay[xx] & bx00111111;  /* Retrieve 6 least significant bits (these are the actual ID of the sprite) */

          u_int8_t * p = &SpriteOAM[ 0xFE00 + (x << 2) ];

          SpritePosY = *p++ - 16;//SpriteOAM[0xFE00 + (x << 2)] - 16;     // sprite's ypos on screen
          SpritePosX = *p++ - 8;//SpriteOAM[0xFE00 + (x << 2) + 1] - 8;  // sprite's xpos on screen
          PatternNum = *p++;//SpriteOAM[0xFE00 + (x << 2) + 2];      // pattern num
          if (SpriteMaxY == 15) PatternNum &= bx11111110;     // the LSB is ignored in 8x16 sprite mode

          SpriteFlags = *p;//SpriteOAM[0xFE00 + (x << 2) + 3];  // Flags

          u_int8_t SpritePalette = SpriteFlags & bx00010000 ? 1 : 0;

          p = &VideoRAM[ 0x8000 + ( PatternNum << 4 ) ];
          register u_int8_t * s = SpriteBuff;
          u_int8_t * pend = &p[ (SpriteMaxY<<1) + 2 ];

          if( SpriteFlags & bx00100000 ){ // xflip
            while( p != pend ){
              register u_int8_t b1 = *p++;
              register u_int8_t b2 = *p++;
              *s++ = (((b2 & bx00000001)) | (((b1 & bx00000001)) << 1));
              *s++ = (((b2 & bx00000010) >> 1) | (((b1 & bx00000010) >> 1) << 1));
              *s++ = (((b2 & bx00000100) >> 2) | (((b1 & bx00000100) >> 2) << 1));
              *s++ = (((b2 & bx00001000) >> 3) | (((b1 & bx00001000) >> 3) << 1));
              *s++ = (((b2 & bx00010000) >> 4) | (((b1 & bx00010000) >> 4) << 1));
              *s++ = (((b2 & bx00100000) >> 5) | (((b1 & bx00100000) >> 5) << 1));
              *s++ = (((b2 & bx01000000) >> 6) | (((b1 & bx01000000) >> 6) << 1));
              *s++ = (((b2 & bx10000000) >> 7) | (((b1 & bx10000000) >> 7) << 1));
            }
          } else { // normal
            while( p != pend ){
              register u_int8_t b1 = *p++;
              register u_int8_t b2 = *p++;

              *s++ = (((b2 & bx10000000) >> 7) | (((b1 & bx10000000) >> 7) << 1));
              *s++ = (((b2 & bx01000000) >> 6) | (((b1 & bx01000000) >> 6) << 1));
              *s++ = (((b2 & bx00100000) >> 5) | (((b1 & bx00100000) >> 5) << 1));
              *s++ = (((b2 & bx00010000) >> 4) | (((b1 & bx00010000) >> 4) << 1));
              *s++ = (((b2 & bx00001000) >> 3) | (((b1 & bx00001000) >> 3) << 1));
              *s++ = (((b2 & bx00000100) >> 2) | (((b1 & bx00000100) >> 2) << 1));
              *s++ = (((b2 & bx00000010) >> 1) | (((b1 & bx00000010) >> 1) << 1));
              *s++ = (((b2 & bx00000001)) | (((b1 & bx00000001)) << 1));
            }
          }

          if( SpriteFlags & bx01000000 ) {
	    // Bit 6 = Y flip (vertical mirror)
            register u_int32_t tmp, *l1 = (u_int32_t*)SpriteBuff, *l2 = &l1[ SpriteMaxY<<1 ];
            //u_int32_t lend = &l1[ SpriteMaxY + 1 ];
            while( 1 ){// l1 != lend ){
              // 4 lines for unroll a bit
              // 0
              tmp = *l1;
              *l1++ = *l2;
              *l2++ = tmp;
              tmp = *l1;
              *l1++ = *l2;
              *l2 = tmp;
              l2 -= 3;

              // 1
              tmp = *l1;
              *l1++ = *l2;
              *l2++ = tmp;

              tmp = *l1;
              *l1++ = *l2;
              *l2 = tmp;
              l2 -= 3;

              // 2
              tmp = *l1;
              *l1++ = *l2;
              *l2++ = tmp;

              tmp = *l1;
              *l1++ = *l2;
              *l2 = tmp;
              l2 -= 3;

              // 3
              tmp = *l1;
              *l1++ = *l2;
              *l2++ = tmp;

              tmp = *l1;
              *l1++ = *l2;
              *l2-- = tmp;

              if( l1 == l2 ) break;
              l2 -= 2;
            };

          }

          // Now apply the sprite onscreen...

          u_int8_t *spritePal = spriteColorTable[ SpritePalette ],
          *spriteCurrentPalette = &spriteCurrentPal[SpritePalette];
          u_int8_t *spritePalIndex = &IoRegisters[SpritePalette + pal_OBP0];// ? pal_OBP1 : pal_OBP0;
          if( *spriteCurrentPalette != *spritePalIndex ){
            u_int8_t colorMore[2] = { 4, 8 }, more = colorMore[SpritePalette];
            *spriteCurrentPalette = *spritePalIndex;
            spritePal[0] = ( (*spriteCurrentPalette) & 3 ) + more;
            spritePal[1] = ( ((*spriteCurrentPalette) >> 4) & 3 ) + more;
            spritePal[2] = ( ((*spriteCurrentPalette) >> 2) & 3 ) + more;
            spritePal[3] = ( ((*spriteCurrentPalette) >> 6) & 3 ) + more;
          }

      	  z = (CurLY - SpritePosY); /* Apply to screen only one line of the sprite (no need to do more) */
      	  for (t = 0; t < 8; t++){
      	    /*  Update all pixels, but not 0 (which are transparent for a sprite) */
      	    if (((SpritePosX + t) >= 0) && ((SpritePosY + z) >= 0) && ((SpritePosX + t) < 160) && ((SpritePosY + z) < 144)) { /* don't try to write outside the screen */
      	      if( SpriteBuff[(z << 3) | t] ){  /* color 0 is transparent on sprites */
            		/* If bit 7 of the sprite's flags is set, then the sprite is "hidden" (prevails only over color 0 of background) */
            		if (((SpriteFlags & bx10000000) == 0) || getPixel(SpritePosX + t, 0) == *bgColorTable/*UbyteBuff1*/) { /* Sprite's priority over background */
            		  /*ScreenBuffer(SpritePosX + t, SpritePosY + z) = GbPalette(SpriteBuff((z << 3) + t), SpritePalette) */
/*            		  if (SpritePalette == 0) {
            		    setPixel(SpritePosX + t, 0, GetGbPalette(pal_OBP0, SpriteBuff[(z << 3) | t])+4);
            		  } else {
            		    setPixel(SpritePosX + t, 0, GetGbPalette(pal_OBP1, SpriteBuff[(z << 3) | t])+8);
            		  }
*/
                  setPixel( SpritePosX + t, 0, spritePal[ SpriteBuff[ (z << 3) | t ] ] );
            		}
      	      }
      	    }
      	  }
          /*} */
    }
}

#define bgDisabledColor 0

/*FORCE_INLINE*/ void clearFramebuffer( void ){
   u_int32_t *p = (u_int32_t*)framebuffer;
  do {
    *p++ = bgDisabledColor; *p++ = bgDisabledColor;
    *p++ = bgDisabledColor; *p++ = bgDisabledColor;
    *p++ = bgDisabledColor; *p++ = bgDisabledColor;
    *p++ = bgDisabledColor; *p++ = bgDisabledColor;
  } while( p != (u_int32_t*)&framebuffer[160] );
}

#define TurnLcdOff clearFramebuffer

#define GetLcdMode() (IoRegisters[0xFF41] & bx00000011)
//#define SetLcdMode(x) ( IoRegisters[0xFF41] &= (bx11111100 | x) )

FORCE_INLINE void SetLcdMode(uint8_t x) {
  IoRegisters[0xFF41] &= bx11111100;
  IoRegisters[0xFF41] |= x;
}


#define DIV456( A ) ( (A) * (0x1000000 / 456) ) >> 24

void write_data_16(uint16_t data);
void write_command_16(uint16_t data);

void SetScanline( void ){//uint32_t s){
  volatile uint32_t *SET = (uint32_t *) 0xA0002200;

  write_command_16(0x20);

  if( scaling ){
    write_data_16(CurLY+((CurLY+3)>>2)-4);
    write_command_16(0x21);
    write_data_16(10);
  } else {
    write_data_16(16+CurLY);
    write_command_16(0x21);
    write_data_16(30);
  }

  write_command_16(0x22);

  // CLR_CS_SET_CD_RD_WR;
  SET[0] = 1 << 2;
  SET[1] = 1 << 24;
  SET[1] = 1 << 12;
}

//#ifdef SCALING


#define FLUSH_QUAD_SCALE					\
  " ldm %[pixelptr]!, {%[qd]}             \n"		\
  " uxtb %[pixel], %[qd]                  \n"		\
  " lsls %[pixel], %[pixel], 2            \n"		\
  " ldr %[pixel], [%[palette], %[pixel]]  \n"	       \
  " str %[pixel], [%[LCD], 0]             \n"	       \
  " movs %[pixel], 252                    \n"	       \
  " str %[WR], [%[LCD], %[pixel]]         \n"	       \
  " lsrs %[qd], %[qd], 8                  \n"	       \
  " str %[WR], [%[LCD], 124]              \n"	       \
						       \
  " uxtb %[pixel], %[qd]                  \n"	       \
  " lsls %[pixel], %[pixel], 2            \n"	       \
  " ldr %[pixel], [%[palette], %[pixel]]  \n"	       \
  " str %[pixel], [%[LCD], 0]             \n"	       \
  " movs %[pixel], 252                    \n"	       \
  " str %[WR], [%[LCD], %[pixel]]         \n"	       \
  " lsrs %[qd], %[qd], 8                  \n"	       \
  " str %[WR], [%[LCD], 124]              \n"	       \
						       \
  " movs %[pixel], 252                    \n"	       \
  " str %[WR], [%[LCD], %[pixel]]         \n"	       \
  " movs %[pixel], 252                    \n"	       \
  " str %[WR], [%[LCD], 124]              \n"	       \
						       \
  " uxtb %[pixel], %[qd]                  \n"	 \
  " lsls %[pixel], %[pixel], 2            \n"	 \
  " ldr %[pixel], [%[palette], %[pixel]]  \n"	 \
  " str %[pixel], [%[LCD], 0]             \n"	 \
  " movs %[pixel], 252                    \n"	 \
  " str %[WR], [%[LCD], %[pixel]]         \n"	 \
  " lsrs %[qd], %[qd], 8                  \n"	 \
  " str %[WR], [%[LCD], 124]              \n"	 \
						 \
  " uxtb %[pixel], %[qd]                  \n"	 \
  " lsls %[pixel], %[pixel], 2            \n"	 \
  " ldr %[pixel], [%[palette], %[pixel]]  \n"	 \
  " str %[pixel], [%[LCD], 0]             \n"	 \
  " movs %[pixel], 252                    \n"	 \
  " str %[WR], [%[LCD], %[pixel]]         \n"	 \
  "subs %[x], 4                           \n"	 \
  " str %[WR], [%[LCD], 124]              \n"

//#else

#define FLUSH_QUAD					\
  " ldm %[pixelptr]!, {%[qd]}             \n"		\
  " uxtb %[pixel], %[qd]                  \n"		\
  " lsls %[pixel], %[pixel], 2            \n"		\
  " ldr %[pixel], [%[palette], %[pixel]]  \n"		\
  " str %[pixel], [%[LCD], 0]             \n"		\
  " movs %[pixel], 252                    \n"		\
  " str %[WR], [%[LCD], %[pixel]]         \n"		\
  " lsrs %[qd], %[qd], 8                  \n"		\
  " str %[WR], [%[LCD], 124]              \n"		\
							\
  " uxtb %[pixel], %[qd]                  \n"		\
  " lsls %[pixel], %[pixel], 2            \n"		\
  " ldr %[pixel], [%[palette], %[pixel]]  \n"		\
  " str %[pixel], [%[LCD], 0]             \n"		\
  " movs %[pixel], 252                    \n"		\
  " str %[WR], [%[LCD], %[pixel]]         \n"		\
  " lsrs %[qd], %[qd], 8                  \n"		\
  " str %[WR], [%[LCD], 124]              \n"		\
							\
  " uxtb %[pixel], %[qd]                  \n"		\
  " lsls %[pixel], %[pixel], 2            \n"		\
  " ldr %[pixel], [%[palette], %[pixel]]  \n"		\
  " str %[pixel], [%[LCD], 0]             \n"		\
  " movs %[pixel], 252                    \n"		\
  " str %[WR], [%[LCD], %[pixel]]         \n"		\
  " lsrs %[qd], %[qd], 8                  \n"		\
  " str %[WR], [%[LCD], 124]              \n"		\
							\
  " uxtb %[pixel], %[qd]                  \n"		\
  " lsls %[pixel], %[pixel], 2            \n"		\
  " ldr %[pixel], [%[palette], %[pixel]]  \n"		\
  " str %[pixel], [%[LCD], 0]             \n"		\
  " movs %[pixel], 252                    \n"		\
  " str %[WR], [%[LCD], %[pixel]]         \n"		\
  "subs %[x], 4                           \n"		\
  " str %[WR], [%[LCD], 124]              \n"

//#endif


void FlushScanline_scale(){
  uint32_t *LCD = (uint32_t *) 0xA0002188;
  volatile uint32_t *SET = (uint32_t *) 0xA0002284;
  volatile uint32_t *CLR = (uint32_t *) 0xA0002204;

  uint8_t *d = framebuffer;
  uint32_t x = 160, pixel, quad, WR = 1<<12, *pal=palette;
  /* */

  asm volatile(
      ".syntax unified                \n"

      "nextPixelLoop%=:                       \n"
      FLUSH_QUAD_SCALE
      FLUSH_QUAD_SCALE
      "bne nextPixelLoop%=                    \n"

      : // outputs
	[pixelptr]"+l"(d),
	[x]"+l"(x),
	[pixel]"=l"(pixel),
	[qd]"=l"(quad),
	[palette]"+l"(pal),
	[WR]"+l"(WR),
	[LCD]"+l"(LCD)

      : // inputs

      : // clobbers
	"cc"
      );
}

void FlushScanline(){
  uint32_t *LCD = (uint32_t *) 0xA0002188;
  volatile uint32_t *SET = (uint32_t *) 0xA0002284;
  volatile uint32_t *CLR = (uint32_t *) 0xA0002204;

  uint8_t *d = framebuffer;
  uint32_t x = 160, pixel, quad, WR = 1<<12, *pal=palette;
  /* */

  asm volatile(
      ".syntax unified                \n"

      "nextPixelLoop%=:                       \n"
      FLUSH_QUAD
      FLUSH_QUAD
      "bne nextPixelLoop%=                    \n"

      : // outputs
	[pixelptr]"+l"(d),
	[x]"+l"(x),
	[pixel]"=l"(pixel),
	[qd]"=l"(quad),
	[palette]"+l"(pal),
	[WR]"+l"(WR),
	[LCD]"+l"(LCD)

      : // inputs

      : // clobbers
	"cc"
      );
}

uint32_t frameCount;

/*FORCE_INLINE*/ void VideoSysUpdate(int cycles, struct zboyparamstype *zboyparams) {
  static u_int8_t frameskip = 0;

  VideoClkCounterVBlank += cycles;
  if (VideoClkCounterVBlank >= 70224) {
    frameCount++;
    CurLY = 0;
    VideoClkCounterVBlank -= 70224;
    VideoClkCounterMode = VideoClkCounterVBlank; /* Sync VideoClkCounterMode with VideoClkCounterVBlank */
  }

  /*  MemoryInternalHiRAM[0xFF44] is the LY register (current vertical scanline) */

  /* Not sure if I should increment LY when LCD is off... but if I don't, then Baseball do not start... */
  CurLY = DIV456(VideoClkCounterVBlank);       /* LY should be between 0..153 */
  IoRegisters[0xFF44] = CurLY;               /* Save new LY */

  if( IoRegisters[0xFF40] & 128 ) {    /* If LCD is ON... */
    if (IoRegisters[0xFF44] == IoRegisters[0xFF45]) {    /* Compare LY with LYC, and if equal, and */
      if( (IoRegisters[0xFF41] & bx00000100) == 0 ){   /* coincidence bit not set (yet), then    */
        IoRegisters[0xFF41] |= bx00000100;             /* set the coincidence (2) bit of the     */
        if( IoRegisters[0xFF41] & bx01000000 ) INT( INT_LCDC ); /* STAT register (FF41) and (if enabled via bit 6 of STAT register) trigger  */
      }
    } else {
      IoRegisters[0xFF41] &= bx11111011;   /* reset the coincidence flag */
    }
  }

  if( VideoClkCounterVBlank >= 65664 ){    /* We are in a VBLANK period! */
    /* Here I trigger the vblank interrupt and set mode 1 */
    if( GetLcdMode() != 1 ){   /* Check if not already in mode 1 */
      SetLcdMode(1);   /* Here I set LCD mode 1 */
      INT(INT_VBLANK);   /* Ask for the VBLANK interrupt */

    	if( !frameskip-- ) frameskip = 2;

      if( lcdControlRegister & 128 == 0 ) TurnLcdOff();
    }

    return;
  }

  /* Outside the VBlank, perform the MODE 0-2-3 loop */
  VideoClkCounterMode += cycles;
  if( VideoClkCounterMode >= 456 ) VideoClkCounterMode -= 456;

  if (VideoClkCounterMode <= 80) { /* mode 2 */
    if( GetLcdMode() != 2 ){    /*  Check if not already in mode 2 */
      SetLcdMode(2);   /* Here I set LCD mode 2 */
      if( IoRegisters[0xFF41] & bx00100000 ) INT( INT_LCDC );  /* If OAM int is enabled, request LCDC int */
    }
    return;
  }

  if( VideoClkCounterMode <= (172 + 80) ){  /* mode 3 */
    SetLcdMode(3);   /* Here I set LCD mode 3 (no int here) */
    return;
  }

  /* mode 0 (H-blank) */
  if( GetLcdMode() == 0 ) return; /* Check if not already in mode 0 */

  if( LastLYdraw != CurLY && lcdControlRegister & 128 ){ // lcd on && curline hasn't been drawn
    if( !frameskip ){

    // recompute bg pal if need
    if( bgCurrentPal != IoRegisters[pal_BGP] ){
      bgCurrentPal = IoRegisters[pal_BGP];
      bgColorTable[0] = bgCurrentPal & 3;
      bgColorTable[1] = (bgCurrentPal >> 4) & 3;
      bgColorTable[2] = (bgCurrentPal >> 2) & 3;
      bgColorTable[3] = (bgCurrentPal >> 6) & 3;
    }

      if( scaling ){
        if( CurLY > 2 && CurLY < 155 ){
          // bg enabled ?
          if( lcdControlRegister & 1 )
            DrawBackground();
          else {
            clearFramebuffer();
          }

          // window enabled ?
          if( lcdControlRegister & 32 ) DrawWindow();

          if( lcdControlRegister & 2 ) DrawSprites();

          if( LastLYdraw != CurLY - 1 ) SetScanline();

          FlushScanline_scale();

          if( !(CurLY&3) ) FlushScanline_scale();
        }
      } else { // no scale
        // bg enabled ?
        if( lcdControlRegister & 1 )
          DrawBackground();
        else {
          clearFramebuffer();
        }

        // window enabled ?
        if( lcdControlRegister & 32 ) DrawWindow();

        if( lcdControlRegister & 2 ) DrawSprites();

        if( LastLYdraw != CurLY - 1 ) SetScanline();

        FlushScanline();
      }
    }

    LastLYdraw = CurLY;

  }

  /* Trigger the hblank interrupt if enabled via bit 3 of the stat register (FF41) (via LCDC int?) */
  SetLcdMode(0);   /* Here I set LCD mode 0 */

  if( IoRegisters[0xFF41] & bx00001000 ) INT(INT_LCDC); /* If hblank int is enabled, request LCDC int */
}
