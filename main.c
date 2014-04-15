#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BAUDRATE B9600
#define MODEMDEVICE "/dev/ttyUSB0"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define UInt16		unsigned int
#define byte		char

volatile int STOP=FALSE;

// Compute the MODBUS RTU CRC
// Source: http://www.ccontrolsys.com/w/How_to_Compute_the_Modbus_RTU_Message_CRC
UInt16 ModRTU_CRC(byte* buf, int len)
{
  UInt16 crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (UInt16)buf[pos];          // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;  
}

int main()
{
	int fd, res, c;
	struct termios oldtio, newtio;
	char buf[255];

printf("3Before init sent");
	fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY);
	if (fd < 0) { perror(MODEMDEVICE); exit(-1); }

	tcgetattr(fd, &oldtio); /* save current port settings */
printf("2Before init sent\n\r");

	bzero(&newtio, sizeof(newtio));
//	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_cflag = BAUDRATE | CS8 | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;

	newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 chars received */

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	/* connection initialisation & password */
printf("1Before init sent\n\r");
	char initCmd[] = { 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x77, 0x81 };

	UInt16 crc =  ModRTU_CRC(initCmd, 9);
	printf("CRC: %x\n\r", crc);
	exit(0);

//	char initCmd[] = { 0x00, 0x01, '1', '1', '1', '1', '1', '1', 0x01, 0x77, 0x81 };
printf("Before init sent\n\r");
	write(fd, initCmd, sizeof(initCmd));
printf("Init sent\n\r");

	/* read whatever is returned, just 1 line */
	c = 0;
	while (STOP==FALSE)
	{
		res = read(fd, &buf[0], 255-c);   	/* returns after 1 chars have been input */
	printf(":%x:%d\n", buf[0], res);
		if (buf[0] == '\r' || buf[0] == '\n') STOP=TRUE;
		c++;
	}
	buf[c]='\0';               			/* so we can printf... */
	printf(":%s:%d\n", buf, res);

	close(fd);
	tcsetattr(fd, TCSANOW, &oldtio);
	exit(0);
}

