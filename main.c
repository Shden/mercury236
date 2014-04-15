#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#pragma pack(1)
#define BAUDRATE 	B9600		// 9600 baud
#define MODEMDEVICE 	"/dev/ttyUSB0"	// Dongle device
#define _POSIX_SOURCE 	1 /* POSIX compliant source */
#define FALSE 		0
#define TRUE 		1
#define UInt16		uint16_t
#define byte		unsigned char
#define TIME_OUT	50		// Mercury inter-command delay (ms)
#define BSZ		255
#define PM_ADDRESS	0		// RS485 addess of the power meter

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

void printPackage(byte *data, int size, int isin)
{
	printf("%s bytes: %d\n\r\t", (isin) ? "Received" : "Sent", size);
	for (int i=0; i<size; i++)
		printf("%02X ", (byte)data[i]);
	printf("\n\r");
}


// ***** Commands
// Test connecion
typedef struct TestCmd
{
	byte	address;
	byte	command;
	UInt16	CRC;
} TestCmd;

// 1-byte responce (usually with status code)
typedef struct Result_1b
{
	byte	address;
	byte	result;
	UInt16	CRC;
} Result_1b;

// Connection initialisaton command
typedef struct InitCmd
{
	byte	address;
	byte	command;
	byte 	accessLevel;
	byte	password[6];
	UInt16	CRC;
} InitCmd;

// Connecion terminaion command
typedef struct ByeCmd
{
	byte	address;
	byte	command;
	UInt16	CRC;
} ByeCmd;

// Power meter parameters read command
typedef struct ReadParamCmd
{
	byte	address;
	byte	command;	// 8h
	byte	paramId;	// No of parameter to read
	byte	BWRI;
	UInt16 	CRC;
} ReadParamCmd;

// Result with 3 bytes per phase
typedef struct Result_3x3b
{
	byte	address;
	byte	p1[3];
	byte	p2[3];
	byte	p3[3];
	UInt16	CRC;
} Result_3x3b;

// 3-phase vector (for voltage, frequency, power by phases)
typedef struct P3V
{
	float	p1;
	float	p2;
	float	p3;
} P3V;

// **** Enums
typedef enum Direction
{
	OUT = 0,
	IN = 1
} Direction;

typedef enum ResultCode
{
	OK = 0,
	ILLEGAL_CMD = 1,
	INTERNAL_COUNTER_ERR = 2,
	PERMISSION_DENIED = 3,
	CLOCK_ALREADY_CORRECTED = 4,
	CHANNEL_ISNT_OPEN = 5,
	WRONG_RESULT_SIZE = 256,
	WRONG_CRC = 257
} ResultCode;

typedef enum ExitCode
{
	EXIT_OK = 0,
	EXIT_FAIL = 1
} ExitCode;

// -- Check the responce
int checkResult_1b(byte* buf, int len)
{
	if (len != sizeof(Result_1b))
		return WRONG_RESULT_SIZE;

	Result_1b *res = (Result_1b*)buf;
	UInt16 crc = ModRTU_CRC((byte*)res, sizeof(res) - sizeof(UInt16));
	if (crc != res->CRC)
		return WRONG_CRC;

	return res->result & 0x0F;
}

// -- Check the communication channel
int checkChannel(int ttyd)
{
	// Command initialisation
	TestCmd testCmd = { .address = PM_ADDRESS, .command = 0x00 };
	testCmd.CRC = ModRTU_CRC((byte*)&testCmd, sizeof(testCmd) - sizeof(UInt16));
	printPackage((byte*)&testCmd, sizeof(testCmd), OUT);

	// Send test channel command
	write(ttyd, (byte*)&testCmd, sizeof(testCmd));
	usleep(TIME_OUT);

	// Get responce
	char buf[BSZ];
	int len = read(ttyd, buf, BSZ);
	printPackage((byte*)buf, len, IN);

	return checkResult_1b(buf, len);
}

// -- Connection initialisation
int initConnection(int ttyd)
{
	InitCmd initCmd = {
		.address = PM_ADDRESS,
		.command = 0x01,
		.accessLevel = 0x01,
		.password = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
	};
	initCmd.CRC = ModRTU_CRC((byte*)&initCmd, sizeof(initCmd) - sizeof(UInt16));
	printPackage((byte*)&initCmd, sizeof(initCmd), OUT);

	write(ttyd, (byte*)&initCmd, sizeof(initCmd));
	usleep(TIME_OUT);

	// Read initialisation result
	char buf[BSZ];
	int len = read(ttyd, buf, BSZ);
	printPackage((byte*)buf, len, IN);

	return checkResult_1b(buf, len);
}

// -- Close connection
int closeConnection(int ttyd)
{
	ByeCmd byeCmd = { .address = PM_ADDRESS, .command = 0x02 };
	byeCmd.CRC = ModRTU_CRC((byte*)&byeCmd, sizeof(byeCmd) - sizeof(UInt16));
	printPackage((byte*)&byeCmd, sizeof(byeCmd), OUT);

	write(ttyd, (byte*)&byeCmd, sizeof(byeCmd));
	usleep(TIME_OUT);

	// Read closing responce
	char buf[BSZ];
	int len = read(ttyd, buf, BSZ);
	printPackage((byte*)buf, len, IN);

	return checkResult_1b(buf, len);
}

float B3F(byte b[3], float factor)
{
	int val = (b[0] << 16) | (b[2] << 8) | b[1];
	return val/factor;
}

// Get current voltage (U) by phases
int getU(int ttyd, P3V* U)
{
	ReadParamCmd getUCmd =
	{
		.address = PM_ADDRESS,
		.command = 0x08,
		.paramId = 0x16,
		.BWRI = 0x11
	};
	getUCmd.CRC = ModRTU_CRC((byte*)&getUCmd, sizeof(getUCmd) - sizeof(UInt16));
	printPackage((byte*)&getUCmd, sizeof(getUCmd), OUT);

	write(ttyd, (byte*)&getUCmd, sizeof(getUCmd));
	usleep(TIME_OUT);

	// Read closing responce
	char buf[BSZ];
	int len = read(ttyd, buf, BSZ);
	printPackage((byte*)buf, len, IN);

	Result_3x3b* res = (Result_3x3b*)buf;
	U->p1 = B3F(res->p1, 100.0);
	U->p2 = B3F(res->p2, 100.0);
	U->p3 = B3F(res->p3, 100.0);

// TBD check & extract
	return OK;
}


int main()
{
	int fd, res, c;
	struct termios oldtio, newtio;
	char buf[255];

	// Open RS485 dongle
	fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NDELAY );
	if (fd < 0)
	{
		perror(MODEMDEVICE);
		exit(EXIT_FAIL);
	}
	fcntl(fd, F_SETFL, 0);

	tcgetattr(fd, &oldtio); /* save current port settings */

	bzero(&newtio, sizeof(newtio));

	cfsetispeed(&newtio, BAUDRATE);
	cfsetospeed(&newtio, BAUDRATE);

	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
//	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
//	newtio.c_cflag = BAUDRATE | CS8 | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	cfmakeraw(&newtio);
	tcsetattr(fd, TCSANOW, &newtio);

	if (OK != checkChannel(fd))
	{
		printf("Power meter communication channel test failed.\n\r");
		exit(EXIT_FAIL);
	}

	if (OK != initConnection(fd))
	{
		printf("Power meter connection initialisation error.\n\r");
		exit(EXIT_FAIL);
	}

	// Get voltage by phases
	P3V U;
	if (OK != getU(fd, &U))
	{
		printf("Cannot collect voltage data.\n\r");
		exit(EXIT_FAIL);
	}
printf("U: %f * %f * %f\n\r", U.p1, U.p2, U.p3);

	if (OK != closeConnection(fd))
	{
		printf("Power meter connection closing error.\n\r");
		exit(EXIT_FAIL);
	}

	close(fd);
	tcsetattr(fd, TCSANOW, &oldtio);
	exit(EXIT_OK);
}

