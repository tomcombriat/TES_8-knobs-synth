#include "midi_handles.h"

void HandleNoteOn(byte channel, byte note, byte velocity)
{

  carrier_freq = (Q16n0_to_Q16n16(mtof(note)));
    Serial.println(carrier_freq);
      digitalWrite(LED,HIGH);
}

void HandleNoteOff(byte channel, byte note, byte velocity)
{


      digitalWrite(LED,LOW);
}


