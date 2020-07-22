/*
 * zBoy - A GameBoy emulator for Windows and Linux
 * Copyright (C) Mateusz Viste 2010, 2011, 2012, 2013, 2014, 2015
 * © 2020 Noferi Mickaël ~ r043v/dph ~ noferov@gmail
 * ----------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------
 */

/* include some standard C headers */
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() and exit(), random() and randomize() */
#include <string.h>
#include <time.h>       /* for time-related functions (clock, time...) */
#include <stdint.h>     /* for uint8_t type */
#include <signal.h>

#include "drv.h"

#define pVer "0.60"
#define pDate "2010,2011,2012,2013,2014,2015"

#include "zboystructs.h"

struct RomInformations RomInfos;

unsigned int GraphicWidth, GraphicHeight;
unsigned int UserMessageFramesLeft = 0;

#include "declares.h"         /* Some SUB/FUNCTION declarations */
#include "binary.h"           /* Provides basic support to binary notation (eg. bx01100111) */
#include "adjtiming.c"        /* AdjustTiming() */
#include "config.h"           /* zBoy_loadconfig() & zBoy_saveconfig() */
#include "numconv.c"          /* UbyteToByte() and DwordVal() */
#include "mmu.c"              /* Include Memory Management Unit emulation core */
#include "cpu-ints.c"         /* Interrupts routines */
#include "video.c"            /* Video emulation core */
#include "joypad.c"           /* Joypad interface emulation */
#include "cpu-timer.c"        /* Include timer emulation */
#include "cpu-divider.c"      /* Include divider emulation */
#ifdef DEBUGMODE
  #include "debug.c"          /* Includes some debugging routines */
#endif
#include "cpu-z80.c"          /* Z80 CPU emulation core */
#include "save.c"             /* SaveGame() and LoadGame() */
#include "reset.c"            /* ResetEmulator() */

#include "mbc0.c"
#include "mbc1.c"

uint8_t mapper = 0;

void checkMapper( void ){
  mapper = embedrom[0x147];
  if( mapper == 6 || mapper == 3 ) mapper = 1;
  if( mapper > 1 ) mapper = 0;

  writeHandlers = mapper ? writeHandlersmbc1 : writeHandlersmbc0 ;
}

void indexRAM( void ){
  mapper ? indexRAMmbc1() : indexRAMmbc0();
}

//void computePalColorTable(void);
void keyPadInit( void );
void keyPadUpdate( uint32_t );

void print(char *msg);

void zboymain(void){//int argc, char **argv) {
  struct zboyparamstype zboyparams;
  int UsedCycles;
  RomInfos.MemoryROM_PTR = embedrom;

  /* preload a default zBoy configuration */
  zboy_loaddefaultconfig(&zboyparams);

  /* (try to) load parameters from the zBoy configuration file */
  //zboy_loadconfig(&zboyparams);

  //zboyparams.GraphicScaleFactor = 1;    /* use 1x scaling */
  //zboyparams.scalingalgo = REFRESHSCREEN_NOSCALE; /* No scaling at all */

  /* check the configuration, and fix it if needed */
  //zboy_fixconfig(&zboyparams);

  GraphicWidth = 160;// * zboyparams.GraphicScaleFactor;
  GraphicHeight = 144;// * zboyparams.GraphicScaleFactor;

  //keyPadInit();

  ResetEmulator(); /* Fire up the Zilog80 monster, and init all required variables */

  /* starting emulation... */

  while( 1 ){
    UsedCycles = CpuExec();
    TotalCycles += UsedCycles;    /* Increment the global cycles counter */
    uTimer( UsedCycles );           /* Update uTimer */
    incDivider( UsedCycles );       /* Increment the DIV register */

    if( HaltState ){
      VideoSysUpdate( UsedCycles );   /* Update Video subsystem */
      CheckJoypad(UsedCycles , &zboyparams);  /* Update the Joypad register */
      //keyPadUpdate( UsedCycles );
      CheckInterrupts();
      UsedCycles = 0;
    }

    int partial = CpuExec();
    UsedCycles += partial;
    TotalCycles += partial;    /* Increment the global cycles counter */
    uTimer( partial );           /* Update uTimer */
    incDivider( partial );       /* Increment the DIV register */

    VideoSysUpdate( UsedCycles );   /* Update Video subsystem */

    CheckJoypad(UsedCycles , &zboyparams);  /* Update the Joypad register */
    //keyPadUpdate( UsedCycles );

/*
    if( IoRegisters[0xFF00] && ( IoRegisters[0xFF00] & 0x0F ) != 0x0F ){
      char b[64];
      sprintf(b, "0x%02X", IoRegisters[0xFF00] );
      print( b );
    }
*/

    if ( InterruptsState || HaltState )
      CheckInterrupts();

  }

//  zboy_saveconfig(&zboyparams);

  /* shutdown the I/O subsystem */
//  drv_close();

  //return(0);
}
