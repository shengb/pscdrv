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


/* Gen save raw stream data to file */
static long subGenStreamProc(aSubRecord *pasub)
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

/* Gen save formatted data to file */
static long subGenFormatProc(aSubRecord *pasub)
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
		TS.nsec = (long)(*bPtr) << 3;
		epicsTimeToStrftime(tsbuf, sizeof(tsbuf), FMT, &TS);
		fprintf(outFile,"%s %d %d\n", tsbuf, ((long)*cPtr >> 8), (long)*dPtr);
	     */

        	fprintf(outFile,"%09u %09u %05u 0x%08x\n", ((long)*aPtr), ((long)(*bPtr) << 3), ((long)*dPtr), ((long)*ePtr));
	        aPtr++;
        	bPtr++;
        	dPtr++;
        	ePtr++;
        }
	sz = ftell(outFile);
        fclose(outFile);
       	*aOutPtr = (double) sz/1000000.;

	return 0;
}

/* Gen timestamp data process: concatenate I32 sec and I32 nsec part */
static long subGenTSProc(aSubRecord *pasub)
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

/* Gen voltage data process: 2's complement calc */
static long subGenVoltProc(aSubRecord *pasub)
{
        double *aPtr;
        double *aOutPtr;
        aPtr = (double *)pasub->a;
        aOutPtr = (double *)pasub->vala;
        int i;
        int wfLength = 1024;
        long long code;

        for(i=0;i<wfLength;i++) {
           code = (long long)(*aPtr) >> 8;
           if ((code >> 17) & 1)
		*aOutPtr = (double)((~code & 0x1FFFF)*(-0.000038)*4);
	    else
		*aOutPtr = (double)(( code & 0x1FFFF)*( 0.000038)*4);
            //printf("i=%d, lsb = 0x%016llX, msb = 0x%016llX, *aOutPtr = %f\n\r", i, lsb, msb, *aOutPtr);
            aOutPtr++;
            aPtr++;
        }

        return 0;
}

epicsRegisterFunction(subGenStreamProc);
epicsRegisterFunction(subGenFormatProc);
epicsRegisterFunction(subGenTSProc);
epicsRegisterFunction(subGenVoltProc);
