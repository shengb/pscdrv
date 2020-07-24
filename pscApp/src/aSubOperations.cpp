/* Y.Tian, September 2010-2013 */
/* P.Cheblakov 2012-2013 */
/* M.Davidsaver 2013 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

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

#include <gsl/gsl_statistics_float.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_fft_halfcomplex.h>

#include <map>


/* ADC data process: little-big indian change, high-low word change */
static long subADCWfProc(aSubRecord *pasub)
{
	double *aPtr;
	long *bPtr, *cPtr;
	float *aOutPtr;
	long i,wfLength,wfReadLength;
	short tempS1,tempS2;
	float tempF = 0.0;
	bPtr = (long *)pasub->b;
	cPtr = (long *)pasub->c;
	aOutPtr = (float *)pasub->vala;	
	wfReadLength = *bPtr;
	wfLength= *cPtr; 	
	
	const int shift_10k = 4;
	// Achtung! It's very ugly workaround to fix the waveform shift of 
	// 10k measurements
	int shift = wfReadLength > 1500 ? shift_10k : 0;
	
	aPtr = (double *)pasub->a+shift;
 	
//	printf("wfReadLength = %d, wfLength = %d, shift = %d\n", wfReadLength, wfLength, shift);
	
	for(i=0;i<wfReadLength-shift;i++) {
	   		if( ((int)(i/2)) == ((int)((i+1)/2)))  { 
			//even word: 
			tempS1 = (short)(*aPtr);
			tempS2 = (tempS1&0x00FF)*256+((tempS1&0xFF00)>>8);		
			tempF = tempS2/3276.6;
			aPtr++;
			} else {
			tempS1 = (short)(*aPtr);
			tempS2 = (tempS1&0x00FF)*256+((tempS1&0xFF00)>>8);		
			*aOutPtr = (float)(tempS2/3276.6);
			aOutPtr++;
			*aOutPtr= tempF;
			aOutPtr++;
			aPtr++;	}					
	}
	
	// TODO This is a workaround for shifting data in packet from PSC
	// fill last 'shift' points with previous data
	for (i=0; i < wfLength - (wfReadLength - shift) ; i++)
	{
	    *(aOutPtr+i) = *(aOutPtr-1);
	}
	
	return 0;
}

// Storage for FFT's wavetable 
// Wavetable can be reused for FFT calculation of the same size

class WavetableStorage
{
public:
    static WavetableStorage& instance();
    gsl_fft_real_wavetable* getWavetable(int size);

protected:
    static WavetableStorage Instance;

private:
    WavetableStorage() {};
    ~WavetableStorage();
    
    typedef std::map<int, gsl_fft_real_wavetable*> wtmap_t;
    wtmap_t wtmap;
    
};

WavetableStorage WavetableStorage::Instance;

WavetableStorage& WavetableStorage::instance()
{
    return Instance;
}

gsl_fft_real_wavetable* WavetableStorage::getWavetable(int size)
{
    wtmap_t::iterator it = wtmap.find(size);
    if( it != wtmap.end() ) {
        return it->second;
    } 
    else
    {
        fprintf(stderr, "Create new wavetable for FFT with size %d\n", size);
        wtmap[size] = gsl_fft_real_wavetable_alloc(size);
        return wtmap[size];
    }
}

WavetableStorage::~WavetableStorage()
{
    for (wtmap_t::iterator it = wtmap.begin(); it != wtmap.end(); ++it)
        gsl_fft_real_wavetable_free(it->second);
}

typedef struct {
  dbCommon *prec;
  CALLBACK cb;
  epicsEventId start;
  int done;
  double* data;
  double* cmplx_coeffs;
  epicsUInt32 count;
    gsl_fft_real_wavetable*        real;
    gsl_fft_real_workspace*        work;
    
  epicsThreadId worker;
} fftdata;

static void fftworker(void *raw)
{
    fftdata *priv = (fftdata*)raw;

    while(!priv->done) {
        epicsEventMustWait(priv->start);
//        fprintf(stderr, "%s: Wakeup\n", priv->prec->name);
        if(priv->done)
            break;
        gsl_fft_real_transform (priv->data, 1, priv->count, 
                                priv->real, priv->work);
         
        gsl_fft_halfcomplex_unpack(priv->data, priv->cmplx_coeffs, 1, priv->count);
        
//        fprintf(stderr, "%s: Data ready\n", priv->prec->name);
        callbackRequestProcessCallback(&priv->cb, priorityHigh, (void*)priv->prec);
    }
}

static long subFFT(aSubRecord *pasub)
{
    fftdata *priv = (fftdata*)pasub->dpvt;
    double* fPtrA      = (double*)pasub->a;
    double* fPtrValA   = (double*)pasub->vala;
    
    assert(pasub->nea <= pasub->neva);
    
    if(!priv) {
        priv = new fftdata;
        if(!priv)
            return -1;
            
        priv->prec = (dbCommon*)pasub;
//        callbackSetUser(priv->cb, priv);
        priv->start = epicsEventMustCreate(epicsEventEmpty);
        priv->data         = (double*)malloc(pasub->nea * sizeof(double));
        priv->cmplx_coeffs = (double*)malloc(2 * pasub->nea * sizeof(double));
        priv->work = gsl_fft_real_workspace_alloc(pasub->nea);
        priv->real = WavetableStorage::instance().getWavetable(pasub->nea);
        priv->count = pasub->noa;
        
        if(!priv->data || !priv->cmplx_coeffs || !priv->work || !priv->real)
            return -2;
     
        priv->worker = epicsThreadMustCreate("fftworker", epicsThreadPriorityScanHigh,
                                             epicsThreadGetStackSize(epicsThreadStackSmall),
                                             &fftworker, priv);
            
        pasub->dpvt = priv;
        
    }
    
    if(!pasub->pact) {
      // start async
//        fprintf(stderr, "%s: Start\n", pasub->name);
        priv->count = pasub->noa;
        memcpy(priv->data, fPtrA, sizeof(double)*pasub->noa);
        
        epicsEventSignal(priv->start); 

        pasub->pact = 1;
    } else {
        pasub->pact = 0;
        // finish
        for(size_t i = 0; i < priv->count; i++)
        {
            fPtrValA[i] = sqrt(priv->cmplx_coeffs[2*i]  * priv->cmplx_coeffs[2*i] +
                               priv->cmplx_coeffs[2*i+1]* priv->cmplx_coeffs[2*i+1]) / priv->count;
        }
        pasub->nova = priv->count;
//        fprintf(stderr, "%s: Stop\n", pasub->name);
    }                 
    
    return 0;
}

static long subApplyCoefficient(aSubRecord *pasub)
{
    float* fPtrA      = (float*)pasub->a;
    double coeff      = *(double*)pasub->b;
    double offset     = *(double*)pasub->c;
    char isInverse    = *(char*)pasub->d;
    float* fPtrValA   = (float*)pasub->vala;
    epicsUInt32 count = pasub->noa;
    unsigned int i;

    if (count != pasub->nova)
    {
        printf("Elements count mismatch\n");
        //TODO What should return function in case of error?
        return 0;
    }
    
    if ((isInverse && coeff == 0.0) || (offset == 0.0 && coeff == 1.0))
    {
        memcpy(fPtrValA, fPtrA, count * sizeof(float));
        return 0;
    }    
    
    for(i = 0; i < count; i++)
    {
        if (isInverse)
            fPtrValA[i] = (fPtrA[i] - offset) / coeff;
        else
            fPtrValA[i] = (fPtrA[i] - offset) * coeff;
    }

    return 0;
}	

#define slicerCount 5

static long subSlice(aSubRecord *pasub)
{
    float* fPtrA           = (float*)pasub->a;
    epicsUInt32 points[slicerCount] = {
                              *(epicsUInt32 *)pasub->b,
                              *(epicsUInt32 *)pasub->c,
                              *(epicsUInt32 *)pasub->d,
                              *(epicsUInt32 *)pasub->e,
                              *(epicsUInt32 *)pasub->f/*,
                              *(epicsUInt32 *)pasub->g,
                              *(epicsUInt32 *)pasub->h,
                              *(epicsUInt32 *)pasub->i,
                              *(epicsUInt32 *)pasub->j,
                              *(epicsUInt32 *)pasub->k*/
                             };
    float* fPtrVal[slicerCount] = {
                              (float *)pasub->valb,
                              (float *)pasub->valc,
                              (float *)pasub->vald,
                              (float *)pasub->vale,
                              (float *)pasub->valf/*,
                              (float *)pasub->valg,
                              (float *)pasub->valh,
                              (float *)pasub->vali,
                              (float *)pasub->valj,
                              (float *)pasub->valk*/
                             };
    int i;

    for(i = 0; i < slicerCount; i++)
    {
        *fPtrVal[i] = fPtrA[points[i]];
    }

    return 0;
}

static long subSampling(aSubRecord *pasub)
{
    float* fPtrA             = (float*)pasub->a;
    epicsUInt32 initialCount = pasub->noa;
    epicsUInt32 divider      = *(epicsUInt32*)pasub->b;
    float* fPtrValA          = (float*)pasub->vala;
    epicsUInt32 finalCount   = pasub->nova;
    unsigned int i;

    int shift = 4; //4
    for(i = 9-shift; i < initialCount && i/divider < finalCount; i+=divider)
    {
        fPtrValA[i/divider] = fPtrA[i];
    }

    return 0;
}

static long subArraySubtraction(aSubRecord *pasub)
{
    float* fPtrA      = (float*)pasub->a;
    float* fPtrB      = (float*)pasub->b;
    float* fPtrValA   = (float*)pasub->vala;
    epicsUInt32 count = pasub->noa;
    unsigned int i;

    if (count != pasub->nob || count != pasub->nova)
    {
        printf("Elements count mismatch\n");
        //TODO What should return function in case of error?
        return 0;
    }

    for(i = 0; i < count; i++)
    {
        fPtrValA[i] = fPtrA[i] - fPtrB[i];
    }

    return 0;
}

static long subArrayMovingAverage(aSubRecord *pasub)
{
    float* fPtrA       = (float*)pasub->a;
    int window         = *(epicsUInt32*)pasub->b;
    int count          = pasub->noa;
    float* fPtrValA    = (float*)pasub->vala;
    long* fPtrValB     = (long*)pasub->valb;
    int i, j;
    int left, right;
    double sum;
    
    window = window / 2;
    
    // return real window size
    *fPtrValB = window*2+1;
    
    if (window < 0)
    {
        printf("Window should be equal or greater then zero\n");
        //TODO What should return function in case of error?
        return 0;
    }

    if (count != pasub->nova)
    {
        printf("Elements count mismatch\n");
        //TODO What should return function in case of error?
        return 0;
    }
    
    // optimization for case when window == 0
    if (window == 0)
    {
        memcpy(fPtrValA, fPtrA, count * sizeof(float));
        
        return 0;
    }

    for(i = 0; i < count; i++)
    {
        int l,r;
        
        left  = i - window > 0     ? i - window   : 0;
        l     = i - window > 0     ? 0            : 0 - (i - window);

        right = i + window < count ? i + window   : count-1;
        r     = i + window < count ? 0            : (i + window) - (count-1);
                
        if ( l > r ) 
            right -= l; 
        else
            left += r;
        
        sum = 0.0;

        for(j = left; j <= right; j++)
            sum += fPtrA[j];
            fPtrValA[i] = sum / (right-left+1);
        }

    return 0;
}

static long subArrayStatsVariance(aSubRecord *pasub)
{
    int count = pasub->noa;

    double mean = gsl_stats_float_mean((float*)pasub->a,
                                       1,
                                       count);

    *(double*)pasub->vala = gsl_stats_float_sd_with_fixed_mean((float*)pasub->a,
                                                               1,
                                                               count,
                                                               mean);
    *(double*)pasub->valb = mean;

    return 0;
}

// fills an outputs waveform by zeros
static long subZero(aSubRecord *pasub)
{
    float* fPtrValA   = (float*)pasub->vala;
    float* fPtrValB   = (float*)pasub->valb;
    float* fPtrValC   = (float*)pasub->valc;
    epicsUInt32 count = pasub->nova;
    unsigned int i;

    for(i = 0; i < count; i++)
    {
        fPtrValA[i] = 0.0;
    }
    memcpy(fPtrValB, fPtrValA, count * sizeof(float));
    memcpy(fPtrValC, fPtrValA, count * sizeof(float));
    
    return 0;
}

static long subArrayCompare(aSubRecord *pasub)
{
    float* fTol       = (float*)pasub->a; // tolerance 
    float* fSub       = (float*)pasub->b; // differential signal
    float  coeff      = *(float*)pasub->c; // coefficient
    char* fPtrValA    = (char*)pasub->vala;
    epicsUInt32 count = pasub->noa;
    unsigned int i;
    int status = 0;

    if (count != pasub->nob)
    {
        printf("subArrayCompare: Elements count mismatch\n");
        //TODO What should return function in case of error?
        return 0;
    }

    for(i = 0; i < count; i++)
    {
        if ( fTol[i] <= 0.0 )   // wrong tollerance table
        {
            status = -2.0;
            break;
        }
            
        if (fTol[i] < fabs(fSub[i]))
        {
            status = 1;         // out of tollerance
        }
        
        if (fTol[i] * coeff < fabs(fSub[i]))
        {
            status = 2;         // out of (tollerance * coeff)
            break;
        }
    }
    
    *fPtrValA = status;

    return 0;
}

static long subCopyWaveform(aSubRecord *pasub)
{
    float* fPtrA      = (float*)pasub->a;
    float* fPtrB      = (float*)pasub->b;
    char   sel        = *(char*)pasub->c;
    float* fPtrValA   = (float*)pasub->vala;

    if ( pasub->noa != pasub->nova || pasub->nob != pasub->nova)
    {
        printf("subCopyWaveform: Elements count mismatch\n");
        //TODO What should return function in case of error?
        return 0;
    }

    if ( sel == 1 )
    {
	memcpy(fPtrValA, fPtrA, sizeof(float) * pasub->noa);
    }

    if ( sel == 2 )
    {
	memcpy(fPtrValA, fPtrB, sizeof(float) * pasub->noa);
    }

    return 0;
}

epicsRegisterFunction(subADCWfProc);
epicsRegisterFunction(subApplyCoefficient);
epicsRegisterFunction(subFFT);
epicsRegisterFunction(subSlice);
epicsRegisterFunction(subSampling);
epicsRegisterFunction(subArraySubtraction);
epicsRegisterFunction(subArrayMovingAverage);
epicsRegisterFunction(subArrayStatsVariance);
epicsRegisterFunction(subZero);
epicsRegisterFunction(subArrayCompare);
epicsRegisterFunction(subCopyWaveform);
