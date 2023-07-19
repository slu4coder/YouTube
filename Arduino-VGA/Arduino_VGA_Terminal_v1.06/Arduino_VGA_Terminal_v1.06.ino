// **********************************************************************************
// ***** ARDUINO NANO 40x25 ANSI DUMP (VT-100) TERMINAL by Carsten Herting 2020 *****
// **********************************************************************************
// LICENSE: See end of file for license information.
// Firmware Version 1.06 - 20.06.2020
// set Arduino Nano FuseA from default 0xFF to 0xBF (enabling output of 16MHz CLKO on pin 8)
// Pins 0-7:  Port D, used to write data to the parallel load of 74165, used to read data from 74174 input register
// Pin  8:    Arduino CLK out (16MHz)
// Pin  9:    74173 /OE
// Pin  10:   VSYNC (timer1)
// Pin  11:   74165 PE (timer2)
// Pin  12:   HSync "by hand" inside ISR
// Pin  13:   74173 MR

volatile int vLine;                              // current horizontal video line
volatile byte reg[128];                          // Queue of data to be written into VRAM
volatile byte regout = 0;                        // Index of current output position in queue
volatile byte regin = 0;                         // Index of current input position in queue
volatile int mFrame = 0;                         // 1/60s frame counter for cursor blinking
byte mEscValid = 0;                              // Number of valid characters in mEscBuffer[]  
byte mEscBuffer[5];
byte mRow = 8;                                   // cursor position in terminal window
byte mCol = 0;                                   // cursor position in terminal window
byte oldc = ' ';                                 // stores char a cursor position

byte vram[25][40] = { 
  "****************************************",
  "*  ATMega328P VGA ANSI TERMINAL v1.06  *",
  "****************************************",
  "* - 320x480 pixels / 60Hz refresh rate *",
  "* - 40x25 characters VRAM text output  *",
  "* - full 8x8 pixel ASCII character set *",
  "* - processing ANSI escape sequences   *",
  "****************************************",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
  "                                        ",
};

// my improved charset line data starting with character 32 (SPACE)
const byte charset[8][96] = {
  0x00,0x18,0x66,0x66,0x18,0x62,0x3C,0x06,0x0C,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x18,0x3C,0x3C,0x06,0x7E,0x3C,0x7E,0x3C,0x3C,0x00,0x00,0x0E,0x00,0x70,0x3C,0x3C,0x18,0x7C,0x3C,0x78,0x7E,0x7E,0x3C,0x66,0x3C,0x1E,0x66,0x60,0x63,0x66,0x3C,0x7C,0x3C,0x7C,0x3C,0x7E,0x66,0x66,0x63,0x66,0x66,0x7E,0x3C,0x00,0x3C,0x00,0x00,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0E,0x18,0x70,0x00,0xFF,
  0x00,0x18,0x66,0x66,0x3E,0x66,0x66,0x0C,0x18,0x18,0x66,0x18,0x00,0x00,0x00,0x03,0x66,0x18,0x66,0x66,0x0E,0x60,0x66,0x66,0x66,0x66,0x00,0x00,0x18,0x00,0x18,0x66,0x66,0x3C,0x66,0x66,0x6C,0x60,0x60,0x66,0x66,0x18,0x0C,0x6C,0x60,0x77,0x76,0x66,0x66,0x66,0x66,0x66,0x18,0x66,0x66,0x63,0x66,0x66,0x06,0x30,0x60,0x0C,0x18,0x00,0x66,0x00,0x60,0x00,0x06,0x00,0x0E,0x00,0x60,0x18,0x06,0x60,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x00,0xFF,
  0x00,0x18,0x66,0xFF,0x60,0x0C,0x3C,0x18,0x30,0x0C,0x3C,0x18,0x00,0x00,0x00,0x06,0x6E,0x38,0x06,0x06,0x1E,0x7C,0x60,0x0C,0x66,0x66,0x18,0x18,0x30,0x7E,0x0C,0x06,0x6E,0x66,0x66,0x60,0x66,0x60,0x60,0x60,0x66,0x18,0x0C,0x78,0x60,0x7F,0x7E,0x66,0x66,0x66,0x66,0x60,0x18,0x66,0x66,0x63,0x3C,0x66,0x0C,0x30,0x30,0x0C,0x3C,0x00,0x6E,0x3C,0x60,0x3C,0x06,0x3C,0x18,0x3E,0x60,0x00,0x00,0x60,0x18,0x66,0x7C,0x3C,0x7C,0x3E,0x7C,0x3E,0x7E,0x66,0x66,0x63,0x66,0x66,0x7E,0x18,0x18,0x18,0x00,0xFF,
  0x00,0x18,0x00,0x66,0x3C,0x18,0x38,0x00,0x30,0x0C,0xFF,0x7E,0x00,0x7E,0x00,0x0C,0x76,0x18,0x0C,0x1C,0x66,0x06,0x7C,0x18,0x3C,0x3E,0x00,0x00,0x60,0x00,0x06,0x0C,0x6E,0x7E,0x7C,0x60,0x66,0x78,0x78,0x6E,0x7E,0x18,0x0C,0x70,0x60,0x6B,0x7E,0x66,0x7C,0x66,0x7C,0x3C,0x18,0x66,0x66,0x6B,0x18,0x3C,0x18,0x30,0x18,0x0C,0x7E,0x00,0x6E,0x06,0x7C,0x60,0x3E,0x66,0x3E,0x66,0x7C,0x38,0x06,0x6C,0x18,0x7F,0x66,0x66,0x66,0x66,0x66,0x60,0x18,0x66,0x66,0x6B,0x3C,0x66,0x0C,0x70,0x18,0x0E,0x3B,0xFF,
  0x00,0x00,0x00,0xFF,0x06,0x30,0x67,0x00,0x30,0x0C,0x3C,0x18,0x00,0x00,0x00,0x18,0x66,0x18,0x30,0x06,0x7F,0x06,0x66,0x18,0x66,0x06,0x00,0x00,0x30,0x7E,0x0C,0x18,0x60,0x66,0x66,0x60,0x66,0x60,0x60,0x66,0x66,0x18,0x0C,0x78,0x60,0x63,0x6E,0x66,0x60,0x66,0x78,0x06,0x18,0x66,0x66,0x7F,0x3C,0x18,0x30,0x30,0x0C,0x0C,0x18,0x00,0x60,0x3E,0x66,0x60,0x66,0x7E,0x18,0x66,0x66,0x18,0x06,0x78,0x18,0x7F,0x66,0x66,0x66,0x66,0x60,0x3C,0x18,0x66,0x66,0x7F,0x18,0x66,0x18,0x18,0x18,0x18,0x6E,0xFF,
  0x00,0x00,0x00,0x66,0x7C,0x66,0x66,0x00,0x18,0x18,0x66,0x18,0x18,0x00,0x18,0x30,0x66,0x18,0x60,0x66,0x06,0x66,0x66,0x18,0x66,0x66,0x18,0x18,0x18,0x00,0x18,0x00,0x66,0x66,0x66,0x66,0x6C,0x60,0x60,0x66,0x66,0x18,0x6C,0x6C,0x60,0x63,0x66,0x66,0x60,0x3C,0x6C,0x66,0x18,0x66,0x3C,0x77,0x66,0x18,0x60,0x30,0x06,0x0C,0x18,0x00,0x66,0x66,0x66,0x60,0x66,0x60,0x18,0x3E,0x66,0x18,0x06,0x6C,0x18,0x6B,0x66,0x66,0x7C,0x3E,0x60,0x06,0x18,0x66,0x3C,0x3E,0x3C,0x3E,0x30,0x18,0x18,0x18,0x00,0xFF,
  0x00,0x18,0x00,0x66,0x18,0x46,0x3F,0x00,0x0C,0x30,0x00,0x00,0x18,0x00,0x18,0x60,0x3C,0x7E,0x7E,0x3C,0x06,0x3C,0x3C,0x18,0x3C,0x3C,0x00,0x18,0x0E,0x00,0x70,0x18,0x3C,0x66,0x7C,0x3C,0x78,0x7E,0x60,0x3C,0x66,0x3C,0x38,0x66,0x7E,0x63,0x66,0x3C,0x60,0x0E,0x66,0x3C,0x18,0x3C,0x18,0x63,0x66,0x18,0x7E,0x3C,0x03,0x3C,0x18,0x00,0x3C,0x3E,0x7C,0x3C,0x3E,0x3C,0x18,0x06,0x66,0x3C,0x06,0x66,0x3C,0x63,0x66,0x3C,0x60,0x06,0x60,0x7C,0x0E,0x3E,0x18,0x36,0x66,0x0C,0x7E,0x0E,0x18,0x70,0x00,0xFF,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7C,0x00,0x00,0x3C,0x00,0x00,0x00,0x00,0x00,0x60,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0x00,0x00,0x18,0x00,0x00,0xFF,
};

int main()                                        // IMPORTANT: Enforce simplest main() loop possible
{
  setup();
  while(true) loop();
}

void setup()
{
  noInterrupts();                                 // disable interrupts before messing around with timer registers

  DDRB  = 0b00111111;                             // pin8: CLKO, pin 9: 74173 /OE, pin 10: VSYNC (timer1), pin 11: 74165 PE (timer2), pin 12: HSync "by hand" inside ISR, pin 13: 74173 MR
  PORTB = 0b00010010;                             // MR=0, HSYNC=1, /OE=1
  GTCCR = 0b10000011;                             // set TSM, PSRSYNC und PSRASY to correlate all 3 timers

  // *****************************
  // ***** Timer0: VGA HSYNC *****
  // *****************************
  TCNT0  = 0;
  TCCR0A = (1<<WGM01) | (0<<WGM00);               // mode 2: Clear Timer on Compare Match (CTC)
  TCCR0B = (0<<WGM02) | (0<<CS02) | (1<<CS01) | (0<<CS00); // x8 prescaler -> 0.5µs
  OCR0A  = 63;                                    // compare match register A (TOP) -> 32µs
  TIMSK0 = (1<<OCIE0A);                           // Output Compare Match A Interrupt Enable (not working: TOIE1 with ISR TIMER0_TOIE1_vect because it is already defined by timing functions)
  
  // *****************************
  // ***** Timer1: VGA VSYNC *****
  // *****************************
  TCNT1  = 0;
  TCCR1A = (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (1<<WGM10);         // mode 15 (Fast PWM), set OC1B on Compare Match, clear OC1B at BOTTOM, controlling OC1B pin 10
  TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS12) | (0<<CS11) | (1<<CS10); // x1024 prescaler -> 64µs
  OCR1A  = 259;                                   // compare match register A (TOP) -> 16.64ms
  OCR1B  = 0;                                     // compare match register B -> 64µs
  TIMSK1 = (1<<TOIE1);                            // enable timer overflow interrupt setting vLines = 0

  // *************************************************
  // ***** Timer2: 74165 Parallel Load Enable PE *****
  // *************************************************
  TCNT2  = 0;
  TCCR2A = (1<<COM2A1) | (1<<COM2A0) | (1<<WGM21) | (1<<WGM20); // mode 7: Fast PWM, COM2A0=0: normal port HIGH, COM2A0=1: Toggle OC2A pin 11 on Compare Match
  TCCR2B = (1<<WGM22) | (0<<CS22) | (0<<CS21) | (1<<CS20) ;     // set x0 prescaler -> 62.5ns;
  OCR2A  = 7;                                     // compare match register A (TOP) -> 250ns
  TIMSK2 = 0;                                     // no interrupts here

  GTCCR = 0;                                      // clear TSM => all timers start synchronously
  
  UCSR0B = 0;                       	            // brute-force the USART off just in case...

  interrupts();
}

ISR(TIMER1_OVF_vect) { vLine = 0; mFrame++; }     // timer overflow interrupt sets back vLine at the start of a frame

ISR(TIMER0_COMPA_vect)                            // HSYNC generation and drawing of a scanline
{
  if (TCNT2 & 1) PORTB = 0b00010010;              // MR=0, HSYNC=1, /OE=1: Canceling out interrupt jitter using the fast timer (if branching, this code takes a cycle longer)
  PORTB = 0b00000000;                             // MR=0, HSYNC=0, /OE=0: Start the HSYNC pulse & activate 74173 output
    DDRD  = 0b00000000;                           // check the 74173 input register for new data, switch to D = input
    __builtin_avr_delay_cycles(1);                // IMPORTANT: Wait for the 74173 outputs to stabilize
    byte a = PIND;                                // doppelte Auslesung, um Auslesen während eines Schreibvorgangs abzufangen
    byte b = PIND;
    byte c = regin;
    
    if(a != 0)                                    // wurde überhaupt etwas != 0 gelesen?
    {
      if(a == b)                                  // war der Lesevorgang valide?
      {
        PORTB = 0b00100010;                       // MR=1, HSYNC=0, /OE=1: Pull register reset and output
        reg[c++] = a;                             // übernimm den neuen Wert in den Puffer
        c &= 0b1111111;
        regin = c;
      } else __builtin_avr_delay_cycles(13);      // sorgt für gleiche Laufzeit in jedem Zweig 4/5/9       
    } else __builtin_avr_delay_cycles(15);        // sorgt für gleiche Laufzeit in jedem Zweig              
    
    PORTB = 0b00000010;                           // MR=0, HSYNC=0, /OE=1: Disable register output

    DDRD  = 0b11111111;                           // switch port D to output
    PORTD = 0;                                    // prevent trash data from being fed into the shift register
    vLine++;                                      // use the time during the pulse to init pointers
    byte* vrow = vram[byte((vLine-36)>>4)];       // pointer to the vram row 0...24 to display
    byte* cset = charset[byte((vLine-36)>>1) & 0b00000111] - 32; // pointer to the charset line 0..7 to use starting @ character 32
  PORTB = 0b00010010;                             // MR=0, HSYNC=1, /OE=1: End of HSYNC pulse

  if (vLine >= 36 && vLine < 436)                 // skip 2 lines (VSYNC pulse) + 33 lines (vertical back porch)
  {  
    TCCR2A &= ~(1<<COM2A1);                       // enable OC2A toggling pin 11
    PORTD = cset[*vrow++];                        // start feeding data
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    PORTD = cset[*vrow++];
    __builtin_avr_delay_cycles(1);
    TCCR2A |= (1<<COM2A1);                        // stop any further parallel load PE (disabling OC2A toggling pin 11)
    PORTD = 0;
  }
}

void loop()
{ 
  if ((mFrame & 63) > 31) vram[mRow][mCol] = 127; else vram[mRow][mCol] = oldc;   // cursor blinking

  if (regout != regin)                              // was a character received
  {
    vram[mRow][mCol] = oldc;                        // restore character beneath the cursor BEFORE a possible scrolling happens
    do
    {
      ProcessChar(reg[regout]);                   // process this character
      regout++;
      regout &= 0b1111111;
    } while(regout != regin);
    oldc = vram[mRow][mCol];    
  }
}

void Scroll()
{
  for(int r=0; r<24; r++) for(int c=0; c<40; c++) vram[r][c] = vram[r+1][c];    // move all rows one step up
  for(int c=0; c<40; c++) vram[24][c] = 32;                                     // fill lowest line with SPACES
}

void ProcessChar(byte inbyte)                     // processes a character (accepts some VT52/GEMDOS ESC sequences, control chars, normal chars)
{
  mFrame = 25;

  if (mEscValid > 4) mEscValid = 0;               // unverarbeibare ESC-Sequenzen löschen und dieses Zeichen normal verarbeiten
  if (inbyte == 27) { mEscValid = 1; return; }    // neue ESC sequence starten

  if(mEscValid > 0)                               // ES IST BEREITS EIN ESC AKTIV
  {
    if (mEscValid < 2) { if (inbyte == '[') mEscValid++; else mEscValid = 0; } // als 2. Zeichen MUSS '[' kommen    
    else                                          // es wurde bereits '\e[' korrekt empfangen...
    {
      mEscBuffer[mEscValid++] = inbyte;           // ein weiteres Zeichen hinzufügen
      switch(inbyte)                // Für jede ESC sequence muss geprüft werden, ob damit der Befehl komplett ist
      {
        case 'A':                            // move cursor up
        {
          byte anz;
          if (mEscValid == 5) anz = 10 * (mEscBuffer[2] - 48) + mEscBuffer[3] - 48;
          if (mEscValid == 4) anz = mEscBuffer[2] - 48;
          if (mEscValid == 3) anz = 1;
          for(byte i=0; i<anz; i++) if (mRow > 0) mRow--;
          mEscValid = 0;
          break;
        }
        case 'B':                            // move cursor down
        {
          byte anz;
          if (mEscValid == 5) anz = 10 * (mEscBuffer[2] - 48) + mEscBuffer[3] - 48;
          if (mEscValid == 4) anz = mEscBuffer[2] - 48;
          if (mEscValid == 3) anz = 1;
          for(byte i=0; i<anz; i++) if (mRow < 24) mRow++;
          mEscValid = 0;
          break;
        }
        case 'C':                            // move cursor right
        {
          byte anz;
          if (mEscValid == 5) anz = 10 * (mEscBuffer[2] - 48) + mEscBuffer[3] - 48;
          if (mEscValid == 4) anz = mEscBuffer[2] - 48;
          if (mEscValid == 3) anz = 1;
          for(byte i=0; i<anz; i++) { if (mCol < 39) mCol++; else if (mRow < 24) { mCol = 0; mRow++; } }
          mEscValid = 0;
          break;
        }
        case 'D':                            // move cursor left
        {
          byte anz;
          if (mEscValid == 5) anz = 10 * (mEscBuffer[2] - 48) + mEscBuffer[3] - 48;
          if (mEscValid == 4) anz = mEscBuffer[2] - 48;
          if (mEscValid == 3) anz = 1;
          for(byte i=0; i<anz; i++) { if (mCol > 0) mCol--; else if (mRow > 0) { mCol = 39; mRow--; } }
          mEscValid = 0;
          break;
        }
        case 'H':                            // move cursor to upper left corner
          mRow = 0; mCol = 0;
          mEscValid = 0;
          break;
        case 'J':                            // clear VRAM from cursor onwards 
          for(int c=mCol; c<40; c++) vram[mRow][c] = 32;
          for(int r=mRow+1; r<25; r++) for(int c = 0; c<40; c++) vram[r][c] = 32;
          mEscValid = 0;
          break;
        case 'K':                            // clear line from cursor onwards (does not move the cursor) 
          for(int c = mCol; c<40; c++) vram[mRow][c] = 32;
          mEscValid = 0;
          break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
          break;                             // Zahlen erstmal hinzufügen
        default:
          mEscValid = 0;
          break;      
      }
    }
  }
  else
  {
    switch(inbyte)
    {
      case '\n':                           // Sonderzeichen 'newline' abfangen
        mCol = 0;
        if (mRow < 24) mRow++; else Scroll();
        break;
      case '\r':                           // Sonderzeichen 'carriage return' abfangen
        mCol = 0;
        break;
      case 8:                              // Sonderzeichen 'BACKSPACE' abfangen
        if (mCol > 0) vram[mRow][--mCol] = 32;
        else if (mRow > 0) { mRow--; vram[mRow][39] = 32; mCol = 39; }
        break;
      default:
        if (inbyte >= 32)
        {
          vram[mRow][mCol++] = inbyte;           // einfach das via TI OUT gesendete Zeichen im Terminal ausgeben
          if (mCol > 39)
          {
            mCol = 0;
            if (mRow < 24) mRow++; else Scroll();
          }
        }
        break;
    }          
  }
}

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2020 Carsten Herting
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
