#ifndef _midi_handles_
#define _midi_handles_

void HandleNoteOn(byte channel, byte note, byte velocity);
void HandleNoteOff(byte channel, byte note, byte velocity);
void HandleControlChange(byte channel, byte control, byte val);
void HandlePitchBend(byte channel, int bend);
void HandleAfterTouchChannel(byte channel, byte after);



#endif
