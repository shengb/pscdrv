
#ifdef  DEBUG_PRINT
#include <stdio.h>
  int fbckSupFlag = DP_ERROR;
#endif

#include <string.h>       
#include <math.h>          
#include <stdlib.h>        

#include <subRecord.h>        
#include <registryFunction.h> 
#include <epicsExport.h>      
#include <epicsTime.h>        
#include <dbAccess.h>         
#include <alarm.h>            

#ifndef dbGetPdbAddrFromLink
#define dbGetPdbAddrFromLink(PLNK) \
    ( ( (PLNK)->type != DB_LINK ) \
      ? 0 \
      : ( ( (struct dbAddr *)( (PLNK)->value.pv_link.pvt) ) ) )
#endif

static int fbckSupInit(subRecord *psub)
{

  return 0;
}


double  RootMeanSquare ( double *  pSrc,
	 int SampleLength)

{
	 int	i;
	 double	Sum;


	Sum = (*pSrc) * (*pSrc);
	pSrc++;

	for (i = 1; i < SampleLength; i++)
	{
#if (SIGLIB_ARRAY_OR_PTR == SIGLIB_ARRAY_ACCESS)		/* Select between array index or pointer access modes */
		Sum += pSrc[i] * pSrc[i];
#else
		Sum += (*pSrc) * (*pSrc);
		pSrc++;
#endif
	}

	return (sqrt (Sum / ((double)SampleLength)));
}				


static long fbckSupRMSCalc(subRecord *psub)
{
  DBADDR *sqhstAddr = dbGetPdbAddrFromLink(&psub->inpa);
  double *sqhst_p;
  int i = 0;
  double sum_sqhst = 0;

  /*printf("starting longCalc\n");*/

  if (!sqhstAddr) return -1;

  /*  printf("compress record addrs are good\n");*/

  sqhst_p = (double *)sqhstAddr->pfield;
  if (!sqhst_p) return -1;

  /*printf("hst pointers and sizes are good\n");*/

  if (sqhstAddr->no_elements <=0) {
	psub->val = 0;
  }
  else { 
	/* now calculate */
#if 0	
	for ( i=0; i < sqhstAddr->no_elements; i++) {
	  sum_sqhst += sqhst_p[i];
	}
  
    /* calc and report the value as a percentage */
	psub->val = 100*(sqrt(sum_sqhst / sqhstAddr->no_elements));
#endif	
	psub->val = RootMeanSquare(sqhst_p, sqhstAddr->no_elements);
  }

  return 0;
}

epicsRegisterFunction(fbckSupInit);
epicsRegisterFunction(fbckSupRMSCalc);

