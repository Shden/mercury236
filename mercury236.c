/*
 *	Mercury 236 power meter communication library.
 *
 *	RS485 USB dongle is used to connect to the power meter and to collect grid power measures
 *	including voltage, current, consumption power, counters, cos(f) etc.
 *
 * 	Protocol documentation: https://www.incotexcom.ru/files/em/docs/mercury-protocol-obmena-9-2019.pdf
 */
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include "mercury236.h"

#define BSZ			255

// **** Debug output globals
int debugPrint;

// **** Enums
typedef enum
{
	OUT = 0,
	IN = 1
} Direction;

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

// -- Print out data buffer in hex
void printPackage(byte *data, int size, int isin)
{
	if (debugPrint)
	{
		printf("%s bytes: %d\n\r\t", (isin) ? "Received" : "Sent", size);
		for (int i=0; i<size; i++)
			printf("%02X ", (byte)data[i]);
		printf("\n\r");
	}
}

// -- Print out error code
void printError(int code)
{
	if (debugPrint)
		printf("Error received: %d\n\r", code);
}

/* -- Non-blocking file read with timeout
 *
 *    Returns: 
 *	0 if timed out.
 *	< 0 if select error
 *	number of bytes read if success
 */
int nb_read(int fd, byte* buf, int sz)
{
	fd_set set;
	struct timeval timeout;

	// Initialise the input set
	FD_ZERO(&set);
	FD_SET(fd, &set);

	// Set timeout
	timeout.tv_sec = CH_TIME_OUT;
	timeout.tv_usec = 0;

	int r = select(fd + 1, &set, NULL, NULL, &timeout);
	if (r > 0)
		return read(fd, buf, sz);
	else 
		return r;
}

// -- Check 1 byte responce
int checkResult_1b(byte* buf, int len)
{
	if (len != sizeof(Result_1b))
		return WRONG_RESULT_SIZE;

	Result_1b *res = (Result_1b*)buf;
	UInt16 crc = ModRTU_CRC(buf, len - sizeof(UInt16));
	if (crc != res->CRC)
		return WRONG_CRC;

	return OK; // res->result & 0x0F;
}

// -- Check 3 byte responce
int checkResult_3b(byte* buf, int len)
{
	if (len != sizeof(Result_3b))
		return WRONG_RESULT_SIZE;

	Result_3b *res = (Result_3b*)buf;
	UInt16 crc = ModRTU_CRC(buf, len - sizeof(UInt16));
	if (crc != res->CRC)
		return WRONG_CRC;

	return OK;
}

// -- Check 3 bytes x 3 phase responce
int checkResult_3x3b(byte* buf, int len)
{
	if (len != sizeof(Result_3x3b))
		return WRONG_RESULT_SIZE;

	Result_3x3b *res = (Result_3x3b*)buf;
	UInt16 crc = ModRTU_CRC(buf, len - sizeof(UInt16));
	if (crc != res->CRC)
		return WRONG_CRC;

	return OK;
}

// -- Check 3 bytes x 3 phase and sum responce
int checkResult_4x3b(byte* buf, int len)
{
	if (len != sizeof(Result_4x3b))
		return WRONG_RESULT_SIZE;

	Result_4x3b *res = (Result_4x3b*)buf;
	UInt16 crc = ModRTU_CRC(buf, len - sizeof(UInt16));
	if (crc != res->CRC)
		return WRONG_CRC;

	return OK;
}

// -- Check 4 bytes x 3 phase and sum responce
int checkResult_4x4b(byte* buf, int len)
{
	if (len != sizeof(Result_4x4b))
		return WRONG_RESULT_SIZE;

	Result_4x4b *res = (Result_4x4b*)buf;
	UInt16 crc = ModRTU_CRC(buf, len - sizeof(UInt16));
	if (crc != res->CRC)
		return WRONG_CRC;

	return OK;
}

/* 
 * Sends command and receives responce, one attempt.
 *
 * Returns:
 * 	> 0 - nuber of bytes received
 * 	<= 0 - error occured
 */
int sendReceive(int ttyd, byte* commandBuff, int commandLen,
	byte* responceBuff, int responceBuffSize)
{
	printPackage(commandBuff, commandLen, OUT);

	// Send command
	write(ttyd, commandBuff, commandLen);
	usleep(TIME_OUT);

	// Get responce
	int len = nb_read(ttyd, responceBuff, responceBuffSize);
	if (len)
		printPackage(responceBuff, len, IN);
	else
		printError(len);
	return len;
}

/*
 * Check the communication channel.
 * 
 * Returns:
 * 	CHECK_CHANNEL_FAILURE - channel doesnt respond as expected.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int checkChannel(int ttyd)
{
	// Command initialisation
	TestCmd testCmd = { .address = PM_ADDRESS, .command = 0x00 };
	testCmd.CRC = ModRTU_CRC((byte*)&testCmd, sizeof(testCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&testCmd, sizeof(testCmd), buf, BSZ);
	if (len)
		return checkResult_1b(buf, len);

	return CHECK_CHANNEL_FAILURE;
}

/*
 * Initialise connection with power meter.
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int initConnection(int ttyd)
{
	InitCmd initCmd = {
		.address = PM_ADDRESS,
		.command = 0x01,
		.accessLevel = 0x01,
		.password = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
	};
	initCmd.CRC = ModRTU_CRC((byte*)&initCmd, sizeof(initCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&initCmd, sizeof(initCmd), buf, BSZ);
	if (len)
		return checkResult_1b(buf, len);
	
	return COMMUNICATION_ERROR;
}

/*
 * Finalise connection to power meter.
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int closeConnection(int ttyd)
{
	ByeCmd byeCmd = { .address = PM_ADDRESS, .command = 0x02 };
	byeCmd.CRC = ModRTU_CRC((byte*)&byeCmd, sizeof(byeCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&byeCmd, sizeof(byeCmd), buf, BSZ);
	if (len)
		return checkResult_1b(buf, len);

	return COMMUNICATION_ERROR;
}

// Decode float from 3 bytes
float B3F(byte b[3], float factor)
{
	int val = ((b[0] & 0x3F) << 16) | (b[2] << 8) | b[1];
	return val/factor;
}

// Decode float from 4 bytes
float B4F(byte b[4], float factor)
{
	int val = ((b[1] & 0x3F) << 24) | (b[0] << 16) | (b[3] << 8) | b[2];
	return val/factor;
}

/* 
 * Get voltage (U) by phases.
 *
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
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

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getUCmd, sizeof(getUCmd), buf, BSZ);

	if (len)
	{
		// Check and decode result
		int checkResult = checkResult_3x3b(buf, len);
		if (OK == checkResult)
		{
			Result_3x3b* res = (Result_3x3b*)buf;
			U->p1 = B3F(res->p1, 100.0);
			U->p2 = B3F(res->p2, 100.0);
			U->p3 = B3F(res->p3, 100.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}

/*
 * Get current (I) by phases.
 *
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int getI(int ttyd, P3V* I)
{
	ReadParamCmd getICmd =
	{
		.address = PM_ADDRESS,
		.command = 0x08,
		.paramId = 0x16,
		.BWRI = 0x21
	};
	getICmd.CRC = ModRTU_CRC((byte*)&getICmd, sizeof(getICmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getICmd, sizeof(getICmd), buf, BSZ);

	if (len)
	{	
		// Check and decode result
		int checkResult = checkResult_3x3b(buf, len);
		if (OK == checkResult)
		{
			Result_3x3b* res = (Result_3x3b*)buf;
			I->p1 = B3F(res->p1, 1000.0);
			I->p2 = B3F(res->p2, 1000.0);
			I->p3 = B3F(res->p3, 1000.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}

/*
 * Get power consumption factor cos(f) by phases.
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int getCosF(int ttyd, P3VS* C)
{
	ReadParamCmd getCosCmd =
	{
		.address = PM_ADDRESS,
		.command = 0x08,
		.paramId = 0x16,
		.BWRI = 0x30
	};
	getCosCmd.CRC = ModRTU_CRC((byte*)&getCosCmd, sizeof(getCosCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getCosCmd, sizeof(getCosCmd), buf, BSZ);

	if (len)
	{
		// Check and decode result
		int checkResult = checkResult_4x3b(buf, len);
		if (OK == checkResult)
		{
			Result_4x3b* res = (Result_4x3b*)buf;
			C->p1 = B3F(res->p1, 1000.0);
			C->p2 = B3F(res->p2, 1000.0);
			C->p3 = B3F(res->p3, 1000.0);
			C->sum = B3F(res->sum, 1000.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}

/*
 * Get grid frequency (Hz).
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int getF(int ttyd, float *f)
{
	ReadParamCmd getFCmd =
	{
		.address = PM_ADDRESS,
		.command = 0x08,
		.paramId = 0x16,
		.BWRI = 0x40
	};
	getFCmd.CRC = ModRTU_CRC((byte*)&getFCmd, sizeof(getFCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getFCmd, sizeof(getFCmd), buf, BSZ);

	if (len)
	{
		// Check and decode result
		int checkResult = checkResult_3b(buf, len);
		if (OK == checkResult)
		{
			Result_3b* res = (Result_3b*)buf;
			*f = B3F(res->res, 100.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}

/*
 * Get phases angle.
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int getA(int ttyd, P3V* A)
{
	ReadParamCmd getACmd =
	{
		.address = PM_ADDRESS,
		.command = 0x08,
		.paramId = 0x16,
		.BWRI = 0x51
	};
	getACmd.CRC = ModRTU_CRC((byte*)&getACmd, sizeof(getACmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getACmd, sizeof(getACmd), buf, BSZ);

	if (len)
	{
		// Check and decode result
		int checkResult = checkResult_3x3b(buf, len);
		if (OK == checkResult)
		{
			Result_3x3b* res = (Result_3x3b*)buf;
			A->p1 = B3F(res->p1, 100.0);
			A->p2 = B3F(res->p2, 100.0);
			A->p3 = B3F(res->p3, 100.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}

/*
 * Get active power (W) consumption by phases with total.
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int getP(int ttyd, P3VS* P)
{
	ReadParamCmd getPCmd =
	{
		.address = PM_ADDRESS,
		.command = 0x08,
		.paramId = 0x16,
		.BWRI = 0x00
	};
	getPCmd.CRC = ModRTU_CRC((byte*)&getPCmd, sizeof(getPCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getPCmd, sizeof(getPCmd), buf, BSZ);

	if (len)
	{
		// Check and decode result
		int checkResult = checkResult_4x3b(buf, len);
		if (OK == checkResult)
		{
			Result_4x3b* res = (Result_4x3b*)buf;
			P->p1 = B3F(res->p1, 100.0);
			P->p2 = B3F(res->p2, 100.0);
			P->p3 = B3F(res->p3, 100.0);
			P->sum = B3F(res->sum, 100.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}

/*
 * Get reactive power (VA) consumption by phases with total.
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int getS(int ttyd, P3VS* S)
{
	ReadParamCmd getSCmd =
	{
		.address = PM_ADDRESS,
		.command = 0x08,
		.paramId = 0x16,
		.BWRI = 0x08
	};
	getSCmd.CRC = ModRTU_CRC((byte*)&getSCmd, sizeof(getSCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getSCmd, sizeof(getSCmd), buf, BSZ);

	if (len)
	{
		// Check and decode result
		int checkResult = checkResult_4x3b(buf, len);
		if (OK == checkResult)
		{
			Result_4x3b* res = (Result_4x3b*)buf;
			S->p1 = B3F(res->p1, 100.0);
			S->p2 = B3F(res->p2, 100.0);
			S->p3 = B3F(res->p3, 100.0);
			S->sum = B3F(res->sum, 100.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}

/*
 * Get power counters by phases for the period.
 * 
 * Parameters:
 * 	periodId - one of PowerPeriod enum values
 *	month - month number when periodId is PP_MONTH
 *	tariffNo - 0 for all tariffs, 1 - tariff #1, 2 - tariff #2 etc.
 * 
 * Returns:
 *	COMMUNICATION_ERROR - unable to get responce from tty.
 * 	WRONG_CRC - data recieved but CRC check failed.
 * 	OK - means ok.
 */
int getW(int ttyd, PWV* W, int periodId, int month, int tariffNo)
{
	ReadParamCmd getWCmd =
	{
		.address = PM_ADDRESS,
		.command = 0x05,
		.paramId = (periodId << 4) | (month & 0xF),
		.BWRI = tariffNo
	};
	getWCmd.CRC = ModRTU_CRC((byte*)&getWCmd, sizeof(getWCmd) - sizeof(UInt16));

	byte buf[BSZ];
	int len = sendReceive(ttyd, (byte*)&getWCmd, sizeof(getWCmd), buf, BSZ);

	if (len)
	{
		// Check and decode result
		int checkResult = checkResult_4x4b(buf, len);
		if (OK == checkResult)
		{
			Result_4x4b* res = (Result_4x4b*)buf;
			W->ap = B4F(res->ap, 1000.0);
			W->am = B4F(res->am, 1000.0);
			W->rp = B4F(res->rp, 1000.0);
			W->rm = B4F(res->rm, 1000.0);
		}

		return checkResult;
	}

	return COMMUNICATION_ERROR;
}
