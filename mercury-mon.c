/*
 *      Mercury power meter monitoring application. The application is designed
 *      to stay active and regularly poll data from the power meter.
 */
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "mercury236.h"

#define BSZ	                255
#define OPT_DEBUG		"--debug"
#define OPT_HELP		"--help"

int debugPrint = 0;

typedef enum
{
	EXIT_OK = 0,
	EXIT_FAIL = -1
} ExitCode;

// -- Command line usage help
void printUsage()
{
	printf("Usage: mercury-mon [RS485]\n\r\n\r");
	printf("  RS485\t\taddress of RS485 dongle (e.g. /dev/ttyUSB0), required.\n\r");
	printf("  %s\tto print extra debug info.\n\r", OPT_DEBUG);
	printf("\n\r");
	printf("  %s\tprints this screen.\n\r", OPT_HELP);
	printf("\n\r");
        printf("Press Ctrl+C to exit.\n\r");
}

int terminateMonitorNow = 0;

// -- Signal Handler for SIGINT 
void sigint_handler(int sig_num)
{
        printf("\nTerminating monitor by Ctrl+C...\n");
        terminateMonitorNow = 1;
}

int main(int argc, const char** args)
{
	// must have RS485 address (1st required param)
	if (argc < 2)
	{
		printf("Error: no RS485 device specified\n\r\n\r");
		printUsage();
		exit(EXIT_FAIL);
	}

        // Ctrl+C handler
        signal(SIGINT, sigint_handler);

	char dev[BSZ];
	strncpy(dev, args[1], BSZ);

	// get command line options
	for (int i=2; i<argc; i++)
	{
		if (!strcmp(OPT_DEBUG, args[i]))
			debugPrint = 1;
		// else if (!strcmp(OPT_TEST_RUN, args[i]))
		// 	dryRun = 1;
		// else if (!strcmp(OPT_HUMAN, args[i]))
		// 	format = OF_HUMAN;
		// else if (!strcmp(OPT_CSV, args[i]))
		// 	format = OF_CSV;
		// else if (!strcmp(OPT_JSON, args[i]))
		// 	format = OF_JSON;
		// else if (!strcmp(OPT_HEADER, args[i]))
		// 	header = 1;
		else if (!strcmp(OPT_HELP, args[i]))
		{
			printUsage();
			exit(EXIT_OK);
		}
		else
		{
			printf("Error: %s option is not recognised\n\r\n\r", args[i]);
			printUsage();
			exit(EXIT_FAIL);
		}
	}

        // // Open shared memory file descriptor
        // int fdSharedMemory = shm_open(
        //                         SHARED_MEM_BACKING_FILE,
        //                         O_RDWR | O_CREAT,               /* read/write, create if needed */
        //                         SHARED_MEM_ACCESS_PERM);        /* access permissions (0644) */
        // if (fdSharedMemory < 0) 
        // {
        //         fprintf(stderr, "Can't open shared mem segment...");
        //         exit(EXIT_FAIL);
        // }

        // ftruncate(fdSharedMemory, sizeof(OutputBlock));           /* set size */

        // // Get shared memory block address
        // caddr_t outputBlockPtr = mmap(
        //                         NULL,                           /* let system pick where to put segment */
        //                         sizeof(OutputBlock),            /* how many bytes */
        //                         PROT_READ | PROT_WRITE,         /* access protections */
        //                         MAP_SHARED,                     /* mapping visible to other processes */
        //                         fdSharedMemory,                 /* file descriptor */
        //                         0);                             /* offset: start at 1st byte */

        // if (MAP_FAILED == outputBlockPtr)
        // {
        //         fprintf(stderr, "Can't get segment...");
        //         exit(EXIT_FAIL);
        // }

        // fprintf(stderr, "shared mem address: %p [0..%ld]\n", outputBlockPtr, sizeof(OutputBlock) - 1);
        // fprintf(stderr, "backing file:       /dev/shm%s\n", SHARED_MEM_BACKING_FILE );

	OutputBlock o;
	bzero(&o, sizeof(OutputBlock));

        // semaphore code to lock the shared mem 
        sem_t* semptr = sem_open(
                                MERCURY_SEMAPHORE,              /* name */
                                O_CREAT,                        /* create the semaphore */
                                MERCURY_ACCESS_PERM,            /* protection perms */
                                1);                             /* initial value */
        if (SEM_FAILED == semptr)
        {
                fprintf(stderr, "Semaphore open error.");
                exit(EXIT_FAIL);      
        }

        // Open RS485 dongle
        // O_RDWR Read/Write access to serial port
        // O_NOCTTY - No terminal will control the process  
        // O_NDELAY - Non blocking open
        int RS485 = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);

        if (RS485 < 0)
        {
                fprintf(stderr, "Cannot open %s terminal channel.\n\r", dev);
                exit(EXIT_FAIL);
        }

        fcntl(RS485, F_SETFL, 0);

        struct termios serialPortSettings;
        bzero(&serialPortSettings, sizeof(serialPortSettings));

        cfsetispeed(&serialPortSettings, BAUDRATE);
        cfsetospeed(&serialPortSettings, BAUDRATE);

        serialPortSettings.c_cflag &= PARENB;				/* Disables the Parity Enable bit(PARENB),So No Parity   */
        serialPortSettings.c_cflag &= ~CSTOPB;				/* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
        serialPortSettings.c_cflag &= ~CSIZE;				/* Clears the mask for setting the data size             */
        serialPortSettings.c_cflag |= CS8;				/* Set the data bits = 8                                 */

        serialPortSettings.c_cflag |= CREAD | CLOCAL;			/* Enable receiver,Ignore Modem Control lines       */ 

        serialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);		/* Disable XON/XOFF flow control both i/p and o/p */
        serialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);	/* Non Cannonical mode                            */

        serialPortSettings.c_oflag &= ~OPOST;				/* No Output Processing */

        tcflush(RS485, TCIOFLUSH);
        tcsetattr(RS485, TCSANOW, &serialPortSettings);

        //debugPrint = 1;

        int exitCode = 0;
        switch(checkChannel(RS485))
        {
                case OK:
                        do
                        {
                                int loopStatus = OK;
                                /* wait until semaphore != 0 */
                                if (!sem_wait(semptr))
                                {
                                        loopStatus =
                                                initConnection(RS485) +
                                                
                                                // getU(RS485, &o.U) +    // Get voltage by phases
                                                // getI(RS485, &o.I) +    // Get current by phases
                                                // getCosF(RS485, &o.C) + // Get power cos(f) by phases
                                                // getF(RS485, &o.f) +    // Get grid frequency 
                                                // getA(RS485, &o.A) +    // Get phase angles
                                                // getP(RS485, &o.P) +    // Get active power consumption by phases

                                                // Only poll for power consumption
                                                getS(RS485, &o.S) +    // Get reactive power consumption by phases

                                                // // Get power counter from reset, for yesterday and today
                                                // getW(RS485, &o.PR, PP_RESET, 0, 0) +        // total from reset
                                                // getW(RS485, &o.PRT[0], PP_RESET, 0, 0+1) +  // day tariff from reset
                                                // getW(RS485, &o.PRT[1], PP_RESET, 0, 1+1) +  // night tariff from reset
                                                // getW(RS485, &o.PY, PP_YESTERDAY, 0, 0) + 
                                                // getW(RS485, &o.PT, PP_TODAY, 0, 0) +

                                                closeConnection(RS485);
                                        
                                        // increment semaphore to let other processes go
                                        sem_post(semptr);
                                }        

                                printf((OK == loopStatus)
                                        ? "Successfull run, current power consumption: %8.2fW\n\r"
                                        : "One or more errors occurred during data collection.\n\r", o.S.sum);

                                usleep(5 * 1000 * 1000); // 5 sec

                        } while (!terminateMonitorNow);
                        
                        printf("Monitor terminated successfully.\n\r");
                        exitCode = EXIT_OK;
                        break;

                case CHECK_CHANNEL_FAILURE:

                        printf("Power meter channel time out.\n\r");
                        exitCode = EXIT_FAIL;
                        break;

                default:

                        printf("Power meter communication channel test failed.\n\r");
                        exitCode = EXIT_FAIL;
                        break;
	}

        // Clean up
        // munmap(outputBlockPtr, sizeof(OutputBlock)); /* unmap the storage */
        close(RS485);
        sem_close(semptr);
 
        exit(exitCode);
}
