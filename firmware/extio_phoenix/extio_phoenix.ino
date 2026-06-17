/* comment */

char inputBuffer[5];
int inputLen = 0;
char outputBuffer[4];
int outputLen = 0;

boolean runningError = false;

void setup()
{
  Serial.begin(115200);
  inputBuffer[0] = 0;
  outputBuffer[0] = 0;

  // declare input pins and output pins
  for ( int i = 2; i <= 21; i++ )
  {
    if ( i == 13 ) // do not hook up - not a normal pin
    {
      continue;
    }
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(A5, INPUT_PULLUP);
  pinMode(A6, INPUT_PULLUP);
  pinMode(A7, INPUT_PULLUP);
  for ( int i = 24; i <= 53; i++ )
  {
    pinMode(i, OUTPUT);
  }
  pinMode(A5, OUTPUT);
  pinMode(A6, OUTPUT);
  pinMode(A7, OUTPUT);
}

void loop()
{
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent()
{
  while (Serial.available())
  {
    if (inputLen < 5)
    {
      inputBuffer[inputLen++] = (char)Serial.read();
    }
    else
    {
      Serial.read(); // discard if somehow overrun
    }

    if ( inputBuffer[0] == 'C' ) // pulse coin counter once
    {
      // TODO: coin counter is on pin 22
      inputLen = 0;
    }
    else if ( inputBuffer[0] == 'L' ) // lockout signal engage
    {
      // TODO: lockout coil is on pin 23
      inputLen = 0;
    }
    else if ( inputBuffer[0] == 'U' ) // unlock the coin mech
    {
      // TODO: lockout coil is on pin 23
      inputLen = 0;
    }
    else if ( inputBuffer[0] == 'I' ) // please reply with input
    {
      prepareInputPacket();
      inputLen = 0;
    }
    else if ( inputBuffer[0] == '1' ) // LED lights on the orbs
    {
      if ( inputLen >= 4 )
      {
        for ( int i = 0; i < 8; i++ )
        {
          digitalWrite(i+30, (inputBuffer[1] & (1<<i)) != 0 ? HIGH : LOW);
          digitalWrite(i+38, (inputBuffer[2] & (1<<i)) != 0 ? HIGH : LOW);
          digitalWrite(i+46, (inputBuffer[3] & (1<<i)) != 0 ? HIGH : LOW);
        }
        inputLen = 0;
      }
    }
    else if ( inputBuffer[0] == '2' ) // menu buttons and spot lights
    {
      if ( inputLen >= 3 )
      {
        for ( int i = 0; i < 6; i++ )
        {
          digitalWrite(i+24, (inputBuffer[1] & (1<<i)) != 0 ? HIGH : LOW);
        }
        digitalWrite(A13, (inputBuffer[2] & (1<<0)) != 0 ? HIGH : LOW); // center spotlight (yellow)
        digitalWrite(A14, (inputBuffer[2] & (1<<1)) != 0 ? HIGH : LOW); // middle spotlight pair
        digitalWrite(A15, (inputBuffer[2] & (1<<2)) != 0 ? HIGH : LOW); // outer spotlight pair

        inputLen = 0;
      }
    }
    else if ( inputBuffer[0] == 'D' ) // special initialization packet
    {
      if ( inputLen >= 3 )
      {
        if (inputBuffer[0] == 'D' && inputBuffer[1] == 'M' && inputBuffer[2] == 'X')
        {
          outputBuffer[0] = 'O'; outputBuffer[1] = 'K'; outputBuffer[2] = '!'; outputBuffer[3] = 0;
          outputLen = 3;
          runningError = false;
        }
        else
        {
          outputBuffer[0] = 'B'; outputBuffer[1] = 'A'; outputBuffer[2] = 'D'; outputBuffer[3] = 0;
          outputLen = 3;
          runningError = true; // what are you sending me?
        }
        inputLen = 0;
      }
    }
    else
    {
      inputLen = 0;
      runningError = true; // unrecognized packet type, are we out of sync?
    }

    // did we create a response?
    if ( outputLen > 0 )
    {
      Serial.write(outputBuffer, outputLen);
      outputLen = 0;
    }
  }
}

void prepareInputPacket()
{
  const int pins0[] = { A5, A6, 2, 3, 4, 5, 6, 7 };
  const int pins1[] = { 8, 9, 10, 11, 12, 13, 14, 15 };
  const int pins2[] = { 16, 17, 18, 19, 20, 21, A7, 12 }; // pin 12 is not hooked up

  if ( !runningError )
  {
    outputBuffer[0] = packThePins(pins0);
    outputBuffer[1] = packThePins(pins1);
    outputBuffer[2] = packThePins(pins2);
  }
  else
  {
    outputBuffer[0] = 'B'; outputBuffer[1] = 'A'; outputBuffer[2] = 'D';
  }
  outputLen = 3;
}

// a helper function for prepareInputPacket
unsigned char packThePins(const int pins[])
{
    unsigned char c = 0;
    for ( int i = 0; i < 8; i++ )
    {
        if ( digitalRead(pins[i]) == HIGH )
        {
            c |= 1 << i;
        }
    }
    return c;
}
