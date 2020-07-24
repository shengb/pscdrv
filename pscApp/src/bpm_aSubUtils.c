/*
 *	Bpm array calculation.
 *
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbFldTypes.h>
#include <registryFunction.h>
#include <aSubRecord.h>
#include <waveformRecord.h>
#include <epicsExport.h>
#include <epicsTime.h>

/* TBT and FA waveform 'nm' to 'mm' unit conversion */
static long bpmPosXyUnitConvrsion(aSubRecord *pasub)
{
int i;
	static float  *px, *py;
	static float data_x[200000];		/* max 200k */
	static float data_y[200000];
	static long *pN;
	static long N;

	// read N
	pN =  (long *)pasub->d;
	N =  (long)(*pN);

	long* pvalue = pasub->c;

	px = (float *)pasub->a;
	py = (float *)pasub->b;

	//printf("max len = %d,   %d\n", pasub->noa, *pvalue );	
	if (*pvalue == 0) {
		//for (i=0; i<pasub->noa; i++) {
		for (i=0; i<N; i++) {
			data_x[i] = *(px+i) * 0.000001;	/* nm to mm */
			data_y[i] = *(py+i) * 0.000001;
	//		printf("%d x =  %.0f, %f, y = %.0f, %f\n", i, *(px+i), data_x[i], *(py+i), data_y[i]);		}
		}
		memcpy(pasub->vala, data_x, pasub->noa*sizeof(float) );
		memcpy(pasub->valb, data_y, pasub->noa*sizeof(float) );
	}
	else {	
		memset(pasub->vala, 0, pasub->noa*sizeof(float) );
		memset(pasub->valb, 0, pasub->noa*sizeof(float) );	
	}

		
	return(0);
}



/*
 *
 record(aSub, "$(P){BPM:$(NO)}PY:Tbt-Calc")
{
        field(SNAM, "bpmWfmPolyCoeffCalSup")
        field(INPA, "$(P){BPM:$(NO)}TBT-X")
        field(INPB, "$(P){BPM:$(NO)}TBT-Y")
        field(INPC,  "$(P){BPM:$(NO)}K:Ky10-SP")
        field(INPD,  "$(P){BPM:$(NO)}K:Ky12-SP")
        field(INPE,  "$(P){BPM:$(NO)}K:Ky30-SP")
		
        field(INPF,  "$(P){BPM:$(NO)}Kx-SP")
        field(INPG,  "$(P){BPM:$(NO)}Ky-SP")		
        field(INPH,  "$(P){BPM:$(NO)}BbaXOff-SP")
        field(INPI,  "$(P){BPM:$(NO)}BbaYOff-SP")	
		
        field(FTA,  "FLOAT")
        field(FTB,  "FLOAT")
        field(FTC,  "FLOAT")
        field(FTD,  "FLOAT")
        field(FTE,  "FLOAT")
		
        field(FTF,  "FLOAT")
        field(FTG,  "FLOAT")
        field(FTH,  "FLOAT")
        field(FTI,  "FLOAT")	
		
        field(NOA,  "$(TBT_WFM_LEN)")
        field(NOB,  "$(TBT_WFM_LEN)")
        field(NOC,  "1")
        field(NOD,  "1")
        field(NOE,  "1")
        field(OUTA, "$(P){BPM:$(NO)}PY:Tbt-x PP")
        field(OUTB, "$(P){BPM:$(NO)}PY:Tbt-y PP")
        field(NOVA, "$(TBT_WFM_LEN)")
        field(NOVB, "$(TBT_WFM_LEN)")
        field(NOVC, "1")
        field(NOVD, "1")
        field(NOVE, "1")
        field(FTVA, "FLOAT")
        field(FTVB, "FLOAT")
        field(FTVC, "FLOAT")
        field(FTVD, "FLOAT")
        field(FTVE, "FLOAT")
}


 */
static long bpmWfmPolyCoeffCalSup(aSubRecord *pasub)
{
	int i;
	static float  *px, *py;
	static float data_x[200000];		/* max 200k */
	static float data_y[200000];
	static float *pK10, *pK12, *pK30;
	static float *pKx, *pKy;
	static float *pBbaOffset_x, *pBbaOffset_y;
	
	static float  Kx, Ky, bba_off_x, bba_off_y, x, y, K1, K3, K5, xp3, xp5, yp3, yp5, sum;
	static float  x_sum_linear;
	static float  y_sum_linear;
	

	px   =  (float *)pasub->a;	/* x position */
	py   =  (float *)pasub->b;	/* y position */
	pK10 =  (float *)pasub->c;
	pK12 =  (float *)pasub->d;
	pK30 =  (float *)pasub->e;
	//Kx, Ky
	pKx =  (float *)pasub->f;
	pKy =  (float *)pasub->g;
	pBbaOffset_x =  (float *)pasub->h;
	pBbaOffset_y =  (float *)pasub->i;
	
	K1   =  (float)(*pK10);
	K3   =  (float)(*pK12);
	K5   =  (float)(*pK30);
	Kx   =  (float)(*pKx);
	Ky   =  (float)(*pKy);
	bba_off_x = (float) (*pBbaOffset_x);
	bba_off_y = (float) (*pBbaOffset_y);
	
	
	//printf("max len = %d, %f, %f, %f\n", pasub->noa, k10, k12, k30);	
	for (i=0; i<pasub->noa; i++) 
	{
		x = *(px+i) * 0.000001;	/* nm to mm */
		y = *(py+i) * 0.000001;	/* nm to mm */
		//printf("%d x =  %.0f, y = %.0f\n", i, x, y);
		
		x_sum_linear = ( x - bba_off_x) / Kx;
		xp3 = pow(x_sum_linear, 3);
		xp5 = pow(x_sum_linear, 5);

		y_sum_linear = ( y - bba_off_y) / Ky ;
		yp3 = pow(y_sum_linear, 3);
		yp5 = pow(y_sum_linear, 5);	
		
		data_x[i] = (K1*x_sum_linear + K3*xp3 + K5*xp5) + bba_off_x;		
		data_y[i] = (K1*y_sum_linear + K3*yp3 + K5*yp5) + bba_off_y;
		//printf("%d x =  %f, %f, y = %f, %f\n", i, x, data_x[i], y, data_y[i] );
	}
	memcpy(pasub->vala, data_x, pasub->noa*sizeof(float) );
	memcpy(pasub->valb, data_y, pasub->noa*sizeof(float) );

	
  	return 0;
}



#if 0
// Button SUM
static long bpmPosButtonSum(aSubRecord *pasub)
{
int i;
	static long  *pA, *pB, *pC, *pD;
	static long data_sum[200000];		/* max 200k */
	
	pA = (long *)pasub->a;
	pB = (long *)pasub->b;
	pC = (long *)pasub->c;
	pD = (long *)pasub->d;

	
//	printf("max len = %d\n", pasub->noa);	
	for (i=0; i<pasub->noa; i++) {
		data_sum[i] = *(pA+i) + *(pB+i) + *(pC+i) + *(pD+i) ;	/* nm to mm */
	//	printf("%d a=%d, b=%d, c= %d, d=%d, sum=%d\n", i, *(pA+i), *(pB+i), *(pC+i), *(pD+i), data_sum[i]);
	}
	memcpy(pasub->vala, data_sum, pasub->noa*sizeof(long) );
		
	return(0);
}
#endif

static long bpmPosButtonSum(aSubRecord *pasub)
{
int i;
int	len;
	static long  *pA, *pB, *pC, *pD;
	static float bsum[200000];           /* max 200k */
	static float ab[200000];
	static float bb[200000];
	static float cb[200000];
	static float db[200000];
	static long *pN;
	static long N;

	pA = (long *)pasub->a;
	pB = (long *)pasub->b;
	pC = (long *)pasub->c;
	pD = (long *)pasub->d;

	// read N
	pN =  (long *)pasub->e;
	N =  (long)(*pN);

#if 1
//	printf("max len = %d, %d\n", N, pasub->noa);
//	for (i=0; i < pasub->noa; i++) {
	for (i=0; i < N; i++) {
		ab[i] = (float) (*(pA+i) * 4.656612875245797e-10);	//2^31
		bb[i] = (float) (*(pB+i) * 4.656612875245797e-10);
		cb[i] = (float) (*(pC+i) * 4.656612875245797e-10);
		db[i] = (float) (*(pD+i) * 4.656612875245797e-10);
		bsum[i] = ab[i] + bb[i] + cb[i] + db[i] ;  
	//  printf("%d a=%f, b=%f, c= %f, d=%f, sum=%f\n", i,  ab[i],  bb[i],  cb[i],  db[i], bsum[i]);
	}	
#endif

	len = pasub->noa*sizeof(float);
	memcpy(pasub->vala, ab,  len);
	memcpy(pasub->valb, bb, len );
	memcpy(pasub->valc, cb, len );
	memcpy(pasub->vald, db, len );
	memcpy(pasub->vale, bsum, len );
	//printf("%d  len=%d\n", N, len);

	return(0);
}


/*
 *
 */
#define FREQ_5HZ_I              1
#define FREQ_60HZ_I             60
#define FREQ_120HZ_I            120
#define FREQ_500HZ_I            500
#define FREQ_5KHZ_I             4999

static long cc_fa_bpmPsdIntCal(aSubRecord *pasub)
{
        static float *px1;
        static float *py1;

        static float *pFreq;
        static long  *pF1, *pF2;
        static float freq_5hz;
        static float result[10];
        static long  f1, f2;
        static float xpos_int_user1, ypos_int_user1, xpos_int_user2, ypos_int_user2;
		static float xpos_int_5hz, xpos_int_60hz, xpos_int_120hz, xpos_int_500hz, xpos_int_5khz;
		static float ypos_int_5hz, ypos_int_60hz, ypos_int_120hz, ypos_int_500hz, ypos_int_5khz;
		
#if 1
        px1 = (float *)pasub->a;
        py1 = (float *)pasub->b;
        //
        pFreq = (float * )pasub->c;
        pF1   = (long * )pasub->d;
        pF2   = (long * )pasub->e;

        f1 =  (long)(*pF1);
        f2 =  (long)(*pF2);
		
        freq_5hz = (float) (*(pFreq+FREQ_5HZ_I));       //4.98086 Hz

//5 Hz		
        xpos_int_5hz = (float) (*(px1+FREQ_5HZ_I));
        ypos_int_5hz = (float) (*(py1+FREQ_5HZ_I));
//60 Hz
        xpos_int_60hz = (float) (*(px1+FREQ_60HZ_I));
        ypos_int_60hz = (float) (*(py1+FREQ_60HZ_I));		
//120 Hz
        xpos_int_120hz = (float) (*(px1+FREQ_120HZ_I));
        ypos_int_120hz = (float) (*(py1+FREQ_120HZ_I));			
//500 Hz
        xpos_int_500hz = (float) (*(px1+FREQ_500HZ_I));
        ypos_int_500hz = (float) (*(py1+FREQ_500HZ_I));
//5 kHz
        xpos_int_5khz = (float) (*(px1+FREQ_5KHZ_I));
        ypos_int_5khz = (float) (*(py1+FREQ_5KHZ_I));
//
        if(f1 <1 ) f1 = 1;
        else if(f1>4999)  f1=4999;
        if(f2 <1 ) f2 = 1;
        else if(f2>4999)  f2=4999;

        xpos_int_user1 = (float) (*(px1+f1));
        ypos_int_user1 = (float) (*(py1+f1));
        xpos_int_user2 = (float) (*(px1+f2));
        ypos_int_user2 = (float) (*(py1+f2));

        result[0] = xpos_int_60hz - xpos_int_5hz;
        result[1] = ypos_int_60hz - ypos_int_5hz;
        result[2] = xpos_int_120hz - xpos_int_5hz;
        result[3] = ypos_int_120hz - ypos_int_5hz;		
        result[4] = xpos_int_500hz - xpos_int_5hz;
        result[5] = ypos_int_500hz - ypos_int_5hz;		
        result[6] = xpos_int_5khz - xpos_int_5hz;
        result[7] = ypos_int_5khz - ypos_int_5hz;
        result[8] = xpos_int_user2 - xpos_int_user1;
        result[9] = ypos_int_user2 - ypos_int_user1;

        memcpy(pasub->vala, &result[0], 4);
        memcpy(pasub->valb, &result[1], 4 );
        memcpy(pasub->valc, &result[2], 4 );
        memcpy(pasub->vald, &result[3], 4 );
        memcpy(pasub->vale, &result[4], 4 );
        memcpy(pasub->valf, &result[5], 4 );
        memcpy(pasub->valg, &result[6], 4 );
        memcpy(pasub->valh, &result[7], 4 );
        memcpy(pasub->vali, &result[8], 4 );
        memcpy(pasub->valj, &result[9], 4 );
		
//        printf("psd_int : %f, %f, %f, %f\n", result[0], result[1], result[2], result[3]);

#endif

	return(0);
}
epicsRegisterFunction(cc_fa_bpmPsdIntCal);


epicsRegisterFunction(bpmPosXyUnitConvrsion);
epicsRegisterFunction(bpmWfmPolyCoeffCalSup);
epicsRegisterFunction(bpmPosButtonSum);

