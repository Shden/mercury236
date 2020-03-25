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
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "mercury236.h"

#define BSZ	255

int debugPrint = 1;

typedef enum
{
	EXIT_OK = 0,
	EXIT_FAIL = -1
} ExitCode;

// -- Command line usage help
void printUsage()
{
	printf("Usage: mercury-mon [RS485]\n\r\n\r");
	printf("  RS485\t\taddress of RS485 dongle (e.g. /dev/ttyUSB0), required\n\r");
        printf("\n\r\n\rPress Ctrl+C to exit.");
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

	// // get command line options
	// int dryRun = 0, format = OF_HUMAN, header = 0; 

	// for (int i=2; i<argc; i++)
	// {
	// 	if (!strcmp(OPT_DEBUG, args[i]))
	// 		debugPrint = 1;
	// 	else if (!strcmp(OPT_TEST_RUN, args[i]))
	// 		dryRun = 1;
	// 	else if (!strcmp(OPT_HUMAN, args[i]))
	// 		format = OF_HUMAN;
	// 	else if (!strcmp(OPT_CSV, args[i]))
	// 		format = OF_CSV;
	// 	else if (!strcmp(OPT_JSON, args[i]))
	// 		format = OF_JSON;
	// 	else if (!strcmp(OPT_HEADER, args[i]))
	// 		header = 1;
	// 	else if (!strcmp(OPT_HELP, args[i]))
	// 	{
	// 		printUsage();
	// 		exit(EXIT_OK);
	// 	}
	// 	else
	// 	{
	// 		printf("Error: %s option is not recognised\n\r\n\r", args[i]);
	// 		printUsage();
	// 		exit(EXIT_FAIL);
	// 	}
	// }

	OutputBlock o;
	bzero(&o, sizeof(o));

        // Open RS485 dongle
        // O_RDWR Read/Write access to serial port
        // O_NOCTTY - No terminal will control the process  
        // O_NDELAY - Non blocking open
        int fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);

        if (fd < 0)
        {
                printf("Cannot open %s terminal channel.\n\r", dev);
                exit(EXIT_FAIL);
        }

        fcntl(fd, F_SETFL, 0);

        struct termios serialPortSettings;
        bzero(&serialPortSettings, sizeof(serialPortSettings));

        cfsetispeed(&serialPortSettings, BAUDRATE);
        cfsetospeed(&serialPortSettings, BAUDRATE);

        serialPortSettings.c_cflag &= PARENB;				/* Disables the Parity Enable bit(PARENB),So No Parity   */
        serialPortSettings.c_cflag &= ~CSTOPB;				/* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
        serialPortSettings.c_cflag &= ~CSIZE;				/* Clears the mask for setting the data size             */
        serialPortSettings.c_cflag |=  CS8;				/* Set the data bits = 8                                 */

        serialPortSettings.c_cflag |= CREAD | CLOCAL;			/* Enable receiver,Ignore Modem Control lines       */ 

        serialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);		/* Disable XON/XOFF flow control both i/p and o/p */
        serialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);	/* Non Cannonical mode                            */

        serialPortSettings.c_oflag &= ~OPOST;				/* No Output Processing */

        tcflush(fd, TCIOFLUSH);
        tcsetattr(fd, TCSANOW, &serialPortSettings);

        //debugPrint = 1;

        switch(checkChannel(fd))
        {
                case OK:
                        do
                        {
                                int loopStatus =
                                        initConnection(fd) +
                                        
                                        getU(fd, &o.U) +        // Get voltage by phases
                                        getI(fd, &o.I) +        // Get current by phases
                                        getCosF(fd, &o.C) +     // Get power cos(f) by phases
                                        getF(fd, &o.f) +        // Get grid frequency 
                                        getA(fd, &o.A) +        // Get phase angles
                                        getP(fd, &o.P) +        // Get active power consumption by phases
                                        getS(fd, &o.S) +        // Get reactive power consumption by phases

                                        // Get power counter from reset, for yesterday and today
                                        getW(fd, &o.PR, PP_RESET, 0, 0) +        // total from reset
                                        getW(fd, &o.PRT[0], PP_RESET, 0, 0+1) +  // day tariff from reset
                                        getW(fd, &o.PRT[1], PP_RESET, 0, 1+1) +  // night tariff from reset
                                        getW(fd, &o.PY, PP_YESTERDAY, 0, 0) + 
                                        getW(fd, &o.PT, PP_TODAY, 0, 0) +

                                        closeConnection(fd);

                                printf((OK == loopStatus)
                                        ? "Successfull power meter data collection cycle.\n\r"
                                        : "One or more errors occurred during data collection.\n\r");

                                usleep(5 * 1000 * 1000); // 5 sec

                        } while (!terminateMonitorNow);
                        
                        printf("Power meter monitor terminated.\n\r");
                        close(fd);
                        exit(EXIT_OK);

                case CHECK_CHANNEL_FAILURE:
                        close(fd);
                        printf("Power meter channel time out.\n\r");
                        exit(EXIT_FAIL);

                default:
                        close(fd);
                        printf("Power meter communication channel test failed.\n\r");
                        exit(EXIT_FAIL);
	}
}