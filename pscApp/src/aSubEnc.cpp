#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>

#include <dbAccess.h>
#include <dbDefs.h>
#include <dbFldTypes.h>
#include <registryFunction.h>
#include <aSubRecord.h>
#include <waveformRecord.h>
#include <epicsExport.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <callback.h>

#define FMT "%Y-%m-%d %H:%M:%S.%09f"

/* Encoder save raw stream data to file */
static long subEncStreamProc(aSubRecord *pasub)
{
        unsigned char *aPtr;
        int i, cd;
        int wfLength = 16384;
        FILE *fd;
	char * filename;
	char * path;

        aPtr = (unsigned char *)pasub->a;

	filename = basename((char *)pasub->b);
	path = dirname((char *)pasub->b);

	cd = chdir(path);
	if (cd < 0){
		fprintf(stderr, "Error opening output directory %s: %s\n", path, strerror(errno));
                return -1;	
	}
	
	fd = fopen(filename, "a");

        for(i=0;i<wfLength;i++) {
            fprintf(fd,"%02x ", *aPtr);
            aPtr++;
            if (!((i+1)%16))
                fprintf(fd,"\n");
        }

        fclose(fd);
        return 0;
}

/* Encoder save formatted data to file */
static long subEncFormatProc(aSubRecord *pasub)
{
        double *aPtr;
        double *bPtr;
        double *cPtr;
        double *dPtr;
        double *ePtr;
        double *aOutPtr;
	FILE *outFile;
	int i, cd;
	long sz;
        int wfLength = 1024;
        epicsTimeStamp TS;
        char tsbuf[35];

	char * filename;
	char * path;
        
	aPtr = (double *)pasub->a;
        bPtr = (double *)pasub->b;
        cPtr = (double *)pasub->c;
        dPtr = (double *)pasub->d;
        ePtr = (double *)pasub->e;
	
        aOutPtr = (double *)pasub->vala;
	
	filename = basename((char *)pasub->g);
	path = dirname((char *)pasub->g);

	cd = chdir(path);
	if (cd < 0){
		fprintf(stderr, "Error opening output directory %s: %s\n", path, strerror(errno) );
                return -1;	
	}
	
	outFile = fopen(filename, "a");
        if(!outFile) {
        	fprintf(stderr, "Error opening file %s: %s\n", filename, strerror(errno) );
                return -1;
        }
        
	for(i=0;i<wfLength;i++) {
	     /* //Uncomment to get formatted timestamp in the file
		TS.secPastEpoch = (long)(*aPtr) - POSIX_TIME_AT_EPICS_EPOCH;
		TS.nsec = (long)(*bPtr);
		epicsTimeToStrftime(tsbuf, sizeof(tsbuf), FMT, &TS);
		fprintf(outFile,"%s %d %d\n", tsbuf, ((long)*cPtr >> 8), (long)*dPtr);
	     */

        	if ((long)*aPtr)
			fprintf(outFile,"%u %09u %d %u %02d\n", ((long)*aPtr), ((long)(*bPtr)), ((long)*cPtr >> 8), ((long)*dPtr), ((long)*ePtr) & 0xFF);
	        aPtr++;
        	bPtr++;
        	cPtr++;
        	dPtr++;
        	ePtr++;
        }
	sz = ftell(outFile);	
	fclose(outFile);
        *aOutPtr = (double) sz/1000000.; 
        
	return 0;
}

/* Encoder timestamp data process: concatenate I32 sec and I32 nsec part */
static long subEncTSProc(aSubRecord *pasub)
{
        double *aPtr, *bPtr;
        double *aOutPtr;
        aPtr = (double *)pasub->a;
        bPtr = (double *)pasub->b;
        aOutPtr = (double *)pasub->vala;
        int i;
        int wfLength = 1024;
        long long lsb, msb;

        for(i=0;i<wfLength;i++) {
            lsb = (long long)(*bPtr);
            msb = (long long)(*aPtr) << 32;
            *aOutPtr= (double)(lsb + msb);
            //printf("i=%d, lsb = 0x%016llX, msb = 0x%016llX, *aOutPtr = %f\n\r", i, lsb, msb, *aOutPtr);
            aOutPtr++;
            aPtr++;
            bPtr++;
        }

        return 0;
}

/* Encoder data process: check current state of A and B lines */
static long subEncABProc(aSubRecord *pasub)
{
        double *aPtr;
        unsigned char *aOutPtr, *bOutPtr;
        aPtr = (double *)pasub->a;
        aOutPtr = (unsigned char *)pasub->vala;
        bOutPtr = (unsigned char *)pasub->valb;
        int i;
        int wfLength = 1024;
        short tmp;

        for(i=0;i<wfLength;i++) {
            tmp = (short)(*aPtr);
            *aOutPtr = (unsigned char)((tmp >> 3) & 1);
            *bOutPtr = (unsigned char)((tmp >> 2) & 1);
           // printf("i=%d, *aPtr = %f, tmp = 0x%x, *aOutPtr = 0x%x, *bOutPtr = 0x%x\n\r", i, *aPtr, tmp, *aOutPtr, *bOutPtr);
            aOutPtr++;
            bOutPtr++;
            aPtr++;
        }

        return 0;
}

epicsRegisterFunction(subEncStreamProc);
epicsRegisterFunction(subEncFormatProc);
epicsRegisterFunction(subEncTSProc);
epicsRegisterFunction(subEncABProc);
