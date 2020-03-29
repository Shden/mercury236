/*
 *      Mercury power meter monitoring application. The application is designed
 *      to stay active and regularly poll data from the power meter.
 * 
 *      Installation as a service on RPi:
 * 
 *      1. Create service file
 *      $ sudo nano /etc/systemd/system/mercury-mon.service
 * 
 *      Description=mercury-mon
 * 
 *      Wants=network.target
 *      After=syslog.target network-online.target
 * 
 *      [Service]
 *      Type=simple
 *      ExecStart=/home/den/Shden/mercury236/mercury-mon /dev/ttyUSB0 17250
 *      Restart=on-failure
 *      RestartSec=10
 *      KillMode=process
 * 
 *      [Install]
 *      WantedBy=multi-user.target
 * 
 *      2. Reload services:
 *      $ sudo systemctl daemon-reload
 * 
 *      3. Enable the service:
 *      $ sudo systemctl enable mercury-mon
 * 
 *      4. Start the service:
 *      $ sudo systemctl start
 * 
 *      5. Check the status of service:
 *      $ systemctl status mercury-mon
 */
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <semaphore.h>
#include <syslog.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "mercury236.h"

#define BSZ	                255
#define OPT_DEBUG		"--debug"
#define OPT_HELP		"--help"
#define POLL_TIME               5 // seconds

int debugPrint = 0;

typedef enum
{
	EXIT_OK = 0,
	EXIT_FAIL = -1
} ExitCode;

// -- Command line usage help
void printUsage()
{
	printf("Usage: mercury-mon [RS485] [MaxPower]\n\r\n\r");
	printf("  RS485\t\taddress of RS485 dongle (e.g. /dev/ttyUSB0), required.\n\r");
        printf("  MaxPower\tpower (Watt) allowed, if this value exceeded, monitor calls script to deactivate some consumers.");
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

// -- All checks for current power go here
void handleConsumptionUpdate(float currentPower, float maxPower)
{
        if (currentPower > maxPower)
        {
                syslog(LOG_NOTICE, "Maximum power exceeded (%8.2fW).\n\r", currentPower);
                FILE* f = fopen("/home/den/Shden/appliances/mainHeater", "w");
                if (NULL != f)
                {
                        fputc('0', f);
                        fclose(f);
                        syslog(LOG_NOTICE, "Main heater turned off.\n\r");
                }
        }
}

int main(int argc, const char** args)
{
        openlog(NULL, LOG_PID, LOG_DAEMON);

	// must have RS485 address (1st required param)
	if (argc < 2)
	{
		syslog(LOG_NOTICE, "Error: no RS485 device specified.\n\r");
		printUsage();
                closelog();
		exit(EXIT_FAIL);
	}

        // must have max power (2nd requred param)
        if (argc < 3)
        {
		syslog(LOG_NOTICE, "Error: max power specified.\n\r");
		printUsage();
                closelog();
		exit(EXIT_FAIL);
        }

        // Ctrl+C handler
        signal(SIGINT, sigint_handler);

        // get RS485 device specification
	char dev[BSZ];
	strncpy(dev, args[1], BSZ);

        // get maximum allowed power
        int maxPower = strtol(args[2], NULL, 10);
        if (maxPower < 100 || maxPower > 30000)
        {
                syslog(LOG_NOTICE, "Error: maximum power (%d) is out of the range (100..30000).\n\r", maxPower);
                closelog();
                exit(EXIT_FAIL);
        }

	// get command line options
	for (int i=3; i<argc; i++)
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
                        closelog();
			exit(EXIT_OK);
		}
		else
		{
			syslog(LOG_NOTICE, "Error: %s option is not recognised\n\r", args[i]);
			printUsage();
                        closelog();
			exit(EXIT_FAIL);
		}
	}

 	OutputBlock o;
	bzero(&o, sizeof(OutputBlock));

        // semaphore code to lock the shared mem 
        int prevMask = umask(0000);
        sem_t* semptr = sem_open(
                                MERCURY_SEMAPHORE,              /* name */
                                O_CREAT,                        /* create the semaphore */
                                MERCURY_ACCESS_PERM,            /* protection perms */
                                1);                             /* initial value */
        umask(prevMask);
        if (SEM_FAILED == semptr)
        {
                syslog(LOG_NOTICE, "Semaphore open error.");
                closelog();
                exit(EXIT_FAIL);      
        }

        // Open RS485 dongle
        // O_RDWR Read/Write access to serial port
        // O_NOCTTY - No terminal will control the process  
        // O_NDELAY - Non blocking open
        int RS485 = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);

        if (RS485 < 0)
        {
                syslog(LOG_NOTICE, "Cannot open %s terminal channel.\n\r", dev);
                closelog();
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

        int exitCode = 0;
        int resCheckChannel = CHECK_CHANNEL_FAILURE;

        if (!sem_wait(semptr))
        {
                resCheckChannel = checkChannel(RS485);
                sem_post(semptr);
        }

        int loopCount = 0;
        switch(resCheckChannel)
        {
                case OK:
                        do
                        {
                                loopCount++;
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

                                if (loopCount > 20)
                                {
                                        loopCount = 0;                                        
                                        syslog(LOG_NOTICE, (OK == loopStatus)
                                                ? "Current power consumption: %8.2fW\n\r"
                                                : "One or more errors occurred during data collection.\n\r", o.S.sum);
                                
                                }
                                // run all checks for the obtained power value
                                handleConsumptionUpdate(o.S.sum, maxPower);

                                usleep(POLL_TIME * 1000 * 1000); // 5 sec

                        } while (!terminateMonitorNow);
                        
                        printf("Monitor terminated successfully.\n\r");
                        exitCode = EXIT_OK;
                        break;

                case CHECK_CHANNEL_FAILURE:

                        syslog(LOG_NOTICE, "Power meter channel time out.\n\r");
                        exitCode = EXIT_FAIL;
                        break;

                default:

                        syslog(LOG_NOTICE, "Power meter communication channel test failed.\n\r");
                        exitCode = EXIT_FAIL;
                        break;
	}

        // Clean up
        // munmap(outputBlockPtr, sizeof(OutputBlock)); /* unmap the storage */
        close(RS485);
        sem_close(semptr);
 
        closelog();
        exit(exitCode);
}
