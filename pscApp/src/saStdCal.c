/*
 *	Slow data standard deviation calculation
 *
 */
 
#ifdef  DEBUG_PRINT
#include <stdio.h>
  int fbckSupFlag = DP_ERROR;
#endif

/* c includes */
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

static int SaStdCalSupInit(subRecord *psub)
{
  return 0;
}


double  SampleSd (const double *  pSrc,	 int SampleLength)
{
	int	i;
	double	Sum, SquaredSum;

	Sum = *pSrc;
	SquaredSum = (*pSrc) * (*pSrc);
	pSrc++;

	for (i = 1; i < SampleLength; i++)
	{
		Sum += *pSrc;
		SquaredSum += (*pSrc) * (*pSrc);
		pSrc++;
	}

	return (sqrt ((SquaredSum -
			((Sum * Sum) / ((double)(SampleLength)))) / ((double)(SampleLength - 1))));

}

static long SaStdCalSup(subRecord *psub)
{
  DBADDR *saData = dbGetPdbAddrFromLink(&psub->inpa);
  double *saData_ptr;
  int i = 0;
  double sum_sqhst = 0;

  if (!saData) return -1;

  saData_ptr = (double *)saData->pfield;
  if (!saData_ptr) return -1;

  if (saData->no_elements <=0) {
	psub->val = 0;
  }
  else { 
	/* now calculate */
	//for ( i=0; i < saData->no_elements; i++) {
	//  sum_sqhst += saData_ptr[i];
	//}
  	psub->val = SampleSd(saData_ptr, saData->no_elements);
  }

  return 0;
}

epicsRegisterFunction(SaStdCalSupInit);
epicsRegisterFunction(SaStdCalSup);

