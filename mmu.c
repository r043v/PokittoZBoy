/*
   ----------------------------------------
    MMU emulation (Memory Management Unit)
    This file is part of the zBoy project.
    Copyright (C) Mateusz Viste 2010, 2011
   ----------------------------------------
*/

#define FORCE_INLINE __attribute__((always_inline)) inline

enum mbc1_models {
  MBC1_16_8 = 1,
  MBC1_4_32 = 2
};


uint8_t _MemoryInternalRAM[0x2000];    /* Internal RAM Area  [8KiB] */
uint8_t _MemoryInternalHiRAM[128]; /* Internal RAM (high area + IE register) */
uint8_t _MemoryBankedRAM[0x2000]; // 0x20A001];    /* Banked RAM [2MiB] */
uint8_t * const MemoryBankedRAM = _MemoryBankedRAM - 0xA000;


#ifndef ROM
#define ROM 2048
#endif

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define romDir roms

#define EMBEDROM STR(romDir/ROM.h)

#include EMBEDROM

//#define MemoryROM embedrom
//uint8_t * MemoryROM = (uint8_t*)embedrom;
//};
//#else
//const uint8_t MemoryROM[128*1024] = {0xDE, 0xAD, 0xBE, 0xEF, 0x1, 0};
//#endif

uint8_t _VideoRAM[0x2000];      /* Video RAM [8KiB] */
uint8_t _SpriteOAM[0xA0];     /* Sprite OAM memory */
uint8_t _IoRegisters[0x80];   /* All I/O memory-mapped registers */
uint8_t * const MemoryInternalRAM = _MemoryInternalRAM - 0xC000;    /* Internal RAM Area  [8KiB] */
uint8_t * const MemoryInternalHiRAM = _MemoryInternalHiRAM - 0xFF80;
uint8_t * const VideoRAM = _VideoRAM - 0x8000;      /* Video RAM [8KiB] */
uint8_t * const SpriteOAM = _SpriteOAM - 0xFE00;     /* Sprite OAM memory */
uint8_t * const IoRegisters = _IoRegisters - 0xFF00;   /* All I/O memory-mapped registers */

// uint8_t MemoryMAP[0x10000];    /* Regular memory (fallback for unmapped regions) */
int Mbc1Model = MBC1_16_8;    /* MBC1 memory model (MbcModel can be 1 or 2)  1=16/8 ; 2=4/32 */
int CurRomBank = 0;           /* Used for ROM bank switching (must be at least 9 bits long for MBC5 support!) */
int CurRamBank = 0;           /* Current RAM bank selection */
// int SaveScoresWriteProtection[2048];


// uint8_t (*MemoryReadSpecial)(register int memaddr);
// void (*MemoryWriteSpecial)(register int memaddr, uint8_t DataByte);

// #define MemoryWriteSpecial MBC0_MemoryWrite
// #define MemoryReadSpecial MBC0_MemoryRead


/* Below are generic MemoryRead and MemoryWrite routines. These routines  *
 * check if the called address is a well-known address. If this address   *
 * is behaving the same on all known MBC controllers, then it is answered *
 * here (and it is FAST). Otherwise, a Memory routine specialized for the *
 * given MBC is called.                                                   */

uint8_t * const ramidx = (uint8_t *) 0x20000000;
uint8_t * RAMette[9] = {
  embedrom,//MemoryROM,
  _VideoRAM - 0x8000,
  _MemoryInternalRAM - 0xC000,
  _MemoryInternalRAM - 0xC000 - 8192,
  _SpriteOAM - 0xFE00,
  _IoRegisters - 0xFF00,
  _MemoryInternalHiRAM - 0xFF80,
  _MemoryBankedRAM - 0xA000,
  embedrom//MemoryROM
};


typedef void (*WriteHandlerT)( uint32_t, uint8_t d, uint8_t * );
//extern const WriteHandlerT writeHandlers[];
extern WriteHandlerT * writeHandlers;

void NULLWrite( uint32_t addr, uint8_t data, uint8_t *bank ){}

#define getMemoryBlock(a) (RAMette[ ramidx[ (a) >> 5 ] ])

/*inline uint8_t *getMemoryBlock( int ReadAddr ){
  return RAMette[ ramidx[ReadAddr>>5] ];
}*/

#define MemoryRead(a) (getMemoryBlock(a)[ (a) ])

/*inline uint8_t MemoryRead(int ReadAddr) {
  return RAMette[ ramidx[ReadAddr>>5] ][ ReadAddr ];
}*/

//#define MemoryWrite(a,d) {int i=ramidx[ a >> 5];writeHandlers[ i ]( a, d, RAMette[i] );}

extern uint8_t mapper;

FORCE_INLINE void MemoryWrite(uint32_t WriteAddr, uint8_t DataHolder) {

  int id = ramidx[ WriteAddr >> 5 ];

  if( !id || id == 8 ){
    if( !mapper ) return;
    MBC1Write( WriteAddr, DataHolder, RAMette[id] );
    return;
  }

  if( id == 5 ){
    IOWrite( WriteAddr, DataHolder, RAMette[id] );
    return;
  }

  RAMette[id][WriteAddr] = DataHolder;
/*
  switch( ramidx[ WriteAddr >> 5 ] ){
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
    case 7:
      RAMette[id][WriteAddr] = DataHolder;
    return;
    case 5:
      IOWrite( WriteAddr, DataHolder, RAMette[id] );
    return;
    default:
      writeHandlers[ id ]( WriteAddr, DataHolder, RAMette[id] );
    return;
  };
*/
}

//#define RAMWrite(a,d,b) b[a]=d;

void RAMWrite( uint32_t WriteAddr, uint8_t DataHolder, uint8_t *buffer ){
    buffer[ WriteAddr ] = DataHolder;
}


uint8_t JoyRegA = 0, JoyRegB = 0, JoyOldReg;
// uint8_t *PCBuffer;

//uint8_t MemoryRead( int );

FORCE_INLINE void IOWrite(uint32_t WriteAddr, uint8_t DataHolder, uint8_t *IoRegisters){

  if (WriteAddr == 0xFF41) {                            /* STAT register: Do not allow to write into 2 last bits of the STAT */
    IoRegisters[0xFF41] = ((IoRegisters[0xFF41] & bx00000011) | (DataHolder & bx11111100)); /* register, as these bits are the mode flag. */
  } else if (WriteAddr == 0xFF44) { /* CURLINE [RW] Current Scanline. */
    IoRegisters[WriteAddr] = 0;     /* Writing into this register resets it. */
    /* SetUserMsg("LY RESET"); */
  } else if (WriteAddr == 0xFF46) {   /* Starts LCD OAM DMA transfer */
      int x;
      register int v = DataHolder << 8;
      //(RAMette[ ramidx[ (a) >> 5 ] ][ (a) ])
      SpriteOAM[ 0xFE00 ] = MemoryRead( v );
      for( x = 1; x < 160; x++) { /* Let's copy XX00-XX9F to FE00-FE9F */
	SpriteOAM[0xFE00 | x] = MemoryRead( v | x);
      }
  } else if (WriteAddr == 0xFF04) {
    IoRegisters[0xFF04] = 0;      /* Divide register: Writing any value sets it to 0 */
  } else if (WriteAddr == 0xFF00) {
    JoypadWrite( DataHolder );
  } else { // if ((WriteAddr >= 0xFF00) && (WriteAddr <= 0xFF4B)) {   /* I/O registers */
    IoRegisters[WriteAddr] = DataHolder;
    //} else if (WriteAddr <= 65535) {
    // MemoryWriteSpecial(WriteAddr, DataHolder);
  }

}

void JoypadWrite(uint8_t JoyNewReg){
  if( (JoyNewReg & bx00100000) != 0) {
    JoyNewReg &= 0xF0;
    /* P14 selected (bit 4 is low) -> down/up/left/right */
    JoyNewReg |= JoyRegA;
  } else if ( (JoyNewReg & bx00010000) != 0) {
    JoyNewReg &= 0xF0;
    /* P15 selected (bit 5 is low) -> Start/Select/B/A */
    JoyNewReg |= JoyRegB;
  } else if ( JoyNewReg == 3){
    /* no bits 4 & 5 set, maybe the game wants to get the system type... */
      JoyNewReg = 0xF0; /* returns FXh to indicate a classic GameBoy device */
  }

  IoRegisters[0xFF00] = JoyNewReg;   /* update the joypad register [FF00h] */
}
