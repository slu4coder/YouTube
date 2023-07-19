void setup()
{
  DDRD = 0b00011100;  // set D2-4 to output

  for(int i=0; i<16384; i++) WriteByte(0, i); // clear RAM
}

void loop()
{
  int y = random(240);
  int x = random(50);
  int adr = ((y+16)<<6) + (12+x);
  WriteByte(0xff, adr);
}

void WriteByte(byte d, long a)
{
  a = (a<<8) + d;
  for(byte i=0; i<24; i++)
  {
    bitWrite(PORTD, 2, a&1);
    a = a>>1;
    bitWrite(PORTD, 3, HIGH); bitWrite(PORTD, 3, LOW); 
  }
  bitWrite(PORTD, 4, HIGH); bitWrite(PORTD, 4, LOW); 
}
