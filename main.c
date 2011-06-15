#include <stdio.h>
#include <stdlib.h>

#include "svf.h"



static void usage(const char *argv0)
{
	fprintf(stderr,
			"Usage: %s <in> <out>\n"
		   "SVFCOMPILER (c) neimod.\n"
           "\n",
		   argv0);
   exit(1);
}


void memdump(const unsigned char* data, unsigned int size)
{
	unsigned int i;


	for(i=0; i<size; i++)
		printf("%02X", data[i]);
}

unsigned int putle32(unsigned char* p, unsigned int offset, unsigned int n)
{
	p[offset+0] = n>>0;
	p[offset+1] = n>>8;
	p[offset+2] = n>>16;
	p[offset+3] = n>>24;
	return offset+4;
}

unsigned int putle8(unsigned char* p, unsigned int offset, unsigned int n)
{
	p[offset] = n;
	return offset+1;
}

void callback(svf_statementcontext* ctx, void* callbackarg)
{
	FILE* f = (FILE*)callbackarg;
	unsigned char buffer[4096];
	unsigned int pos = 0;
	unsigned int flags = 0;

	switch(ctx->type)
	{
		case ENDIR: 
			//printf("ENDIR %s\n", svf_statename(ctx->irendstate));
			pos = putle8(buffer, pos, ENDIR);
			pos = putle8(buffer, pos, ctx->irendstate);
			fwrite(buffer, 1, pos, f);
		break;

		case ENDDR: 
			//printf("ENDDR %s\n", svf_statename(ctx->drendstate));
			pos = putle8(buffer, pos, ENDDR);
			pos = putle8(buffer, pos, ctx->drendstate);
			fwrite(buffer, 1, pos, f);
		break;

		case FREQUENCY:
			//printf("FREQUENCY %d^%d\n", ctx->freq.mul, ctx->freq.exp);
			pos = putle8(buffer, pos, FREQUENCY);
			pos = putle32(buffer, pos, svf_realtointeger(&ctx->freq));
			fwrite(buffer, 1, pos, f);
		break;

		case STATE:
			//printf("STATE %s\n", svf_statename(ctx->state));
			pos = putle8(buffer, pos, STATE);
			pos = putle32(buffer, pos, ctx->state);
			fwrite(buffer, 1, pos, f);
		break;

		case RUNTEST:
			//printf("RUNTEST");
			flags = 0;
			if (ctx->runstate != UNKNOWN)
				flags |= (1<<0);
			if (ctx->runendstate != UNKNOWN)
				flags |= (1<<1);

			pos = putle8(buffer, pos, RUNTEST);
			pos = putle8(buffer, pos, flags);
			pos = putle32(buffer, pos, ctx->runcount);
			
			//printf(" %d clocks", ctx->runcount);

			if (ctx->runstate != UNKNOWN)
			{
				//printf(" state %s", svf_statename(ctx->runstate));
				pos = putle8(buffer, pos, ctx->runstate);
			}
			if (ctx->runendstate != UNKNOWN)
			{
				//printf(" endstate %s", svf_statename(ctx->runendstate));
				pos = putle8(buffer, pos, ctx->runendstate);
			}
			fwrite(buffer, 1, pos, f);
			//printf("\n");
		break;

		case SIR:
		case SDR:
			flags = 0;
			if (ctx->scanset->tdi.valid)
				flags |= (1<<0);
			if (ctx->scanset->tdo.valid)
				flags |= (1<<1);
			if (ctx->scanset->mask.valid)
				flags |= (1<<2);

			pos = putle8(buffer, pos, ctx->type);
			pos = putle8(buffer, pos, flags);
			pos = putle32(buffer, pos, ctx->scanlength);
			fwrite(buffer, 1, pos, f);
			
			//printf("SIR %d", ctx->scanlength);
			if (ctx->scanset->tdi.valid)
			{
				//printf(" TDI ");
				//memdump(ctx->scanset->tdi.data, (ctx->scanlength+7)/8);
				fwrite(ctx->scanset->tdi.data, 1, (ctx->scanlength+7)/8, f);
			}
			if (ctx->scanset->tdo.valid)
			{
				//printf(" TDO ");
				//memdump(ctx->scanset->tdo.data, (ctx->scanlength+7)/8);
				fwrite(ctx->scanset->tdo.data, 1, (ctx->scanlength+7)/8, f);
			}
			if (ctx->scanset->mask.valid)
			{
				//printf(" MASK ");
				//memdump(ctx->scanset->mask.data, (ctx->scanlength+7)/8);
				fwrite(ctx->scanset->mask.data, 1, (ctx->scanlength+7)/8, f);
			}
			//printf("\n");
		break;
	}
	
}

int main(int argc, char* argv[])
{
	svf_context svf;
	FILE* f;

	if (argc != 3)
		usage(argv[0]);

	if (svf_open(&svf, argv[1]))
	{
		f = fopen(argv[2], "wb");
		svf_setcallback(&svf, callback, f);
		printf("Processing...\n");
		if (svf_parse(&svf))
		{
			printf("Done\n");
		}
	}

	return 0;
}
