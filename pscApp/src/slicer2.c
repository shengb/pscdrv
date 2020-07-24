#include <stdio.h>

#include <epicsExport.h>
#include <aSubRecord.h>
#include <registryFunction.h>

static long subSlice2(aSubRecord *pasub)
{
    float* fPtrA           = (float*)pasub->a;
    float* fPtrValA        = (float*)pasub->vala;
    epicsUInt32 idx        = *(epicsUInt32 *)pasub->b;

    if (idx < 0 || idx > (pasub->noa)-1)
    {
        fprintf(stderr, "subSlice2: index is out of range. Index=%d, range is %d .. %d\n", idx, 0, pasub->noa);
        return 0;
    }

    *fPtrValA = fPtrA[idx];

    return 0;
}

epicsRegisterFunction(subSlice2);
