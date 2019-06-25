
#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);



void setup() {
  pinMode(PA1, INPUT);
  pinMode(PA2, INPUT);
  pinMode(PA3, INPUT);
  pinMode(PA4, INPUT);
  pinMode(PA5, INPUT);
  pinMode(PA6, INPUT);
  pinMode(PA7, INPUT);
  pinMode(PA8, OUTPUT);
    pinMode(PB8, OUTPUT);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  Serial.begin(115200);
}

void loop() {

  if (MIDI.read())
  {
    Serial.println("Midi event detected!");
    digitalWrite(PA8, LOW);
  }
  else
  {
    delay(1);
    digitalWrite(PA8, HIGH);
  }

  Serial.print(analogRead(PA1));
  Serial.print(" ");


  Serial.print(analogRead(PA2));
  Serial.print(" ");


  Serial.print(analogRead(PA3));
  Serial.print(" ");


  Serial.print(analogRead(PA4));
  Serial.print(" ");


  Serial.print(analogRead(PA5));
  Serial.print(" ");


  Serial.print(analogRead(PA6));
  Serial.print(" ");


  Serial.print(analogRead(PA7));
  Serial.println(" ");

for (unsigned int i = 0 ; i< 2048 ; i++) {
analogWrite(PB8,i);
analogWrite(PA8,i);
}
}
