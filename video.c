/* ----------------------------------------
   Video emulation core of a GameBoy device
   This file is part of the zBoy project
   Copyright (C) Mateusz Viste 2010, 2011
   ----------------------------------------

   FF40 (LCDC register):
   Bit7 0=Display Off  1=Display On
   Bit6 0=TileMap at 9800-9BFF   1=TileMap at 9C00-9FFF
   Bit5 0=WindowDisplay disabled  1=WindowDisplay enabled
   Bit4 0=BG&Window tile data at 8800-97FF   1=BG&Window tile data at 8000-8FFF
   Bit3 0=BgTile Map display at 9800-9BFF   1=BgTitle Map display at 9C00-9FFF
   Bit2 0=SpriteSize=8x8    1=SpriteSize=8x16
   Bit1 0=Do not display Sprites   1=Display sprites
   Bit0 0=Do not display background   1=Display background

   Generating an interrupt means "enabling the corresponding bit of the IF register (FF0F)"
*/

//#include "qsort.c"  /* provides the tableQsort() sub, used to sort sprites in DrawSprites */

unsigned int HideBackgroundDisplay = 0;
unsigned int HideSpritesDisplay = 0;
unsigned int HideWindowDisplay = 0;

uint8_t AutoScreenshot = 0;

unsigned int CountFPS, VideoClkCounterMode, VideoClkCounterVBlank;
unsigned int fpslimitGenFrameTrigger = 0;
// uint8_t setPixel(160][144];
// uint8_t ScreenOldBuffer[160][144];  /* used as a buffer to detect changes on screen */
uint8_t CurLY, LastLYdraw;   /* used by VideoSysUpdate */
//uint32_t ScreenPalette32[256] = {0xFFFFFF,   /* Palette of shades to use onscreen */
//                                 0xAAAAAA,
//                                 0x555555,
//                                 0x000000};

//#include "video_out.c"        /* Video output (& scaling) routines */
/* void (*RefreshScreen)(); */ /* this is a pointer to a function, it needs to be set to the appropriate function later. */


#define pal_BGP 0xFF47
#define pal_OBP0 0xFF48
#define pal_OBP1 0xFF49

__attribute__((always_inline)) uint8_t GetGbPalette(int PalAddr, uint8_t ColIdx) {
  /*  Returns the RGB value of a color index from one of the palettes */
  /*   ColIdx: 0..3 */
  /*   PalType: 0=BGP  1=OBP0  2=OBP1 */
  if( !ColIdx ) return IoRegisters[PalAddr] & 3;

  static const int colidxshift[] = { 0, 4, 2, 6 };

  return (IoRegisters[PalAddr] >> colidxshift[ColIdx]) & 3;
}

uint8_t framebuffer[160*4];

const u_int32_t palettes[] =
{0x73ae0,0x4e7a0,0x25460,0x8518,0x3fff0,0x330b0,0x15650,0x4718,0x3bfa8,0x26b78,0x15438,0x7fff8,0x7db68,0x59558,0x14510,0x7fff8,0xce08,0x1d620,0x53428,0x7bc68,0xce08,0x1d620,0x53428,0x7bc68,0xce08,0x1d620,0x53428,0x7bc68,0xce08,0x1d620,0x53428,0x7bc68,0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x3cc00,0x7b0a0,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x44e38,0x7e080,0x7fff8,0x0,0x6c000,0x7e080,0x7fff8,0x0,0x2000,0x3bf28,0x7fff8,0x0,0xf0,0x32890,0x7fff8,0x2cc08,0x41a20,0x66680,0x7f9b8,0x0,0x35428,0x52048,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x3cc00,0x7ea60,0x7fff8,0x0,0x52080,0x29560,0x7fd90,0x0,0x6c000,0x7e080,0x7ff00,0x0,0x2000,0x6e068,0x7fff8,0x0,0x14a40,0x2e2a8,0x7fff8,0x7fff8,0x7e930,0x12500,0x21830,0x0,0x6c000,0x7e080,0x7fff8,0x7ff00,0x3cc10,0x3bf28,0x7fff8,0x0,0x75b08,0x7fff8,0x7fff8,0x625c8,0x21440,0x4c980,0x4310,0x625c8,0x18c88,0x4c980,0x4310,0x0,0x10820,0xa668,0x7ebd0,0x0,0x1c6b0,0x74000,0x7fff8,0x0,0x2800,0x7c000,0x7f610,0x7df00,0x47680,0x10920,0x0,0x54ff8,0x0,0x7ff78,0x7df00,0x2bf08,0x75b08,0x7fff8,0x7fff8,0x1408,0x7cc00,0x7ff70,0x1edf0,0x0,0x7ff00,0x5d800,0x7fff8,0x5c000,0x7c000,0x7b7d0,0x7fff8,0x0,0x75b08,0x7fff8,0x7fff8,0x20e18,0x31628,0x56760,0x736a0,0x0,0x6c000,0x7e080,0x7fff8,0x0,0x2000,0x56760,0x7fff8,0x0,0x78700,0x7fc00,0x7fff8,0x10038,0x49c00,0x7e540,0x7f8b0,0x0,0x1cd0,0xe6f8,0x7fdc0,0x10038,0x2c0b0,0x514f8,0x7fff8,0x1480,0x11fb8,0x370f8,0x7fff8,0x0,0x5a248,0x73270,0x5bae8,0x0,0x6c000,0x7e080,0x7fff8,0x70718,0x110b8,0x77690,0x7fff8,0x1da00,0x63918,0x7bf98,0x7fff8};

u_int32_t scaling = 1;

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

#define setPixel(x,y,c) framebuffer[x]=c
#define getPixel(x,y) framebuffer[x]

inline void DrawBackground( uint32_t CurScanline ) {
  unsigned int TilesDataAddr, BgTilesMapAddr, TileNum, TileToDisplay, TileTempAddress, LastDisplayedTile;
  uint8_t x, y, z, t, u, PixelX, UbyteBuff1, UbyteBuff2;
  static uint8_t TileBuffer[64];

  if (((IoRegisters[0xFF40] & 1) == 0) || (HideBackgroundDisplay != 0)) { /* if "BackgroundEnabled" bit is not set, or bg has been forced OFF by user, then do not draw background (fill with black instead) */
    //for (x = 0; x < 160; x++) setPixel(x, 0, 3);  /* black */
    u_int32_t *p = (u_int32_t*)framebuffer;
    //static u_int32_t *end = &p[40];
    do {
      *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
      *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;
    } while( p != (u_int32_t*)&framebuffer[160] );

    //memset( framebuffer,0,160 ); // white
    return;
  }
//  else
//  {  /* If "BackgroundEnabled" bit is set then draw background */
    /* Get starting address of tiles data */
    { register u_int32_t tmp = IoRegisters[0xFF40];
      TilesDataAddr  = tmp & 16 ? 0x8000 : 0x8800;
      BgTilesMapAddr = tmp &  8 ? 0x9C00 : 0x9800;
/*      if (( tmp & 16 ) == 0) {
        TilesDataAddr = 0x8800;
      } else {
        TilesDataAddr = 0x8000;
      }
      if ((IoRegisters[0xFF40] & 8) == 0) {
        BgTilesMapAddr = 0x9800;
      } else {
        BgTilesMapAddr = 0x9C00;
      }
*/
    }
//TilesDataAddr = TilesDataAddresses[(IoRegisters[0xFF40] >> 4) & 1];
//BgTilesMapAddr = BgTileMapAddresses[(IoRegisters[0xFF40] >> 3) & 1];

    y = CurScanline + IoRegisters[0xFF42]; // SCY?
    z = (y & 7);   /* Same than z = y MOD 8 (but MUCH faster)    --> this is the tile's row that has to be computed */
    u = (z << 3);  /* << 3 = *8 */
    y >>= 3;  /* Same than y = y/8 (but >> is faster)            --> this is the number of the tile to display */

      /*FOR y = 0 TO 31 */
        LastDisplayedTile = 999; /* a valid tile number is 0..255 - I am forcing the 1st tile to get generated */
        TileNum = (IoRegisters[0xFF43] >> 3);  /* 0xFF43 is the SCX register */

	BgTilesMapAddr += (y << 5);

	PixelX = -(IoRegisters[0xFF43] & 7); // ((startX << 3) - IoRegisters[0xFF43]);  /* 0xFF43 is the SCX register */
	x = PixelX==0;
        for (; x <= 20; x++) {
          if (TilesDataAddr == 0x8000) {
              TileToDisplay = VideoRAM[BgTilesMapAddr + TileNum];
            } else {
              TileToDisplay = UbyteToByte(VideoRAM[BgTilesMapAddr + TileNum]) + 128;
          }
          if (TileToDisplay != LastDisplayedTile) { /* Compute the tile only if it's different than the previous one (otherwise just */
            LastDisplayedTile = TileToDisplay;      /* reuse the same data) - I named this a "micro tile cache" in the changelog. */

            t = (z << 1);    /*   Get only the tile's y line related to current scanline */
            TileTempAddress = TilesDataAddr + (TileToDisplay << 4) + t;
            t <<= 2;
            /*FOR t = 0 TO 15 STEP 2 */
              UbyteBuff1 = VideoRAM[TileTempAddress];
              UbyteBuff2 = VideoRAM[TileTempAddress + 1];
              TileBuffer[t + 7] =
		GetGbPalette(
			     pal_BGP,
			     ((UbyteBuff2 & 1) | ((UbyteBuff1 & 1) << 1))
			      );
	      TileBuffer[t + 6] =
		GetGbPalette(
			     pal_BGP,
			     (((UbyteBuff2>>1)&1) | (((UbyteBuff1 & 2) >> 1) << 1))
			      );
	      TileBuffer[t + 5] =
		GetGbPalette(
			     pal_BGP,
			     (((UbyteBuff2>>2)&1) | (((UbyteBuff1 & 4) >> 2) << 1))
			      );
	      TileBuffer[t + 4] =
		GetGbPalette(
			     pal_BGP,
			     (((UbyteBuff2>>3)&1) | (((UbyteBuff1 & 8) >> 3) << 1))
			      );
	      TileBuffer[t + 3] =
		GetGbPalette(
			     pal_BGP,
			     (((UbyteBuff2>>4)&1) | (((UbyteBuff1 & 16) >> 4) << 1))
			      );
	      TileBuffer[t + 2] =
		GetGbPalette(
			     pal_BGP,
			     (((UbyteBuff2>>5)&1) | (((UbyteBuff1 & 32) >> 5) << 1))
			      );
	      TileBuffer[t + 1] =
		GetGbPalette(
			     pal_BGP,
			     (((UbyteBuff2>>6)&1) | (((UbyteBuff1 & 64) >> 6) << 1))
			      );
	      TileBuffer[t + 0] =
		GetGbPalette(
			     pal_BGP,
			     (((UbyteBuff2>>7)&1) | (((UbyteBuff1 & 128) >> 7) << 1))
			      );
            /*NEXT t */
          }

	  t = u;

//  u_int8_t *p = &framebuffer[PixelX], *tile = &TileBuffer[u];

	  switch( 160 - PixelX ){
	  default:
	  case 8:  setPixel( PixelX++, 0, TileBuffer[u++] );
	  case 7:  setPixel( PixelX++, 0, TileBuffer[u++] );
	  case 6:  setPixel( PixelX++, 0, TileBuffer[u++] );
	  case 5:  setPixel( PixelX++, 0, TileBuffer[u++] );
	  case 4:  setPixel( PixelX++, 0, TileBuffer[u++] );
	  case 3:  setPixel( PixelX++, 0, TileBuffer[u++] );
	  case 2:  setPixel( PixelX++, 0, TileBuffer[u++] );
	  case 1:  setPixel( PixelX++, 0, TileBuffer[u] );
	  case 0:
	  break;
	  };

/*  u_int8_t *p = &framebuffer[PixelX], *tile = &TileBuffer[u]; */

	  u = t;
          // TileNum = ((TileNum+1) & 0x1F) | (TileNum & ~0x1F);
	  TileNum = (TileNum+1) & 0x1F;
        } /* for(x) */
      /*NEXT y */
//  }
}


inline void DrawWindow( uint32_t CurScanline ) {
  /*
   Some infos about the window...

   WindowPosX = 7    [FF4Bh]  (should be >= 0 and <= 166)
   WindowPosY = 0    [FF4Ah]  (should be >= 0 and <= 143)

   Window tiles can be located at 8000h-8FFFh or 8800h-97FFh (just like for the background)
   At 8000h-8FFFh tiles are numbered from 0 to 255, while at 8800h-97FFh tiles are ranging from -128 to 127.

   The LCDC register is at FF40h.
   Bit 6 - Window tile map address (0: 9800h-9BFFh, 1: 9C00h-9FFFh)
   Bit 5 - Window Display (0: off, 1: on)
   Bit 4 - Window & background tile data address (0: 8800h-97FFh, 1: 8000h-8FFFh)
   Bit 0 - Window & background display (0: off, 1: on)

   The palette for both background and window is located at FF47h (BGP)
  */

if( CurScanline > 143
|| HideWindowDisplay
|| ( ( IoRegisters[0xFF40] & bx00100001 ) != bx00100001 )
|| IoRegisters[0xFF4B] > 166
|| IoRegisters[0xFF4A] > CurScanline
|| IoRegisters[0xFF4B] < 7
) return;

  static signed int x, y, z, t, UbyteBuff1, UbyteBuff2, pixrow;
  static signed int TilesMapAddress, TilesDataAddress, TileNum, TileToDisplay, PixelX, TileTempAddress;
  static uint8_t TileBufferWin[64];
/*  if (((IoRegisters[0xFF40] & bx00000001) != 0) && (HideWindowDisplay == 0)) {
    if ((IoRegisters[0xFF40] & bx00100000) != 0) {
      if ((IoRegisters[0xFF4B] <= 166) && (IoRegisters[0xFF4A] <= CurScanline) && (IoRegisters[0xFF4B] >= 7) && (CurScanline <= 143)) {
*/
        if ((IoRegisters[0xFF40] & 64) == 0) {
            TilesMapAddress = 0x9800;
          } else {
            TilesMapAddress = 0x9C00; /* Get tile map address */
        }
        if ((IoRegisters[0xFF40] & bx00010000) == 0) {
            TilesDataAddress = 0x8800;
          } else {
            TilesDataAddress = 0x8000; /* Get tile data address */
        }

        pixrow = (CurScanline - IoRegisters[0xFF4A]);
        y = (pixrow >> 3);  /* don't forget about adding WY! (SHR 3 is the same than / 8)  --> this is the y tile row to display */
        pixrow = (pixrow & bx00000111);  /* MOD 8                --> this is the tile's row of pixels to display */
        TileNum = (y << 5);     /* I'm doing "y << 5" instead of "y * 32" for higher speed */
        for (x = 0; x < 32; x++) {
          if (TilesDataAddress == 0x8000) {
              TileToDisplay = VideoRAM[TilesMapAddress + TileNum];
            } else {
              TileToDisplay = UbyteToByte(VideoRAM[TilesMapAddress + TileNum]) + 128;
          }
          z = (pixrow << 1);   /*      Get only the tile's y line related to current scanline */
          TileTempAddress = TilesDataAddress + (TileToDisplay << 4) + z;  /* << 4 is the same than *16 (just much faster) */
          z <<= 2;
          /*FOR z = 0 TO 15 STEP 2 */
            UbyteBuff1 = VideoRAM[TileTempAddress];     /* << 4 is the same than *16 (just much faster) */
            UbyteBuff2 = VideoRAM[TileTempAddress + 1]; /* << 4 is the same than *16 (just much faster) */
            TileBufferWin[z + 7] =  GetGbPalette(pal_BGP,
						 (UbyteBuff2&1) | ((UbyteBuff1 & 1) << 1));      /* Note: I am using "SHL 2" here instead of "*4" (faster!) */
            TileBufferWin[z + 6] = GetGbPalette(pal_BGP,
						((UbyteBuff2>>1)&1) | (((UbyteBuff1>>1)&1) << 1));
	    TileBufferWin[z + 5] = GetGbPalette(pal_BGP,
						((UbyteBuff2>>2)&1) | (((UbyteBuff1>>2)&1) << 1));
	    TileBufferWin[z + 4] = GetGbPalette(pal_BGP,
						((UbyteBuff2>>3)&1) | (((UbyteBuff1>>3)&1) << 1));
	    TileBufferWin[z + 3] = GetGbPalette(pal_BGP,
						((UbyteBuff2>>4)&1) | (((UbyteBuff1>>4)&1) << 1));
	    TileBufferWin[z + 2] = GetGbPalette(pal_BGP,
						((UbyteBuff2>>5)&1) | (((UbyteBuff1>>5)&1) << 1));
	    TileBufferWin[z + 1] = GetGbPalette(pal_BGP,
						((UbyteBuff2>>6)&1) | (((UbyteBuff1>>6)&1) << 1));
	    TileBufferWin[z] =     GetGbPalette(pal_BGP,
						((UbyteBuff2>>7)&1) | (((UbyteBuff1>>7)&1) << 1));
          /*NEXT z */
          PixelX = ((x << 3) + IoRegisters[0xFF4B] - 7);  /* 0xFF4B is the WX register (starts at 7!)   / (x << 3) is the same than (x * 8), but faster */
          z = (((CurScanline - IoRegisters[0xFF4A]) & 7) << 3);   /* "AND bx0111" is used instead of "MOD 8" and "SHL 3" instead of "* 8" */
          for (t = 0; t < 8; t++) {
            if ((PixelX >= 0) && (PixelX < 160)) {
              /*ScreenBuffer(PixelX, CurScanline) = GbPalette(TileBufferWin(z), 0) */
              setPixel(PixelX,0, TileBufferWin[z]+12); // | 32; /* | 32 is for marking it as 'window' (for possible colorization) */
            }
            PixelX++;  /* add 1 instead of relying on t value (faster) */
            z++;
          }
          TileNum++;   /* I'm adding 1 instead of relying on x value (faster) */
        }
/*      }
    }
  }*/
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

//extern void sqsort( int*f, int*l );
//extern void asmqsort( int*f, int n );

void DrawSprites( int32_t CurScanline ) {

  if( ( IoRegisters[0xFF40] & bx00000010 == 0 ) || HideSpritesDisplay ) return;

  int PatternNum, SpriteFlags, UbyteBuff1, UbyteBuff2, SpritePalette, x, y, z, t, xx;
  int SpritePosX, SpritePosY;
  u_int32_t SpriteMaxY;
  u_int8_t NumberOfSpritesToDisplay = 0;
  static uint8_t SpriteBuff[128];
  static int ListOfSpritesToDisplay[40] = { 0 };

    //SpriteMaxY = IoRegisters[0xFF40] & bx00000100 ? 15 : 7;

    SpriteMaxY = IoRegisters[0xFF40] & bx00000100 ? 15 : 7;

/*    if ((IoRegisters[0xFF40] & bx00000100) == 0) {
        SpriteMaxY = 7;
      } else {
        SpriteMaxY = 15;
    }*/

    union sprite *sprite = sprites;
    u_int32_t * display = (u_int32_t*)ListOfSpritesToDisplay;
    u_int32_t scanline = CurScanline + 16;

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
/*
    for (x = 0; x < 40; x++) {
      SpritePosY = SpriteOAM[0xFE00 + (x << 2)] - 16;     // sprite's ypos on screen
      SpritePosX = SpriteOAM[0xFE00 + (x << 2) + 1] - 8;  // sprite's xpos on screen

      if ((SpritePosX > -8) && (SpritePosY >= (CurScanline - SpriteMaxY)) && (SpritePosX < 160) && (SpritePosY <= CurScanline)) {
        ListOfSpritesToDisplay[NumberOfSpritesToDisplay] = ((SpritePosX + 8) << 8) | x;
	// compute an ID that will allow to sort entries easily
        NumberOfSpritesToDisplay += 1;
      }
    }
*/
    // Sort the list of sprites to display
    if( NumberOfSpritesToDisplay > 1 )
      cqsort( ListOfSpritesToDisplay, &ListOfSpritesToDisplay[NumberOfSpritesToDisplay - 1] );
//      tableQsort(ListOfSpritesToDisplay, NumberOfSpritesToDisplay - 1);

    // And here we are going to display each sprite (in the right order, of course)
    for (xx = 0; xx < NumberOfSpritesToDisplay; xx++) {
          x = ListOfSpritesToDisplay[xx] & bx00111111;  /* Retrieve 6 least significant bits (these are the actual ID of the sprite) */

          u_int8_t * p = &SpriteOAM[ 0xFE00 + (x << 2) ];

          SpritePosY = *p++ - 16;//SpriteOAM[0xFE00 + (x << 2)] - 16;     // sprite's ypos on screen
          SpritePosX = *p++ - 8;//SpriteOAM[0xFE00 + (x << 2) + 1] - 8;  // sprite's xpos on screen
          PatternNum = *p++;//SpriteOAM[0xFE00 + (x << 2) + 2];      // pattern num
          if (SpriteMaxY == 15) PatternNum &= bx11111110;     // the LSB is ignored in 8x16 sprite mode

          SpriteFlags = *p;//SpriteOAM[0xFE00 + (x << 2) + 3];  // Flags

          SpritePalette = SpriteFlags & bx00010000 ? 2 : 1;

/*          if ((SpriteFlags & bx00010000) == 0) {
	    SpritePalette = 1; // use OBJ0 palette
	  } else {
	    SpritePalette = 2; // use OBJ1 palette
          }
*/

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


	  // Bit 5 = X flip (horizontal mirror)
/*          if( SpriteFlags & bx00100000 ){
            s = SpriteBuff;
            register u_int8_t tmp, *s2, *send = &s[ (SpriteMaxY+1)*8 ];
            while( s != send ){
                s2 = s + 7;
                tmp = *s;
                *s++ = *s2;
                *s2-- = tmp;
                tmp = *s;
                *s++ = *s2;
                *s2-- = tmp;
                tmp = *s;
                *s++ = *s2;
                *s2-- = tmp;
                tmp = *s;
                *s = *s2;
                *s2 = tmp;
                s += 5;
            }
          }
*/
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

/*            for (z = 0; z < 8; z++) {
              if (SpriteMaxY == 7) {
		// Swap 8 pixels high sprites
		for (y = 0; y < 25; y += 8) {
		  t = SpriteBuff[y + z];
		  SpriteBuff[y + z] = SpriteBuff[(56 - y) + z];
		  SpriteBuff[(56 - y) + z] = t;
		}
	      } else {   // Swap 16 pixels high sprites
		for (y = 0; y < 57; y += 8) {
		  t = SpriteBuff[y + z];
		  SpriteBuff[y + z] = SpriteBuff[(120 - y) + z];
		  SpriteBuff[(120 - y) + z] = t;
		}
  }
}*/
          }

          // Now apply the sprite onscreen...
          UbyteBuff1 = GetGbPalette(pal_BGP, 0);  // Get background color into UbyteBuff1
          /*for (z = 0; z <= SpriteMaxY; z++) { */

      	  z = (CurScanline - SpritePosY); /* Apply to screen only one line of the sprite (no need to do more) */
      	  /*if ((z < 0) || (z > SpriteMaxY)) printf("Damn, z is sick! z = %d (CurScanline = %d ; SpritePosY = %d)\n", z, CurScanline, SpritePosY); */
      	  for (t = 0; t < 8; t++) {
      	    /*  Update all pixels, but not 0 (which are transparent for a sprite) */
      	    if (((SpritePosX + t) >= 0) && ((SpritePosY + z) >= 0) && ((SpritePosX + t) < 160) && ((SpritePosY + z) < 144)) { /* don't try to write outside the screen */
      	      if (SpriteBuff[(z << 3) | t] > 0) {  /* color 0 is transparent on sprites */
            		/* If bit 7 of the sprite's flags is set, then the sprite is "hidden" (prevails only over color 0 of background) */
            		if (((SpriteFlags & bx10000000) == 0) || getPixel(SpritePosX + t, 0) == UbyteBuff1) { /* Sprite's priority over background */
            		  /*ScreenBuffer(SpritePosX + t, SpritePosY + z) = GbPalette(SpriteBuff((z << 3) + t), SpritePalette) */
            		  if (SpritePalette == 1) { /* OBJ0 */
            		    setPixel(SpritePosX + t, 0, GetGbPalette(pal_OBP0, SpriteBuff[(z << 3) | t])+4);// | 64; /* | 64 is for marking it as 'OBJ0' (for possible later colorization */
            		  } else { /* OBJ1 */
            		    setPixel(SpritePosX + t, 0, GetGbPalette(pal_OBP1, SpriteBuff[(z << 3) | t])+8);// | 128; /* | 128 is for marking it as 'OBJ1' (for possible later colorization */
            		  }
            		}
      	      }
      	    }
      	  }
          /*} */
    }
}


void TurnLcdOff(void) {
  uint8_t x, y;
  for (y = 0; y < 144; y++) {
    for (x = 0; x < 160; x++) setPixel(x,y,0);
  }
}


void InitScreenOldBuffer(void) {    /* Call this one time, at the emulator * /
  uint8_t x, y;                 /* start, to force the refresh of the  * /
  for (y = 0; y < 144; y++) {   /* whole first LCD frame.              * /
    for (x = 0; x < 160; x++) {
      ScreenOldBuffer[x][y] = 255; /* fill with an unlikely value * /
    }
  }
				    */
}



#define GetLcdMode() (IoRegisters[0xFF41] & bx00000011)

static inline void SetLcdMode(uint8_t x) {
  IoRegisters[0xFF41] &= bx11111100;
  IoRegisters[0xFF41] |= x;   /* Here I set the LCD mode */
}


/* Parsing all the screen to detect changes that occured since last screen refresh */
#define DetectScreenChanges(); \
  x1 = 0;		       \
  y1 = 0;		       \
  y2 = 143;		       \
  x2 = 160;

/*
						\
  x1 = 255;\
  y1 = 255;\
  x2 = 0;\
  for (y = 0; y < 144; y++) {\
    for (x = 0; x < 160; x++) {\
      if (setPixel(x][y] != ScreenOldBuffer[x][y]) {\
        ScreenOldBuffer[x][y] = setPixel(x][y];\
        if (y1 > y) y1 = y;\
        y2 = y;\
        if (x1 > x) x1 = x;\
        if (x2 < x) x2 = x;\
      }\
    }\
  }
*/

#define DIV456( A ) (A) * (0x1000000 / 456) >> 24

void write_data_16(uint16_t data);
void write_command_16(uint16_t data);

void SetScanline(uint32_t s){
  volatile uint32_t *SET = (uint32_t *) 0xA0002200;

  write_command_16(0x20);

  if( scaling ){
    write_data_16(s+((s+3)>>2)-4);
    write_command_16(0x21);
    write_data_16(10);
  } else {
    write_data_16(16+s);
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

static inline void VideoSysUpdate(int cycles, struct zboyparamstype *zboyparams) {
  // static int x1, x2, y1, y2, x, y;
  //static unsigned int LastFullframeRenderingTime = 0;
  //static uint32_t OneSecondPollingTimer = 0;
  static uint32_t frameskip = 0;
  uint32_t checkInput = 0;

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

  if ((IoRegisters[0xFF40] & bx10000000) != 0) {    /* If LCD is ON... */
    if (IoRegisters[0xFF44] == IoRegisters[0xFF45]) {    /* Compare LY with LYC, and if equal, and */
        if ((IoRegisters[0xFF41] & bx00000100) == 0) {   /* coincidence bit not set (yet), then    */
          IoRegisters[0xFF41] |= bx00000100;             /* set the coincidence (2) bit of the     */
          if ((IoRegisters[0xFF41] & bx01000000) != 0) { /* STAT register (FF41) and (if enabled   */
            INT(INT_LCDC);                               /* via bit 6 of STAT register) trigger    */
          }
        }
      } else {
        IoRegisters[0xFF41] &= bx11111011;   /* reset the coincidence flag */
    }
  }


  if (VideoClkCounterVBlank >= 65664) {    /* We are in a VBLANK period! */
      /* Here I trigger the vblank interrupt and set mode 1 */
      if (GetLcdMode() != bx00000001) {   /* Check if not already in mode 1 */
        SetLcdMode(1);   /* Here I set LCD mode 1 */
        INT(INT_VBLANK);   /* Ask for the VBLANK interrupt */

	if( !frameskip ){
	  frameskip = 2;
	  checkInput = 1;
	}else
	  frameskip--;

        if ((IoRegisters[0xFF40] & bx10000000) == 0) {   /* The LCD is off, make it all black */
          TurnLcdOff();
          /*UserMessage = "LCD IS OFF" */
        }
        // if (zboyparams->NoSpeedLimit == 0) AdjustTiming(zboyparams); /* Slow down CPU emulation before drawing graphic frame */

      }
    } else {   /* Outside the VBlank, perform the MODE 0-2-3 loop */
      VideoClkCounterMode += cycles;
      if (VideoClkCounterMode >= 456) {
        VideoClkCounterMode -= 456;
      }
      if (VideoClkCounterMode <= 80) { /* mode 2 */
          if (GetLcdMode() != 2) {    /*  Check if not already in mode 2 */
            SetLcdMode(2);   /* Here I set LCD mode 2 */
            if ((IoRegisters[0xFF41] & bx00100000) != 0) {   /* If OAM int is enabled, request LCDC int */
              INT(INT_LCDC);
            }
          }
      } else if (VideoClkCounterMode <= (172 + 80)) {  /* mode 3 */
          if (GetLcdMode() != 3) {    /* Check if not already in mode 3 */
            SetLcdMode(3);   /* Here I set LCD mode 3 (no int here) */
          }
      } else {     /* mode 0 (H-blank) */
          if (GetLcdMode() != 0) {   /* Check if not already in mode 0 */
            if ((IoRegisters[0xFF40] & bx10000000) > 0) {   /* If LCD is ON... */
              if (LastLYdraw != CurLY) {                    /* And curline hasn't been drawn yet... */

    if( !frameskip ){
      DrawBackground(CurLY);                      /* Generate current scanline */
      DrawWindow(CurLY);
      DrawSprites(CurLY);

      if( LastLYdraw != CurLY -1 )
        SetScanline(CurLY);

      if( !scaling ){
  		  FlushScanline();
      } else if( CurLY>2 && CurLY < 155 ){
        FlushScanline_scale();
        if( !(CurLY&3) )
  		    FlushScanline_scale();
      }
    }

                LastLYdraw = CurLY;

              }
            }
            /* Trigger the hblank interrupt if enabled via bit 3 of the stat register (FF41) (via LCDC int?) */
            SetLcdMode(0);   /* Here I set LCD mode 0 */
            if ((IoRegisters[0xFF41] & bx00001000) != 0) {   /* If hblank int is enabled, request LCDC int */
              INT(INT_LCDC);
            }
          }
      }
  }


}
