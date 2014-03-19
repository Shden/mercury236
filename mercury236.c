#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BAUDRATE B9600
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
        
volatile int STOP=FALSE; 

int main()
{
	int fd, res, c;
	struct termios oldtio, newtio;
	char buf[255];

	fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY); 
	if (fd < 0) { perror(MODEMDEVICE); exit(-1); }

	tcgetattr(fd,&oldtio); /* save current port settings */

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;
 
	newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 chars received */

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	/* connection initialisation & password */
	sprintf(buf, "#00#01#01#01#01#01#01#01#01#77#81");
	write(fd, buf, strlen(buf));

	/* read whatever is returned, just 1 line */
	c = 0;
	while (STOP==FALSE) 
	{       			
		res = read(fd, &buf[c], 255-c);   	/* returns after 1 chars have been input */
		if (buf[0] == '\r' || buf[0] == '\n') STOP=TRUE;
		c++;
	}
	buf[c]='\0';               			/* so we can printf... */
	printf(":%s:%d\n", buf, res);
	
	close(fd);
	tcsetattr(fd, TCSANOW, &oldtio);
	exit(0);
}
    