/*************************************************************************
 * Copyright (c) 2004 Altera Corporation, San Jose, California, USA.      *
 * All rights reserved. All use of this software and documentation is     *
 * subject to the License Agreement located at the end of this file below.*
 **************************************************************************
 * Description:                                                           *
 * The following is a simple hello world program running MicroC/OS-II.The *
 * purpose of the design is to be a very simple application that just     *
 * demonstrates MicroC/OS-II running on NIOS II.The design doesn't account*
 * for issues such as checking system call return codes. etc.             *
 *                                                                        *
 * Requirements:                                                          *
 *   -Supported Example Hardware Platforms                                *
 *     Standard                                                           *
 *     Full Featured                                                      *
 *     Low Cost                                                           *
 *   -Supported Development Boards                                        *
 *     Nios II Development Board, Stratix II Edition                      *
 *     Nios Development Board, Stratix Professional Edition               *
 *     Nios Development Board, Stratix Edition                            *
 *     Nios Development Board, Cyclone Edition                            *
 *   -System Library Settings                                             *
 *     RTOS Type - MicroC/OS-II                                           *
 *     Periodic System Timer                                              *
 *   -Know Issues                                                         *
 *     If this design is run on the ISS, terminal output will take several*
 *     minutes per iteration.
 *
 *
 *     http://www.phy.mtu.edu/~suits/notefreqs.html
 *     newt.phys.unsw.edu.au/jw/notes.html                              *
 **************************************************************************/
#include "altera_avalon_jtag_uart_regs.h"
#include <stdio.h>
#include "altera_up_avalon_audio_and_video_config.h"
#include <math.h>
#include "system.h"
#include "alt_types.h"
#include <unistd.h>



/*Defines the buffer size */
#define     BUFFER_SIZE    128

//total number of lasers (voices)
#define 	TOTAL_VOICES	1

//this is the size of our LUT
#define		 NUMBER_OF_POINTS_IN_WAVE_LUT		4096

//MIDI Signals received == 3 bytes
//Byte 1: Note status
//This defines the note statuses
#define		NOTE_ON			1
#define		NOTE_OFF		0

//Byte 2: Pitch (Note)
//this defines the notes frequencies
#define		FREQ_EMPTY_NOTE		0.0
#define 	FREQ_BASE			8.1757989156
#define		SAMPLE_RATE			32000
#define		BITSHIFT_COMPENSATION 16

#define		EMPTY_NOTE			0

//Byte 3: Velocity
//this defines the velocities
#define		OFF_VELOCITY	0


//
#define		WAVE_NONE		0
#define		WAVE_SINE		1
#define		WAVE_SQUARE		2
#define		WAVE_SAW		3

// each voice has a status, pitch (note), and velocity
struct voice {
	int status;
	int note;
	int velocity;
};

//we need a list of 8 voices
static struct voice VOICE_TABLE[TOTAL_VOICES];




void setVoice(int voiceNum, int sampleFreqOsc0){

	int * voiceAddr;

	switch (voiceNum) {
	case 0:
		voiceAddr = SYNTHESIZER_0_BASE;
		break;
	default:
		//return;
		break;
	}

	*voiceAddr = sampleFreqOsc0;

}


void setEndVoice(int voiceNum){

	int * voiceAddr;

	switch (voiceNum) {
	case 0:
		voiceAddr = SYNTHESIZER_0_BASE;
		break;
	default:
		//return;
		break;
	}

	*voiceAddr = 0;

}

//this sends the Note on command to selected voice

void sendNoteOn2Voice(int voiceNum, float sampleFreq) {

	int sampleFreqOsc0 = roundf(sampleFreq);

	//this sends in the sample frequencies to the selected voice
	setVoice(voiceNum, sampleFreqOsc0);
}

//this sends the Note off command to selected voice

void sendNoteOff2Voice(int voiceNum) {

	setEndVoice(voiceNum);

}

/**
 * This Iterate through the voice/note table and look for an empty voice
 * If it finds an unused (off) note, it will use that voice
 * If all the voices are used, it does nothing
 *
 * it returns the index
 */
int turnOnVoice(int noteNum, int velocity, float sampleFreq) {

	int index = 0;

	for (index = 0; index < TOTAL_VOICES; index++) {
		if (NOTE_OFF == VOICE_TABLE[index].status) {
			VOICE_TABLE[index].note = noteNum;
			VOICE_TABLE[index].status = NOTE_ON;
			VOICE_TABLE[index].velocity = velocity;

			sendNoteOn2Voice(index, sampleFreq);

			return index;
		}
	}

	return -1;
}

/**
 * This Iterate through the voice/note table and look for a note to turn off
 * If it finds a specific note, it will reset the note to an off state
 * If it doesn't find it, it does nothing
 */
int turnOffVoice(int noteNum, int velocity) {

	int index = 0;

	for (index = 0; index < TOTAL_VOICES; index++) {
		if (noteNum == VOICE_TABLE[index].note) {
			VOICE_TABLE[index].note = EMPTY_NOTE;
			VOICE_TABLE[index].status = NOTE_OFF;
			VOICE_TABLE[index].velocity = OFF_VELOCITY;

			sendNoteOff2Voice(index);

			return index;
		}
	}

	return -1;
}

/**
 * This will calculate the frequency of the midi note
 */
float midiNote2midiFreq(double midiNote) {
	return (FREQ_BASE * pow(2, (midiNote / 12)));
}

/**
 * This will calculate the sampling frequency used to sample the SINE LUT
 */
float midiFreq2sampleFreq(float midiFreq) {

	float sampleFreq = (midiFreq / SAMPLE_RATE) * NUMBER_OF_POINTS_IN_WAVE_LUT * BITSHIFT_COMPENSATION;

	return sampleFreq;

}

/*This is the API for the midiDriver*
 * It requires the status of the note
 * the pitch (midi note numbeR)
 * and the velocity (velocity of 0 will result in note off
 */
void processNote(int noteStatus, int pitch, int velocity) {

	float midiFreq = 0;
	float sampleFreq = 0;
	int voiceNumber = 0;


	if (velocity != 0) {
		midiFreq = midiNote2midiFreq(pitch);
		sampleFreq = midiFreq2sampleFreq(midiFreq);
		voiceNumber = turnOnVoice(pitch, velocity, sampleFreq);
	} else {
		voiceNumber = turnOffVoice(pitch, velocity);
	}

}

//Initializes Synthesizer Compo
void initMidiLib() {

	//alt_up_audio_dev * audio_dev;
	alt_up_av_config_dev * audio_config_dev;

	//  unsigned int l_buf[BUFFER_SIZE];
	//  int i = 0;
	//  int writeSizeL = 0;

	/* Open Devices */
	// audio_dev = alt_up_audio_open_dev ("/dev/audio_0");
	//  if ( audio_dev == NULL)
	//      printf("Error: could not open audio device \n");
	//  else
	//       printf("Opened audio device \n");
	audio_config_dev = alt_up_av_config_open_dev(AUDIO_AND_VIDEO_CONFIG_0_NAME);
	if (audio_config_dev == NULL)
		printf("Error: could not open audio config device \n");
	else
		printf("Opened audio config device \n");

	/* Configure WM8731 */
	//  alt_up_audio_reset_audio_core(audio_dev);
	alt_up_av_config_reset(audio_config_dev);

	/* Write to configuration registers in the audio codec; see datasheet for what these values mean */
	alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x0, 0x17);
	alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x1, 0x17);
	alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x2, 0xF9);
	alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x3, 0xF9);
	alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x4, 0x12);
	alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x5, 0x06);
	alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x6, 0x00);

}

/******************************************************************************
 *                                                                             *
 * License Agreement                                                           *
 *                                                                             *
 * Copyright (c) 2004 Altera Corporation, San Jose, California, USA.           *
 * All rights reserved.                                                        *
 *                                                                             *
 * Permission is hereby granted, free of charge, to any person obtaining a     *
 * copy of this software and associated documentation files (the "Software"),  *
 * to deal in the Software without restriction, including without limitation   *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,    *
 * and/or sell copies of the Software, and to permit persons to whom the       *
 * Software is furnished to do so, subject to the following conditions:        *
 *                                                                             *
 * The above copyright notice and this permission notice shall be included in  *
 * all copies or substantial portions of the Software.                         *
 *                                                                             *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE *
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER         *
 * DEALINGS IN THE SOFTWARE.                                                   *
 *                                                                             *
 * This agreement shall be governed in all respects by the laws of the State   *
 * of California and by the laws of the United States of America.              *
 * Altera does not recommend, suggest or require that this reference design    *
 * file be used in conjunction or combination with any other product.          *
 ******************************************************************************/
