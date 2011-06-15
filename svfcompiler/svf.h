#ifndef _SVF_H_
#define _SVF_H_

typedef enum
{
	UNKNOWN = -1,
	RESET = 0,
	IDLE = 1,
	DRSELECT = 2,
	DRCAPTURE = 3,
	DRSHIFT = 4,
	DRPAUSE = 5,
	DREXIT1 = 6,
	DREXIT2 = 7,
	DRUPDATE = 8,
	IRSELECT = 9,
	IRCAPTURE = 10,
	IRSHIFT = 11,
	IRPAUSE = 12,
	IREXIT1 = 13,
	IREXIT2 = 14,
	IRUPDATE = 15
} svf_tapstate;


typedef enum
{
	ENDIR = 0,
	ENDDR = 1,
	FREQUENCY = 2,
	STATE = 3,
	RUNTEST = 4,
	SIR = 5,
	SDR = 6
} svf_statement;

typedef enum
{
	NONE,
	TCK,
	SCK
} svf_clk;

typedef enum
{
	INTEGER,
	REAL
} svf_numbertype;

typedef struct
{
	int valid;
	int mul;
	int exp;
} svf_number;

typedef struct
{
	int valid;
	unsigned int size;
	unsigned char* data;
} svf_bitstream;

typedef struct
{
	svf_bitstream tdi;
	svf_bitstream tdo;
	svf_bitstream mask;
	svf_bitstream smask;
} svf_bitstreamset;


typedef struct
{
	svf_statement type;
	svf_tapstate irendstate;
	svf_tapstate drendstate;
	svf_number freq;
	svf_tapstate state;

	svf_tapstate runstate;
	svf_tapstate runendstate;
	svf_number runmintime;
	unsigned int runmincount;
	svf_number runmaxtime;
	unsigned int runcount;
	svf_clk runclk;

	svf_bitstreamset* scanset;
	unsigned int scanlength;

} svf_statementcontext;

typedef void (*svf_parsecallback)(svf_statementcontext* ctx, void* callbackarg);

typedef struct
{
	FILE* file;
	char* buffer;
	unsigned int buffersize;
	unsigned int bufferpos;
	unsigned int line;
	svf_parsecallback callback;
	void* callbackarg;
	svf_bitstreamset header;
	svf_bitstreamset trailer;
	svf_bitstreamset scan;
	double period;
} svf_context;

#ifdef __cplusplus
extern "C" {
#endif

void svf_initbitstream(svf_bitstream* stream);
void svf_destroybitstream(svf_bitstream* stream);
void svf_initbitstreamset(svf_bitstreamset* set);
void svf_destroybitstreamset(svf_bitstreamset* set);
int svf_open(svf_context* ctx, const char* filepath);
void svf_destroy(svf_context* ctx);
void svf_setcallback(svf_context* ctx, svf_parsecallback callback, void* callbackarg);
int svf_readtoken(svf_context* ctx, char** token, unsigned int* tokensize);
int svf_readnumber(svf_context* ctx, svf_number* num, svf_numbertype type);
int svf_matchtoken(char* token, unsigned int tokensize, const char* test);
int svf_readbuffer(svf_context* ctx);
svf_tapstate svf_readtapstate(svf_context* ctx);
int svf_hextonibble(char ch, unsigned char* nibble);
int svf_readbitstream(svf_context* ctx, svf_bitstream* stream, unsigned int size);
int svf_readbitstreamset(svf_context* ctx, svf_bitstreamset* set, unsigned int size);
int svf_parse(svf_context* ctx);
const char* svf_statename(svf_tapstate state);
unsigned int svf_realtointeger(svf_number* number);
double svf_realtodouble(svf_number* number);

#ifdef __cplusplus
}
#endif

#endif //_SVF_H_
