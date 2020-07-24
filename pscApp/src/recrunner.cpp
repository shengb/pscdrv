
#include "recrunner.h"

#include <dbCommon.h>

static bool dbCommonLess(dbCommon* A, dbCommon* B)
{
    if(A->prio < B->prio)
        return true;
    return A < B;
}

static bool dbCommonEqual(dbCommon* A, dbCommon* B)
{
    return A == B;
}

RecRunner::RecRunner()
    :scanning(false)
{
    scanIoInit(&scan);
}

void RecRunner::addRecord(dbCommon *prec)
{
    Guard g(lock);
    if(scanning)
        addset.push_back(prec);
    else
        records.push_back(prec);
}

void RecRunner::delRecord(dbCommon *prec)
{
    Guard g(lock);
    if(scanning) {
        delset.push_back(prec);
        return;
    }

    records_t::iterator it=records.find(prec);
    if(it!=records.end())
        records.erase(it);
}

void RecRunner::process()
{
    Guard g(lock);
    scanning = true;
    try {

        records.sort(&dbCommonLess);
        records.unique(&dbCommonEqual);

        records_t::const_iterator it, end=records.end();
        for(it=records.begin(); it!=end; ++it) {
            dbScanLock(*it);
            dbProcess(*it);
            dbScanUnlock(*it);
        }

        records.splice(records.begin(), addset);
        {
            records_t::const_iterator it, end=delset.end();
            for(it=delset.begin(); it!=end; ++it)
                records.remove(*it);
        }

    } catch(...) {
        scanning = false;
        throw;
    }
    scanning = false;
}
