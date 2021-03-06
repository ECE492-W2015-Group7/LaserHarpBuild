#include <stdio.h>
#include "includes.h"
#include <math.h>
#include "altera_avalon_jtag_uart_regs.h"
#include <io.h>
#include "altera_avalon_fifo_util.h"
#include "altera_avalon_fifo_regs.h"
#include "altera_avalon_fifo.h"
#include "altera_up_avalon_audio.h"
#include "altera_up_avalon_audio_and_video_config.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
/* Other defines */
#define     BUFFER_SIZE    128
#define 	NUMBER_OF_LASER 	8

/* Definition of Task Stacks */
#define   TASK_STACKSIZE       2048
OS_STK    audioTaskStack[TASK_STACKSIZE];
OS_STK    laserInputTaskStack[TASK_STACKSIZE];

/* Definition of Task Priorities */
#define AUDIO_TASK_PRIORITY      	2
#define LASER_INPUT_TASK_PRIORITY  	1

OS_EVENT * laserStatusChanged;


void audioTask(void* pdata)
{
	INT8U err;
    alt_up_audio_dev * audio_dev;
    alt_up_av_config_dev * audio_config_dev;

    unsigned int buffer[BUFFER_SIZE];
    int i = 0;

    audio_config_dev = alt_up_av_config_open_dev("/dev/audio_and_video_config_0");
    if ( audio_config_dev == NULL)
        printf("Error: could not open audio config device \n");
    else
        printf("Opened audio config device \n");

    /* Open Devices */
    audio_dev = alt_up_audio_open_dev ("/dev/audio_0");
    if ( audio_dev == NULL)
        printf("Error: could not open audio device \n");
    else
        printf("Opened audio device \n");

    /* Configure WM8731 */
    alt_up_av_config_reset(audio_config_dev);
    alt_up_audio_reset_audio_core(audio_dev);

    /* Write to configuration registers in the audio codec; see datasheet for what these values mean */
    alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x0, 0x17);
    alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x1, 0x17);
    alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x2, 0x50);
    alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x3, 0x50);
    alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x4, 0x15);
    alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x5, 0x06);
    alt_up_av_config_write_audio_cfg_register(audio_config_dev, 0x6, 0x00);

	altera_avalon_fifo_init(FIFO_0_OUT_CSR_BASE, 0x0, 10, FIFO_0_OUT_CSR_FIFO_DEPTH-10);

	unsigned int data;
	unsigned int level;
	altera_avalon_fifo_read_fifo(FIFO_0_OUT_BASE, FIFO_0_OUT_CSR_BASE);
	while (1){
		level = altera_avalon_fifo_read_level(FIFO_0_OUT_CSR_BASE);
		if (level>0){
			for (i=0;i<level;i++){
				buffer[i]= (altera_avalon_fifo_read_fifo(FIFO_0_OUT_BASE, FIFO_0_OUT_CSR_BASE)>>16) + (3 * 0x7fff);
			}
		}
        alt_up_audio_write_fifo (audio_dev, buffer, level, ALT_UP_AUDIO_RIGHT);
        alt_up_audio_write_fifo (audio_dev, buffer, level, ALT_UP_AUDIO_LEFT);
	}
}

void laserInputTask(void* pdata){
	INT8U err;

	int* laserStatusPointer =(int* )SWITCH_BASE;
	unsigned int* midiOutPointer = (unsigned int*) MIDIOUT_0_BASE;

	int laserStatus = 0;
	int previousLaserStatus = 0;

	int note = 48;
	int noteStatus = 0;
	int noteType = 0;
	while(1){
		OSSemPend(laserStatusChanged, 0, &err);
		previousLaserStatus = laserStatus;
		laserStatus = *laserStatusPointer;
		laserStatus = 255 - laserStatus;	 //TODO fix 255 magic number  ( 2^number of laser)

		if (laserStatus == previousLaserStatus){	//laser status did not change skip the rest
			continue;
		}

		int i;
		int laserToPitchMappingTable[]={60,62,64,65,67,69,71,72};
		int differentBits = previousLaserStatus^laserStatus;		//XOR: 100 ^ 110 = 010

		for (i=0;i<NUMBER_OF_LASER;i++){
			if( ((1 << i) & (differentBits)) == (1 << i) ){		//if laser i is changed
				noteType = ((laserStatus &  (1 << i)) ==  (1 << i)) ?  1: 0;
				if (noteType == 1){
					turnOnVoice(laserToPitchMappingTable[i]);
					//*midiOutPointer = 303039617+laserToPitchMappingTable[i];
				}else{
					turnOffVoice(laserToPitchMappingTable[i]);
					//*midiOutPointer = 303039489+laserToPitchMappingTable[i];
				}
			}
		}

	}
}

void handleLaserStatusChange(int previousStatus, int currentStatus){
	int a[]={60,62,64,65,67,69,71,72};
}

static void laserStatusChangeInterrupt( void * context) {
	OSSemPost(laserStatusChanged);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(SWITCH_BASE, 0xff);
}


int main(void)
{
	laserStatusChanged = OSSemCreate(0);

	alt_ic_isr_register(SWITCH_IRQ_INTERRUPT_CONTROLLER_ID, //alt_u32 ic_id
						SWITCH_IRQ, //alt_u32 irq
						laserStatusChangeInterrupt, //alt_isr_func isr
						NULL,
						NULL);
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(SWITCH_BASE,0xff);


  OSTaskCreateExt(laserInputTask,
                  NULL,
                  (void *)&laserInputTaskStack[TASK_STACKSIZE-1],
                  LASER_INPUT_TASK_PRIORITY,
                  LASER_INPUT_TASK_PRIORITY,
                  laserInputTaskStack,
                  TASK_STACKSIZE,
                  NULL,
                  0);

	OSTaskCreateExt(audioTask,
					  NULL,
					  (void *)&audioTaskStack[TASK_STACKSIZE-1],
					  AUDIO_TASK_PRIORITY,
					  AUDIO_TASK_PRIORITY,
					  audioTaskStack,
					  TASK_STACKSIZE,
					  NULL,
					  0);
  OSStart();
  return 0;
}


