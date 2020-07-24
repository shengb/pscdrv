#ifndef RECRUNNER_H
#define RECRUNNER_H

#include <set>
#include <list>

#include <dbScan.h>
#include <epicsThread.h>

class dbCommon;

class ScanList
{
    typedef std::list<dbCommon*> records_t;
    records_t records;
    IOSCANPVT scan;

    epicsMutex lock;

    bool scanning; // flag to detect recursion
    records_t addset, delset;
public:
    ScanList();

    void addRecord(dbCommon*);
    void delRecord(dbCommon*);

    void process();
};


class RecRunner : public epicsThreadRunnable
{
    epicsThread runner;
    epicsMutex guard;
public:
    RecRunner();

    void
};

#endif // RECRUNNER_H
