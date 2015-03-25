/*
 * MidiLib.h
 *
 *  Created on: 2015-03-07
 *      Author: eorodrig
 */

#ifndef MIDILIB_H_
#define MIDILIB_H_

void processNote(int noteStatus, int pitch, int velocity);
void initMidiLib();
void sendNoteOn2Voice(int voiceNum, float sampleFreq);
void sendNoteOff2Voice(int voiceNum);


#endif /* MIDILIB_H_ */