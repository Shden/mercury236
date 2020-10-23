#include <sys/types.h>
#include <sys/select.h>
#include <stdint.h>
#include <unistd.h>

#pragma pack(push, 1)

#define BAUDRATE 		B57600
#define TIME_OUT		2 * 1000	// Mercury inter-command delay (ms)
#define CH_TIME_OUT		1		// Channel timeout (sec)
#define PM_ADDRESS		0		// RS485 addess of the power meter

#define UInt16			uint16_t
#define byte			unsigned char
#define TARRIF_NUM		2		// 2 tariffs supported

#define MERCURY_SEMAPHORE	"MERCURY_RS485"
#define MERCURY_ACCESS_PERM	0666

// ***** Commands
// Test connection
typedef struct
{
	byte	address;
	byte	command;
	UInt16	CRC;
} TestCmd;

// Connection initialisaton command
typedef struct
{
	byte	address;
	byte	command;
	byte 	accessLevel;
	byte	password[6];
	UInt16	CRC;
} InitCmd;

// Connection terminaion command
typedef struct
{
	byte	address;
	byte	command;
	UInt16	CRC;
} ByeCmd;

// Power meter parameters read command
typedef struct
{
	byte	address;
	byte	command;	// 8h
	byte	paramId;	// No of parameter to read
	byte	BWRI;
	UInt16 	CRC;
} ReadParamCmd;

// ***** Results
// 1-byte responce (usually with status code)
typedef struct
{
	byte	address;
	byte	result;
	UInt16	CRC;
} Result_1b;

// 3-byte responce
typedef struct
{
	byte	address;
	byte	res[3];
	UInt16	CRC;
} Result_3b;

// Result with 3 bytes per phase
typedef struct
{
	byte	address;
	byte	p1[3];
	byte	p2[3];
	byte	p3[3];
	UInt16	CRC;
} Result_3x3b;

// Result with 3 bytes per phase plus 3 bytes for phases sum
typedef struct
{
	byte	address;
	byte	sum[3];
	byte	p1[3];
	byte	p2[3];
	byte	p3[3];
	UInt16	CRC;
} Result_4x3b;

// Result with 4 bytes per phase plus 4 bytes for sum
typedef struct
{
	byte	address;
	byte	ap[4];		// active +
	byte	am[4];		// active -
	byte	rp[4];		// reactive +
	byte	rm[4];		// reactive -
	UInt16	CRC;
} Result_4x4b;

// 3-phase vector (for voltage, frequency, power by phases)
typedef struct
{
	float	p1;
	float	p2;
	float	p3;
} P3V;

// 3-phase vector (for voltage, frequency, power by phases) with sum by all phases
typedef struct
{
	float	sum;
	float	p1;
	float	p2;
	float	p3;
} P3VS;

// Power vector
typedef struct
{
	float 	ap;		// active +
	float	am;		// active -
	float 	rp;		// reactive +
	float 	rm;		// reactive -
} PWV;

// Mains status
typedef enum
{
	MS_OFF = 0,		// No external power available, mains OFF
	MS_ON = 1		// Normal power supply, mains ON
} MS;


// Output results block
typedef struct
{
	P3V 	U;			// voltage
	P3V	I;			// current
	P3V	A;			// phase angles
	P3VS	C;			// cos(f)
	P3VS	P;			// current active power consumption
	P3VS	S;			// current reactive power consumption
	PWV	PR;			// power counters from reset (all tariffs)
	PWV	PRT[TARRIF_NUM];	// power counters from reset (by tariffs)
	PWV	PY;			// power counters for yesterday
	PWV	PT;			// power counters for today
	float	f;			// grid frequency
	MS	ms;			// mains status
} OutputBlock;

typedef enum 			// How much energy consumed:
{
	PP_RESET = 0,		// from reset
	PP_YTD = 1,		// this year
	PP_LAST_YEAR = 2,	// last year
	PP_MONTH = 3,		// for the month specified
	PP_TODAY = 4,		// today
	PP_YESTERDAY = 5	// yesterday
} PowerPeriod;

typedef enum
{
	OK = 0,
	ILLEGAL_CMD = 1,
	INTERNAL_COUNTER_ERR = 2,
	PERMISSION_DENIED = 3,
	CLOCK_ALREADY_CORRECTED = 4,
	CHANNEL_ISNT_OPEN = 5,
	WRONG_RESULT_SIZE = 256,
	WRONG_CRC = 257,
	CHECK_CHANNEL_FAILURE = 258,
	COMMUNICATION_ERROR = 259
} ResultCode;

// Function prototypes:
int checkChannel(int);
int initConnection(int);
int closeConnection(int);
int getU(int, P3V*);
int getI(int, P3V*);
int getCosF(int, P3VS*);
int getF(int, float*);
int getA(int, P3V*);
int getP(int, P3VS*);
int getS(int, P3VS*);
int getW(int, PWV*, int, int, int);

#pragma pack(pop)