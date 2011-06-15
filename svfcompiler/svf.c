#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "svf.h"

#define STATEMAX	16

const char* svf_statenames[STATEMAX]=
{
	"RESET",
	"IDLE",
	"DRSELECT",
	"DRCAPTURE",
	"DRSHIFT",
	"DRPAUSE",
	"DREXIT1",
	"DREXIT2",
	"DRUPDATE",
	"IRSELECT",
	"IRCAPTURE",
	"IRSHIFT",
	"IRPAUSE",
	"IREXIT1",
	"IREXIT2",
	"IRUPDATE"
};

const char* svf_statename(svf_tapstate state)
{
	if (state == UNKNOWN)
		return "UNKNOWN";
	else
		return svf_statenames[state];
}

void svf_initbitstream(svf_bitstream* stream)
{
	stream->valid = 0;
	stream->size = 0;
	stream->data = 0;
}

void svf_destroybitstream(svf_bitstream* stream)
{
	free(stream->data);
}

void svf_initbitstreamset(svf_bitstreamset* set)
{
	svf_initbitstream(&set->tdi);
	svf_initbitstream(&set->tdo);
	svf_initbitstream(&set->mask);
	svf_initbitstream(&set->smask);
}

void svf_destroybitstreamset(svf_bitstreamset* set)
{
	svf_destroybitstream(&set->tdi);
	svf_destroybitstream(&set->tdo);
	svf_destroybitstream(&set->mask);
	svf_destroybitstream(&set->smask);

}

int svf_open(svf_context* ctx, const char* filepath)
{
	ctx->period = 1.0 / 1000000; // 1MHz
	ctx->callback = 0;
	ctx->line = 1;
	ctx->buffer = 0;
	ctx->buffersize = 0;
	ctx->file = fopen(filepath, "rb");
	svf_initbitstreamset(&ctx->scan);
	svf_initbitstreamset(&ctx->header);
	svf_initbitstreamset(&ctx->trailer);
	return ctx->file != 0;
}

void svf_destroy(svf_context* ctx)
{
	free(ctx->buffer);
	svf_destroybitstreamset(&ctx->scan);
	svf_destroybitstreamset(&ctx->header);
	svf_destroybitstreamset(&ctx->trailer);
}

void svf_setcallback(svf_context* ctx, svf_parsecallback callback, void* callbackarg)
{
	ctx->callback = callback;
	ctx->callbackarg = callbackarg;
}

int svf_readtoken(svf_context* ctx, char** token, unsigned int* tokensize)
{
	unsigned int size = 0;
	unsigned int tokenpos = 0;

	unsigned int pos = ctx->bufferpos; 

	while(ctx->buffer[pos] != 0)
	{
		if (ctx->buffer[pos] != ' ')
			break;
		pos++;
	}

	tokenpos = pos;

	while(ctx->buffer[pos] != 0)
	{
		if (ctx->buffer[pos] == ' ')
			break;
		pos++;
	}

	ctx->bufferpos = pos;

	*token = ctx->buffer + tokenpos;
	*tokensize = pos - tokenpos;

	return *tokensize != 0;
}

int svf_readnumber(svf_context* ctx, svf_number* num, svf_numbertype type)
{
	char* token;
	unsigned int tokensize;
	unsigned int p = 0;
	unsigned int rollbackpos = ctx->bufferpos;
	int sign = 1;

	if (0 == svf_readtoken(ctx, &token, &tokensize))
		goto error;

	if (p >= tokensize)
		goto error;
	if (token[p] < '0' || token[p] > '9')
		goto error;
	num->mul = 0;
	num->exp = 1;
	while(p < tokensize && token[p] >= '0' && token[p] <= '9')
	{
		num->mul = num->mul*10 + (token[p]-'0');
		p++;
	}
	if (type == REAL && p < tokensize)
	{
		if (token[p++] != 'E' || p >= tokensize)
			goto error;
		if (token[p] == '-')
		{
			sign = -1;
			p++;
		}
		if (p >= tokensize || token[p] < '0' || token[p] > '9')
			goto error;
		
		num->exp = 0;
		while(p < tokensize && token[p] >= '0' && token[p] <= '9')
		{
			num->exp = num->exp*10 + (token[p]-'0');
			p++;
		}

		num->exp *= sign;
	}

	if (p != tokensize)
		goto error;

	num->valid = 1;
	return 1;

error:
	num->valid = 0;
	num->mul = 0;
	num->exp = 1;

	ctx->bufferpos = rollbackpos;
	return 0;
}


int svf_matchtoken(char* token, unsigned int tokensize, const char* test)
{
	unsigned int testlen = strlen(test);

	if (testlen != tokensize)
		return 0;
	return strncmp(token, test, tokensize) == 0;
}

int svf_readbuffer(svf_context* ctx)
{
	unsigned int p = 0;
	int ch;
	int bracket = 0;
	
	
	ctx->bufferpos = 0;

	while(1)
	{
		if (ctx->buffersize < p+10)
		{
			ctx->buffersize *= 2;
			if (ctx->buffersize < 256)
				ctx->buffersize = 256;

			ctx->buffer = (char*)realloc(ctx->buffer, ctx->buffersize);
		}

		ctx->buffer[p] = 0;
		
		ch = fgetc(ctx->file);

		if (ch == '\n')
			ctx->line++;

		if (ch < 0)
		{
handle_eof:
			if (p)
				break;
			
			return 0;
		}

		if (ch == '!')
		{
			while(1)
			{
				ch = fgetc(ctx->file);
				if (ch < 0)
					goto handle_eof;
				if (ch == '\n')
				{
					ctx->line++;
					break;
				}
			}
		}

		// Convert tabs into spaces
		if (ch == '\t')
			ch = ' ';
		// Convert lowercase into uppercase
		if (ch >= 'a' && ch <= 'z')
			ch = 'A' + (ch-'a');

		if (ch == ';')
			break;

		if (ch == '(')
		{
			ctx->buffer[p++] = ' ';
			bracket++;
			continue;
		}

		if (ch == ')')
		{
			ctx->buffer[p++] = ' ';
			bracket--;
			continue;
		}

		switch(ch)
		{
			case '\r':
			case '\n':
				break;

			case ' ':
			{
				if (bracket)
					break;
			}

			default:
				ctx->buffer[p++] = ch;
		}
	}

	
//	printf("#%s#\n", ctx->buffer);

	return 1;
}

svf_tapstate svf_readtapstate(svf_context* ctx)
{
	char* token;
	unsigned int tokensize;
	unsigned int i;
	unsigned int rollbackpos = ctx->bufferpos;

	if (0 == svf_readtoken(ctx, &token, &tokensize))
		goto error;

	for(i=0; i<STATEMAX; i++)
	{
		if (svf_matchtoken(token, tokensize, svf_statenames[i]))
			return (svf_tapstate)i;
	}

error:
	ctx->bufferpos = rollbackpos;
	return UNKNOWN;
}

int svf_hextonibble(char ch, unsigned char* nibble)
{
	if (ch >= '0' && ch <= '9')
	{
		*nibble = 0 + (ch-'0');
		return 1;
	}
	if (ch >= 'A' && ch <= 'Z')
	{
		*nibble = 0xA + (ch-'A');
		return 1;
	}
	if (ch >= 'a' && ch <= 'z')
	{
		*nibble = 0xA + (ch-'a');
		return 1;
	}

	return 0;
}


int svf_readbitstream(svf_context* ctx, svf_bitstream* stream, unsigned int size)
{
	char* token;
	unsigned int tokensize;
	unsigned int bytesize = (size+7)/8;
	unsigned int oldbytesize = (stream->size+7)/8;
	unsigned int rollbackpos = ctx->bufferpos;
	unsigned char nibble;
	unsigned int i;
	unsigned int nibblecount = (size+3)/4;

	stream->valid = 0;

	if (0 == svf_readtoken(ctx, &token, &tokensize))
		goto error;

	if (tokensize*4 < size)
		goto error;

	stream->size = size;
	if (bytesize > oldbytesize)
		stream->data = (unsigned char*)realloc(stream->data, bytesize);

	for(i=0; i<nibblecount; i++)
	{
		if (0 == svf_hextonibble(token[tokensize-1-i], &nibble))
			goto error;
		stream->data[bytesize-1-(i/2)] = (stream->data[bytesize-1-(i/2)] >> 4) | (nibble<<4);
	}
	if (nibblecount&1)
		stream->data[0] >>= 4;

	stream->valid = 1;

	return 1;

error:
	ctx->bufferpos = rollbackpos;
	return 0;
}

int svf_readbitstreamset(svf_context* ctx, svf_bitstreamset* set, unsigned int size)
{
	char* token;
	unsigned int tokensize;


	set->tdi.valid = 0;
	set->tdo.valid = 0;
	set->mask.valid = 0;
	set->smask.valid = 0;

	while(1)
	{
		if (0 == svf_readtoken(ctx, &token, &tokensize))
			break;
		if (svf_matchtoken(token, tokensize, "TDI"))
		{
			if (0 == svf_readbitstream(ctx, &set->tdi, size))
				goto error;
		}
		else if (svf_matchtoken(token, tokensize, "TDO"))
		{
			if (0 == svf_readbitstream(ctx, &set->tdo, size))
				goto error;
		}
		else if (svf_matchtoken(token, tokensize, "MASK"))
		{
			if (0 == svf_readbitstream(ctx, &set->mask, size))
				goto error;
		}
		else if (svf_matchtoken(token, tokensize, "SMASK"))
		{
			if (0 == svf_readbitstream(ctx, &set->smask, size))
				goto error;
		}
	}

	return 1;

error:
	return 0;
}

unsigned int svf_realtointeger(svf_number* number)
{
	unsigned int base = 1;
	unsigned int i;

	if (number->exp<0)
	{
		return 0;
	}

	for(i=0; i<number->exp; i++)
		base *= 10;

	return base * number->mul;
}

double svf_realtodouble(svf_number* number)
{
	double exp = number->exp;
	double mul = number->mul;

	return mul * pow(10.0, exp);
}

int svf_parse(svf_context* ctx)
{
	char* token;
	unsigned int tokensize;
	svf_number num;
	svf_statementcontext param;
	svf_statement type;

	while(svf_readbuffer(ctx))
	{
		if (0 == svf_readtoken(ctx, &token, &tokensize))
			continue;

//		printf("[");
//		for(i=0; i<tokensize; i++)
//			printf("%c", token[i]);
//		printf("]");

		if (svf_matchtoken(token, tokensize, "ENDIR"))
			type = ENDIR;
		else if (svf_matchtoken(token, tokensize, "ENDDR"))
			type = ENDDR;
		else if (svf_matchtoken(token, tokensize, "FREQUENCY"))
			type = FREQUENCY;
		else if (svf_matchtoken(token, tokensize, "STATE"))
			type = STATE;
		else if (svf_matchtoken(token, tokensize, "RUNTEST"))
			type = RUNTEST;
		else if (svf_matchtoken(token, tokensize, "SIR"))
			type = SIR;
		else if (svf_matchtoken(token, tokensize, "SDR"))
			type = SDR;

		param.type = type;

		if (type == ENDIR)
		{
			param.irendstate = svf_readtapstate(ctx);
			if (param.irendstate == UNKNOWN)
				goto syntax_error;
		}
		else if (type == ENDDR)
		{
			param.drendstate = svf_readtapstate(ctx);
			if (param.drendstate == UNKNOWN)
				goto syntax_error;
		}
		else if (type == FREQUENCY)
		{
			if (0 == svf_readnumber(ctx, &num, REAL))
				goto syntax_error;
			if (0 == svf_readtoken(ctx, &token, &tokensize))
				goto syntax_error;
			if (0 == svf_matchtoken(token, tokensize, "HZ"))
				goto syntax_error;

			param.freq = num;
			ctx->period = 1.0 / svf_realtodouble(&num);
		}
		else if (type == STATE)
		{
			param.state = svf_readtapstate(ctx);
			if (param.state == UNKNOWN)
				goto syntax_error;
		}
		else if (type == RUNTEST)
		{
			param.runstate = svf_readtapstate(ctx);
			param.runendstate = UNKNOWN;
			param.runclk = NONE;
			param.runmintime.valid = 0;
			param.runmaxtime.valid = 0;

			while(1)
			{
				if (0 == svf_readnumber(ctx, &num, REAL))
				{
					if (0 == svf_readtoken(ctx, &token, &tokensize))
						break;
					if (svf_matchtoken(token, tokensize, "MAXIMUM"))
					{
						if (0 == svf_readnumber(ctx, &num, REAL))
							goto syntax_error;
						if (0 == svf_readtoken(ctx, &token, &tokensize))
							goto syntax_error;
						if (0 == svf_matchtoken(token, tokensize, "SEC"))
							goto syntax_error;
						param.runmaxtime = num;
					}
					else if (svf_matchtoken(token, tokensize, "ENDSTATE"))
					{
						param.runendstate = svf_readtapstate(ctx);
						if (param.runendstate == UNKNOWN)
							goto syntax_error;
					}
					else goto syntax_error;
				}
				else 
				{
					if (0 == svf_readtoken(ctx, &token, &tokensize))
						goto syntax_error;
					if (svf_matchtoken(token, tokensize, "SEC"))
					{
						param.runmintime = num;
						param.runmincount = svf_realtodouble(&num) / ctx->period;
						param.runmincount++;
					}
					else if (svf_matchtoken(token, tokensize, "TCK"))
					{
						param.runcount = svf_realtointeger(&num);
						param.runclk = TCK;
					}
					else if (svf_matchtoken(token, tokensize, "SCK"))
					{
						param.runcount = svf_realtointeger(&num);
						param.runclk = SCK;
					}
					else goto syntax_error;
				}
			}

			if (param.runclk == NONE && param.runmintime.valid == 0)
				goto syntax_error;

			if (param.runclk != TCK)
				param.runcount = 0;

			if (param.runmintime.valid && param.runmincount > param.runcount)
				param.runcount = param.runmincount;
		}
		else if (type == SIR || type == SDR)
		{
			param.type = type;
			if (0 == svf_readnumber(ctx, &num, INTEGER))
				goto syntax_error;
			param.scanlength = num.mul;
			param.scanset = &ctx->scan;

			if (0 == svf_readbitstreamset(ctx, &ctx->scan, param.scanlength))
				goto syntax_error;
		}

		if (0 != svf_readtoken(ctx, &token, &tokensize))
			goto syntax_error;

		if (ctx->callback)
			ctx->callback(&param, ctx->callbackarg);
	}

	return 1;

syntax_error:
	fprintf(stderr, "Syntax error on line %d: %s\n", ctx->line, ctx->buffer);
	return 0;
}
