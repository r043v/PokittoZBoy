/*
   Set of microinstructions to be used by the Z80 emulation core
   This file is part of the zBoy project
   Copyright (C) Mateusz Viste 2010,2011,2012,2013
*/



/*
FORCE_INLINE void WriteRegAF(x)
  Register->F = (x AND bx11111111)
  Register->A = ((x >> 8) AND bx11111111)
}
*/

#define FORCE_INLINE __attribute__((always_inline)) inline

// ZNHC0000
FORCE_INLINE int16_t ReadRegAF(){
return (DwordVal(
		 Register->FZ |
		 Register->FN |
		 Register->FH |
		 Register->FC,
		 Register->A));
}

FORCE_INLINE int8_t ReadRegF(){
  return Register->FZ |
    Register->FN |
    Register->FH |
    Register->FC;

}

FORCE_INLINE void WriteRegF(uint8_t f){
  Register->FZ = f & ( 1 << 7 );
  Register->FN = f & ( 1 << 6 );
  Register->FH = f & ( 1 << 5 );
  Register->FC = f & ( 1 << 4 );
}

FORCE_INLINE void WriteRegBC(uint8_t x, uint8_t y) {
  Register->B = x;
  Register->C = y;
}
FORCE_INLINE int ReadRegBC(){ return (DwordVal(Register->C, Register->B)); }

FORCE_INLINE void WriteRegDE(uint8_t x, uint8_t y) {
  Register->D = x;
  Register->E = y;
}
FORCE_INLINE int ReadRegDE(){ return (DwordVal(Register->E, Register->D)); }

FORCE_INLINE void WriteRegHL(uint8_t x, uint8_t y) {
  Register->H = x;
  Register->L = y;
}
FORCE_INLINE int ReadRegHL(){ return (DwordVal(Register->L, Register->H)); }


#define SetFlagZ() Register->FZ=1<<7

#define ResetFlagZ() Register->FZ=0

#define GetFlagZ() Register->FZ

#define SetFlagN() Register->FN=1<<6

#define ResetFlagN() Register->FN=0

#define GetFlagN() Register->FN

#define SetFlagH() Register->FH=1<<5

#define ResetFlagH() Register->FH=0

#define GetFlagH() Register->FH

#define SetFlagC() Register->FC=1<<4

#define ResetFlagC() Register->FC=0

#define GetFlagC() Register->FC

FORCE_INLINE void PushToStack(uint8_t TmpRegister1, uint8_t TmpRegister2) {    /* Push a register pair to the stack */
  //Register->SP -= 1;  /* decrement SP to update the Stack Point address */
  Register->SPBlock = getMemoryBlock( --Register->SP );

  Register->SPBlock[ Register->SP-- ] = TmpRegister1;
  // MemoryWrite(Register->SP, TmpRegister1);        /* Write the byte */

  //Register->SP -= 1;  /* decrement SP to update the Stack Point address */
  // Register->SPBlock = getMemoryBlock( Register->SP );

  // MemoryWrite(Register->SP, TmpRegister2);        /* Write the byte */
  Register->SPBlock[ Register->SP ] = TmpRegister2;
}

FORCE_INLINE void PopFromStack(uint8_t *popreg1, uint8_t *popreg2) {    /* Pop a register pair from the stack */
  *popreg2 = Register->SPBlock[ Register->SP++ ]; // MemoryRead(Register->SP);
  //Register->SP++;
  // Register->SPBlock = getMemoryBlock( Register->SP );

  *popreg1 = Register->SPBlock[ Register->SP++ ]; // MemoryRead(Register->SP);
  //Register->SP++;
  Register->SPBlock = getMemoryBlock( Register->SP );
  // *popreg1 =  MemoryRead(Register->SP);
}

FORCE_INLINE void IncReg8(uint8_t *RegToInc) {
  ResetFlagN();
	*RegToInc += 1;
	if( *RegToInc == 0 ) SetFlagZ(); else ResetFlagZ();

/*
  if (*RegToInc == 255) {
      *RegToInc = 0;
      SetFlagZ();
    } else {
      *RegToInc += 1;
      ResetFlagZ();
  }
*/
  if ((*RegToInc & bx00001111) == 0) {
      SetFlagH();
    } else {
      ResetFlagH();
  }
}

FORCE_INLINE void DecReg8(uint8_t *RegToInc) { /* Decrements by 1 an 8bit register */
  uint8_t OldRegToInc;
  OldRegToInc = *RegToInc;
  SetFlagN();

	*RegToInc -= 1;
//	if( *RegToInc == 255 ) SetFlagZ(); else ResetFlagZ(); ????

  if ((OldRegToInc & bx00001111) < (*RegToInc & bx00001111)) {
      SetFlagH();
    } else {
      ResetFlagH();
  }
  if (*RegToInc == 0) { // ????
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
}


FORCE_INLINE void IncReg16(uint8_t *x, uint8_t *y) { /* Increment a 16 bit register (pair of 8bit registers) */
  if ((*x == 255) && (*y == 255)) {  /* if Reg = 65535 then Reg = 0 */
      *x = 0;
      *y = 0;
    } else {
      if (*y == 255) {
          *y = 0;
          *x += 1;
        } else {
          *y += 1;
      }
  }
}


FORCE_INLINE void DecReg16(uint8_t *x, uint8_t *y) { /* Decrement a 16 bit register (pair of 8bit registers) */
  if ((*x == 0) && (*y == 0)) {  /* if Reg = 0 then Reg = 65535 */
      *x = 255;
      *y = 255;
    } else {
      if (*y == 0) {
          *y = 255;
          *x -= 1;
        } else {
          *y -= 1;
      }
  }
}

FORCE_INLINE void SubValFromReg8(uint8_t *RegToSub, uint8_t *SubValue) {  /* Subtract a n ubyte value from a 8bit register */
  SetFlagN();
  if (*RegToSub == *SubValue) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  if ((*RegToSub & bx00001111) < (*SubValue & bx00001111)) {
      SetFlagH();
    } else {
      ResetFlagH();
  }
  if (*RegToSub < *SubValue) {
      *RegToSub = (256 - (*SubValue - *RegToSub));
      SetFlagC();
    } else {
      *RegToSub -= *SubValue;
      ResetFlagC();
  }
}


FORCE_INLINE void AddToA(uint8_t ValToAdd) {  /* Add ValToAdd to register A and sets some flags (8bit arithmetic) */
    if (((Register->A & bx00001111) + (ValToAdd & bx00001111)) > 15) {
        SetFlagH();
      } else {
        ResetFlagH();
    }
    if ((Register->A + ValToAdd) > 255) {
        SetFlagC();
      } else {
        ResetFlagC();
    }
    Register->A = ((Register->A + ValToAdd) & bx11111111);
    if (Register->A == 0) {
        SetFlagZ();
      } else {
        ResetFlagZ();
    }
    ResetFlagN();
}


FORCE_INLINE void AddToHL(unsigned int ValToAdd) {  /* Add ValToAdd to register HL and sets some flags (16bit Z80 arithmetic) */
    static unsigned int TempHLvalue;
    TempHLvalue = ReadRegHL();
    /* I'm not touching H flag, but really not sure what should I do...
       The Z80 manual says "Set if carry from bit 11", but NO$GMB doesn't set it :-/
       24 aug 2011 - okay, finally I'm touching it. Otherwise the "05-op rp.gb" test
       ROM fails. */
    if ((TempHLvalue & 0xFFF) + (ValToAdd & 0xFFF) > 4095) SetFlagH(); else ResetFlagH();
    if ((TempHLvalue + ValToAdd) > 65535) {
        SetFlagC();
      } else {
        ResetFlagC();
    }
    TempHLvalue = ((TempHLvalue + ValToAdd) & 0xFFFF);
    WriteRegHL((TempHLvalue >> 8), (TempHLvalue & bx11111111));   /* Write 16bit result into HL */
    ResetFlagN();
}


FORCE_INLINE void AddToSP(int8_t ByteToAdd) {  /* Add ByteToAdd signed byte to register SP and set some Z80 flags */
  static uint16_t ResultAddToSP;
  ResultAddToSP = (Register->SP + ByteToAdd);
  /* ResultAddToSP &= 0xFFFF; */ /* Make sure to be in the range 0..65535 - not needed if the variable is an unsigned 16bit wide integer */

  if ((ResultAddToSP & 0xFF) < (Register->SP & 0xFF)) {
      SetFlagC();
    } else {
      ResetFlagC();
  }
  if ((ResultAddToSP & 0xF) < (Register->SP & 0xF)) {
      SetFlagH();
    } else {
      ResetFlagH();
  }
  Register->SP = ResultAddToSP;
  Register->SPBlock = getMemoryBlock( ResultAddToSP );
  ResetFlagZ();
  ResetFlagN();
}


FORCE_INLINE void CmpA(uint8_t n) {
  if ((Register->A & 15) < (n & 15)) {
      SetFlagH();
    } else {
      ResetFlagH();
  }
  if (Register->A < n) {
      SetFlagC();
    } else {
      ResetFlagC();
  }
  if (Register->A == n) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  SetFlagN();
}


FORCE_INLINE void XorA(uint8_t xorvalz) {
  /* Xor A with xorvalz, saves results into A, and sets some Z80 flags. */
  ResetFlagN();
  ResetFlagH();
  ResetFlagC();
  Register->A = (Register->A ^ xorvalz);
  if (Register->A == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
}


FORCE_INLINE void AndA(uint8_t ValToAndWith) {
  Register->A = (Register->A & ValToAndWith);
  if (Register->A == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
  ResetFlagC();
}


FORCE_INLINE void OrA(uint8_t ValToOrWith) { /* A = A OR ValToOrWith (and sets some Z80 flags) */
  Register->A = (Register->A | ValToOrWith);
  if (Register->A == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  ResetFlagH();
  ResetFlagC();
}


FORCE_INLINE void SwapUbyte(uint8_t *ubytevalue) {
  /* Swaps upper & lower nibbles of ubytevalue and sets some Z80 flags */
  *ubytevalue = ((*ubytevalue & bx11110000) >> 4) | ((*ubytevalue & bx00001111) << 4);
  if (*ubytevalue == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  ResetFlagH();
  ResetFlagC();
}


FORCE_INLINE void TestBit0(uint8_t wartosc) {
  if ((wartosc & bx00000001) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void TestBit1(uint8_t wartosc) {
  if ((wartosc & bx00000010) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void TestBit2(uint8_t wartosc) {
  if ((wartosc & bx00000100) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void TestBit3(uint8_t wartosc) {
  if ((wartosc & bx00001000) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void TestBit4(uint8_t wartosc) {
  if ((wartosc & bx00010000) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void TestBit5(uint8_t wartosc) {
  if ((wartosc & bx00100000) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void TestBit6(uint8_t wartosc) {
  if ((wartosc & bx01000000) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void TestBit7(uint8_t wartosc) {
  if ((wartosc & bx10000000) == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  SetFlagH();
}

FORCE_INLINE void ResetBit0(uint8_t *wartosc) {  /* Reset bit 0 of wartosc */
  *wartosc &= bx11111110;
}

FORCE_INLINE void ResetBit1(uint8_t *wartosc) {  /* Reset bit 1 of wartosc */
  *wartosc &= bx11111101;
}

FORCE_INLINE void ResetBit2(uint8_t *wartosc) {  /* Reset bit 2 of wartosc */
  *wartosc &= bx11111011;
}

FORCE_INLINE void ResetBit3(uint8_t *wartosc) {  /* Reset bit 3 of wartosc */
  *wartosc &= bx11110111;
}

FORCE_INLINE void ResetBit4(uint8_t *wartosc) {  /* Reset bit 4 of wartosc */
  *wartosc &= bx11101111;
}

FORCE_INLINE void ResetBit5(uint8_t *wartosc) {  /* Reset bit 5 of wartosc */
  *wartosc &= bx11011111;
}

FORCE_INLINE void ResetBit6(uint8_t *wartosc) {  /* Reset bit 6 of wartosc */
  *wartosc &= bx10111111;
}

FORCE_INLINE void ResetBit7(uint8_t *wartosc) {  /* Reset bit 7 of wartosc */
  *wartosc &= bx01111111;
}

FORCE_INLINE void SetBit0(uint8_t *wartosc) {  /* Set bit 0 of wartosc */
  *wartosc |= bx00000001;
}

FORCE_INLINE void SetBit1(uint8_t *wartosc) {  /* Set bit 1 of wartosc */
  *wartosc |= bx00000010;
}

FORCE_INLINE void SetBit2(uint8_t *wartosc) {  /* Set bit 2 of wartosc */
  *wartosc |= bx00000100;
}

FORCE_INLINE void SetBit3(uint8_t *wartosc) {  /* Set bit 3 of wartosc */
  *wartosc |= bx00001000;
}

FORCE_INLINE void SetBit4(uint8_t *wartosc) {  /* Set bit 4 of wartosc */
  *wartosc |= bx00010000;
}

FORCE_INLINE void SetBit5(uint8_t *wartosc) {  /* Set bit 5 of wartosc */
  *wartosc |= bx00100000;
}

FORCE_INLINE void SetBit6(uint8_t *wartosc) {  /* Set bit 6 of wartosc */
  *wartosc |= bx01000000;
}

FORCE_INLINE void SetBit7(uint8_t *wartosc) {  /* Set bit 7 of wartosc */
  *wartosc |= bx10000000;
}


FORCE_INLINE void ShiftRightSRL(uint8_t *n) {
  /* Shift n right into Carry (MSB set to 0), and set some flags. */
  if ((*n & bx00000001) == 0) {
      ResetFlagC();  /* C contains old bit 0 */
    } else {
      SetFlagC();
  }
  ResetFlagN();  /* reset flags N and H */
  ResetFlagH();
  *n >>= 1;           /* Shift right by 1... */
  *n &= bx01111111;  /* ...and make sure to reset MSB */
  if (*n == 0) {
      SetFlagZ();  /* Set or reset flag Z */
    } else {
      ResetFlagZ();
  }
}


FORCE_INLINE void RotateLeftCarry(uint8_t *NumRotate) {
  /* Rotates NumRotate by 1 to the left through carry flag, and sets some Z80 flags.
     This function is used both by RLA AND RL r instructions. */
  static uint8_t TempUbyteRLC;
  TempUbyteRLC = (*NumRotate & bx10000000); /* Save old bit 7 */
  *NumRotate <<= 1;   /* shift left by 1 */
  if (GetFlagC() != 0) *NumRotate |= bx00000001;  /* Rotate carry flag into bit 0 */
  if (TempUbyteRLC == 0) {
      ResetFlagC();  /* Restore old bit 7 into carry flag */
    } else {
      SetFlagC();    /* Restore old bit 7 into carry flag */
  }
  /* Set or reset the Z flag (this is required to pass Blargg's test #09) */
  if (*NumRotate == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();  /* Flags N & H should be reseted */
  ResetFlagH();

}


FORCE_INLINE void RotateRightCarry(uint8_t *NumRotate) {
  /* Rotates NumRotate by 1 to the right through carry flag, and sets some Z80 flags.
     This function is used both by RRA and RR n instructions. */
  static uint8_t TempUbyte;
  TempUbyte = (*NumRotate & bx00000001); /* Save old bit 0 */
  *NumRotate >>= 1;   /* shift right by 1 */
  if (GetFlagC() != 0) *NumRotate |= bx10000000;  /* Rotate carry flag into bit 7 */
  if (TempUbyte == 0) {
      ResetFlagC();  /* Restore old bit 0 into carry flag */
    } else {
      SetFlagC();    /* Restore old bit 0 into carry flag */
  }
  /* Set or reset the Z flag (this is required to pass Blargg's test #09) */
  if (*NumRotate == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();  /* Flags N & H should be reseted */
  ResetFlagH();
}


FORCE_INLINE void SbcA(uint8_t ValToSub) {
  /* Subtract content of ValToSub along with the Carry flag from A, and sets some Z80 flags */
  /*static uint8_t UbyteBuff, FlagC; */
  static signed int IntBuff, FlagC;
  if (GetFlagC() != 0) FlagC = 1; else FlagC = 0; /* UbyteBuff--; */
  IntBuff = Register->A - (ValToSub + FlagC);
  if (IntBuff < 0) {
      SetFlagC();
      IntBuff += 256;
    } else {
      ResetFlagC();
  }
  if (((Register->A & bx00001111) - (ValToSub & bx00001111) - FlagC) < 0) {
      SetFlagH();
    } else {
      ResetFlagH();
  }
  Register->A = IntBuff;
  SetFlagN();
  if (Register->A == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
}


FORCE_INLINE void AdcA(uint8_t ValToAdd) {
  /* Add content of ValToAdd along with Carry flag to A, and sets some Z80 flags */
  static int UbyteBuff, FlagC;
  if (GetFlagC() == 0) {
      FlagC = 0;
    } else {
      FlagC = 1;
  }
  UbyteBuff = (Register->A + ValToAdd + FlagC);
  ResetFlagN();
  /*if ((Register->A + ValToAdd + FlagC) > 255) { */
  if (UbyteBuff > 255) {  /* Set flag C if carry from bit 7 */
      SetFlagC();
    } else {
      ResetFlagC();
  }
  if (((Register->A & bx00001111) + (ValToAdd & bx00001111) + FlagC) > 15) {  /* Set if carry from bit 3 */
  /*if ((UbyteBuff & 8) < (Register->A & 8)) { */
      SetFlagH();
    } else {
      ResetFlagH();
  }
  Register->A = (UbyteBuff & 0xFF);
  if (Register->A == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
}

FORCE_INLINE void ShiftSLA(uint8_t *ValueToShift) {
  /* Shift ValueToShift left by 1 bit, putting old bit 7 into Carry flag. */
  if ((*ValueToShift & bx10000000) == 0) {
      ResetFlagC();
    } else {
      SetFlagC();
  }
  *ValueToShift <<= 1;
  *ValueToShift &= bx11111111;
  if (*ValueToShift == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  ResetFlagH();
}


FORCE_INLINE void ShiftSRA(uint8_t *ValueToShift) {
  /* Shift ValueToShift right by 1 bit, putting old bit 0 into Carry flag. MSB doesn't change. */
  if ((*ValueToShift & bx00000001) == 0) {
      ResetFlagC();
    } else {
      SetFlagC();
  }
  *ValueToShift >>= 1;
  if ((*ValueToShift & bx01000000) == 0) {  /* Restore old bit 7 (MSB) as it was before */
      *ValueToShift &= bx01111111;  /* reset MSB */
    } else {
      *ValueToShift |= bx10000000;   /* set MSB */
  }
  if (*ValueToShift == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagN();
  ResetFlagH();
}


FORCE_INLINE void RRCA(void) {
  /* Rotates A by 1 to the right, and sets some Z80 flags. */
  static uint8_t TempUbyte;
  TempUbyte = (Register->A & bx00000001); /* Save old bit 7 */
  Register->A >>= 1;   /* shift right by 1 */
  if (TempUbyte == 0) {  /* if old bit 7 was 0 */
      ResetFlagC();  /* Reset carry flag */
      Register->A &= bx01111111;  /* restore old bit 7 into bit 0 */
    } else {  /* if old bit 7 was 1 */
      SetFlagC();    /* Set carry flag */
      Register->A |= bx10000000;  /* restore old bit 7 into bit 0 */
  }
  /* I *do* touch the Z flag. The Z80 manual tells to not touch it (while the Gameboy
     CPU manual tells that it should be reset if result is 0...)
     I decided to do the same than the NO$GMB emulator: I always reset Z. */
  /*IF Register->A = 0 THEN SetFlagZ() ELSE ResetFlagZ() */   /* Set or reset the Z flag */
  ResetFlagZ();
  ResetFlagN();
  ResetFlagH();    /* Flags N & H should be reseted */
}


FORCE_INLINE void RLCA(void) {
  /* Rotates register A by 1 to the left, and sets some Z80 flags. */
  static uint8_t TempUbyte;
  TempUbyte = (Register->A & bx10000000); /* Save old bit 7 */
  Register->A <<= 1;   /* shift left by 1 */
  if (TempUbyte == 0) {
      ResetFlagC();  /* Reset carry flag */
      Register->A &= bx11111110;  /* restore old bit 7 into bit 0 */
    } else {
      SetFlagC();    /* Set carry flag */
      Register->A |= bx00000001;  /* restore old bit 7 into bit 0 */
  }
  /* I *do* touch the Z flag. The Z80 manual tells to not touch it (while the Gameboy
     CPU manual tells that it should be reset if result is 0...)
     I decided to do the same than the NO$GMB emulator: I always reset Z. */
  /*IF Register->A = 0 THEN SetFlagZ() ELSE ResetFlagZ()  */  /* Set or reset the Z flag */
  ResetFlagZ();
  ResetFlagN();  /* Flags N & H should be reseted */
  ResetFlagH();
}


FORCE_INLINE void RotateRLC(uint8_t *valtorot) {
  /* The contents of register r are rotated left 1-bit position.
     The content of bit 7 is copied to the Carry flag and also to bit 0. */
  if ((*valtorot & bx10000000) == 0) {   /* save bit7 into flag C */
      ResetFlagC();
    } else {
      SetFlagC();
  }
  *valtorot <<= 1;
  /* now restore old bit 7 (holded in flag C) into bit 0 */
  if (GetFlagC() == 0) {
      *valtorot &= bx11111110;
    } else {
      *valtorot |= bx00000001;
  }
  if (*valtorot == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagH();
  ResetFlagN();
}


FORCE_INLINE void RotateRRC(uint8_t *valtorot) {
  /* The contents of register r are rotated right 1-bit position.
     The content of bit 0 is copied to the Carry flag and also to bit 7. */
  if ((*valtorot & bx00000001) == 0) {   /* save bit0 into flag C */
      ResetFlagC();
    } else {
      SetFlagC();
  }
  *valtorot >>= 1;
  /* now restore old bit 0 (holded in flag C) into bit 7 */
  if (GetFlagC() == 0) {
      *valtorot &= bx01111111;
    } else {
      *valtorot |= bx10000000;
  }
  if (*valtorot == 0) {
      SetFlagZ();
    } else {
      ResetFlagZ();
  }
  ResetFlagH();
  ResetFlagN();
}


/*static*/ FORCE_INLINE void AdjustDAA(void) { /* Decimal adjust on register A, and sets some Z80 flags */
  int tempA = Register->A;
  if (GetFlagN() == 0) {
      if ((GetFlagH() != 0) || ((tempA & 0xF) > 9)) tempA += 0x06;
      if ((GetFlagC() != 0) || (tempA > 0x9F)) tempA += 0x60;
    } else {
      if (GetFlagH() != 0) tempA = (tempA - 6) & 0xFF;
      if (GetFlagC() != 0) tempA -= 0x60;
  }
  ResetFlagH();
  if ((tempA & 0x100) == 0x100) SetFlagC();
  tempA &= 0xFF;
  if (tempA == 0) SetFlagZ(); else ResetFlagZ();
  Register->A = tempA;
}
