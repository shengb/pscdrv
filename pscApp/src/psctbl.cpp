#include "psc/device.h"
#include "psc/devcommon.h"

#include "menuFtype.h"
#include "waveformRecord.h"
#include "stringinRecord.h"
#include "mbboRecord.h"
#include "mbbiDirectRecord.h"
#include "aoRecord.h"
#include "longoutRecord.h"

#include <gsl/gsl_statistics_float.h>
#include <gsl/gsl_linalg.h>
#include <math.h>

extern "C" int PSCTblCtlDebug; // 0 .. 4
int PSCTblCtlDebug = 1;

#define ALARM_RET(PREC, ALARM_TYPE, ALARM_SEV) \
    {                                                           \
        (void)recGblSetSevr(PREC, ALARM_TYPE, ALARM_SEV);       \
        return 0;                                               \
    }

namespace {

    const int maxPscPointCount = 10860; 

struct TableController
{
    PSC *psc;
    PSC *rxpsc;
    epicsUInt16 bid;
    
    enum mode_t {modeImm, modeAuto, modeMan};
    
    std::vector<float> next_tr;
//    std::vector<float> prev_tr;
    std::vector<float> next_tg;
    std::vector<float> prev_tg;
    mode_t curmode;
    float max_delta1;   // maximal delta for function
    float max_delta2;   // maximal delta for derivative
    int start_point;    // start point for transition table
    
    IOSCANPVT errorscan;
    epicsUInt16 alarmBits;
    
    TableController(PSC *psc, PSC *rxpsc, epicsUInt16 bid)
    :psc(psc)
    ,rxpsc(rxpsc)
    ,bid(bid)
    ,next_tr()
//    ,prev_tr()
    ,next_tg()
    ,prev_tg()
    {
        scanIoInit(&errorscan);
    }
};
 
/*
struct TableRec {
  TableController *ctrl;
  // extra per record
};
*/

typedef std::map<std::pair<PSC*,int>, TableController*> tcmap_t;
tcmap_t tcmap;

int parse_link_table(dbCommon* prec, const char* link)
{
    std::istringstream strm(link);

    std::string /*tx*/name, rxname;
    unsigned int block;

    strm >> name >> rxname >> block;
    if(strm.bad()) {
        timefprintf(stderr, "%s: Error Parsing: '%s'\n",
                prec->name, link);
        return 1;
    }
    
    PSC *psc, *rxpsc;

    psc = PSC::getPSC<PSC>(name);
    rxpsc = PSC::getPSC<PSC>(rxname);
    if(!psc || !rxpsc) {
        timefprintf(stderr, "%s: PSC '%s/%s' not found\n",
                prec->name, name.c_str(), rxname.c_str());
        return 1;
    }
    
    TableController *priv;
    
    tcmap_t::iterator it = tcmap.find(std::make_pair(psc,block));
    if(it!=tcmap.end()) {
        priv = it->second;
    } else {
        if (PSCTblCtlDebug > 0)
            timefprintf(stderr, "Create TC with %p %d\n", (void*)psc, block);
        priv = new TableController(psc,rxpsc,block);
        tcmap[std::make_pair(psc,block)] = priv;
    }
    
    prec->dpvt = (void*)priv;

    return 0;
}

long init_wf_record(waveformRecord* prec)
{
    assert(prec->inp.type==INST_IO);
    if(prec->ftvl!=menuFtypeFLOAT) {
        timefprintf(stderr, "%s: FTVL must be FLOAT\n", prec->name);
        return 0;
    }
    try {
        parse_link_table((dbCommon*)prec, prec->inp.value.instio.string);
    }CATCH(init_wf_record, prec)
    
    return 0;
}

long init_mbbo_record(mbboRecord* prec)
{
    assert(prec->out.type==INST_IO);
    try {
        parse_link_table((dbCommon*)prec, prec->out.value.instio.string);
    }CATCH(init_mbbo_record, prec)
    
    return 0;
}

long init_ao_record(aoRecord* prec)
{
    assert(prec->out.type==INST_IO);
    try {
        parse_link_table((dbCommon*)prec, prec->out.value.instio.string);
    }CATCH(init_ao_record, prec)
    
    return 2;
}

long init_lo_record(longoutRecord* prec)
{
    assert(prec->out.type==INST_IO);
    try {
        parse_link_table((dbCommon*)prec, prec->out.value.instio.string);
    }CATCH(init_ao_record, prec)
    
    return 0;
}


long init_mbbiDirect_record(mbbiDirectRecord* prec)
{
    assert(prec->inp.type==INST_IO);
    try {
        parse_link_table((dbCommon*)prec, prec->inp.value.instio.string);
    }CATCH(init_mbbiDirect_record, prec)
    
    return 0;
}

long proc_set_mode(mbboRecord* prec)
{
    TableController *priv=(TableController*)prec->dpvt;
    if(!priv)
        return 0;
    try {
        Guard g(priv->psc->lock);
        
        if(prec->rval<0 || prec->rval>2)
            ALARM_RET(prec, WRITE_ALARM, INVALID_ALARM)
        
        priv->curmode = (TableController::mode_t)prec->rval;
        
        if (PSCTblCtlDebug > 1)
        {
            timefprintf(stderr, "TC(%p %d): mode=", (void*)priv->psc, priv->bid);
            switch (priv->curmode) 
            {
                case TableController::modeImm:  timefprintf(stderr, "modeImm\n"); 
                break;
                        
                case TableController::modeAuto: timefprintf(stderr, "modeAuto\n"); 
                break;
                        
                default:       timefprintf(stderr, "unknown mode O_o\n");
            }
        }
                                                          

        
    }CATCH(proc_set_mode, prec)
    return 0;
}

long proc_set_max_delta1(aoRecord* prec)
{
    TableController *priv=(TableController*)prec->dpvt;
    if(!priv)
        return 0;
    try {
        Guard g(priv->psc->lock);
         
        if( prec->val < 0 )
            ALARM_RET(prec, WRITE_ALARM, INVALID_ALARM)
              
        priv->max_delta1 = prec->val;
        
        if (PSCTblCtlDebug > 1)
            timefprintf(stderr, "TC(%p %d): max_delta1=%f\n", (void*)priv->psc, 
                                                          priv->bid, 
                                                          priv->max_delta1);

        
    }CATCH(proc_set_max_delta1, prec)
    return 0;
}

long proc_set_max_delta2(aoRecord* prec)
{
    TableController *priv=(TableController*)prec->dpvt;
    if(!priv)
        return 0;
    try {
        Guard g(priv->psc->lock);

        if( prec->val < 0 )
            ALARM_RET(prec, WRITE_ALARM, INVALID_ALARM)
                
        priv->max_delta2 = prec->val;
        
        if (PSCTblCtlDebug > 1)
            timefprintf(stderr, "TC(%p %d): max_delta2=%f\n", (void*)priv->psc, 
                                                          priv->bid, 
                                                          priv->max_delta2);
        
    }CATCH(proc_set_max_delta2, prec)
    return 0;
}

long proc_set_start_point(longoutRecord* prec)
{
    TableController *priv=(TableController*)prec->dpvt;
    if(!priv)
        return 0;
    try {
        Guard g(priv->psc->lock);

        if( prec->val < 0 )
            ALARM_RET(prec, WRITE_ALARM, INVALID_ALARM)
                
        priv->start_point = prec->val;
        
        if (PSCTblCtlDebug > 1)
            timefprintf(stderr, "TC(%p %d): start_point=%d\n", (void*)priv->psc,
                                                           priv->bid, 
                                                           priv->start_point);
        
    }CATCH(proc_set_max_delta2, prec)
    return 0;
}

static long calcTransientTable(TableController* priv)
{
    float* ptrNew     = priv->next_tg.data();
    float* ptrOld     = priv->prev_tg.data();
    int x0            = priv->start_point;
    float* ptrTran    = priv->next_tr.data();
    
//    epicsUInt32 count = pasub->noa;
    int count = priv->next_tr.size();
    
    int i;

    int x1 = count;               // final X point (same as 0-th point of the 
                                  // next ramp). [Early it was x1=count-1]
    
    float y0 = ptrOld[x0];        // value in the begin of transition
    float y1 = ptrNew[0];         // value in the end of transition

    float r0 = ptrOld[x0+1] - ptrOld[x0];
    float r1 = ptrNew[1] - ptrNew[0];
    
//    timeprintf("x0=%d, x1=%d, y0=%f, y1=%f, r0=%f, r1=%f\n", x0, x1, y0, y1, r0, r1);
    
    if (x0 > x1) // nothing to do
	return 0;

    const double x0f = x0, x1f = x1;
    double a_data[] = {    pow(x0,5),    pow(x0,4),   pow(x0,3), pow(x0,2), x0f, 1,
                         5*pow(x0,4),  4*pow(x0,3), 3*pow(x0,2),     2*x0f,   1, 0,
                        20*pow(x0,3), 12*pow(x0,2),       6*x0f,         2,   0, 0,
                        20*pow(x1,3), 12*pow(x1,2),       6*x1f,         2,   0, 0,
                         5*pow(x1,4),  4*pow(x1,3), 3*pow(x1,2),     2*x1f,   1, 0,
                           pow(x1,5),    pow(x1,4),   pow(x1,3), pow(x1,2), x1f, 1
                       };

    double b_data[] = { y0, r0, 0, 0, r1, y1 };

    gsl_matrix_view m = gsl_matrix_view_array (a_data, 6, 6);
    gsl_vector_view b = gsl_vector_view_array (b_data, 6);
    gsl_vector *x = gsl_vector_alloc (6);

    int s;

    gsl_permutation * p = gsl_permutation_alloc (6);
    gsl_linalg_LU_decomp (&m.matrix, p, &s);
    gsl_linalg_LU_solve (&m.matrix, p, &b.vector, x);

//    timeprintf ("x = \n");
//    gsl_vector_timefprintf (stdout, x, "%g");
     
    priv->next_tr = priv->prev_tg;
    for (i = x0; i < count; i++)
    {
        ptrTran[i] = gsl_vector_get(x, 0)*pow(i, 5) +
                     gsl_vector_get(x, 1)*pow(i, 4) +
                     gsl_vector_get(x, 2)*pow(i, 3) +
                     gsl_vector_get(x, 3)*pow(i, 2) +
                     gsl_vector_get(x, 4)*        i +
                     gsl_vector_get(x, 5);
    }

    gsl_permutation_free (p);
    gsl_vector_free (x);

    return 0;
}

static inline uint32_t to_network_byte_order32(uint32_t value)
{
    uint32_t result = 0;
    uint8_t *temp = (uint8_t *) (&result);

    *temp = (uint8_t) ((value >> 24) & 0x000000ff);temp++;
    *temp = (uint8_t) ((value >> 16) & 0x000000ff);temp++;
    *temp = (uint8_t) ((value >> 8) & 0x000000ff);temp++;
    *temp = (uint8_t) (value & 0x000000ff);

    return result;
}

int sub10kPack__(float* inp, void* out, int count, int isUniPol)
{
    const double convert_coeff = 0.000009536752259; // 10 / (2^20 - 1)   
    int i;
    float tmpF;
    uint32_t tmp               = 0;
    
//    timeprintf("count = %d\n", count);
    
    for(i = 0; i < count; i++)
    {

        tmpF = *(float*)(inp+i);
        tmpF = isUniPol?(tmpF / convert_coeff):((tmpF+10.0)/(convert_coeff*2.0));
        tmp = (int)(tmpF+0.5);
        *(uint32_t*)((unsigned char*)out+sizeof(uint32_t)*i) = 
                                                   to_network_byte_order32(tmp);
    }
    
    // fill in all rest points with the last valid value
    for(i = count; i < maxPscPointCount; i++)
    {
        *(uint32_t*)((unsigned char*)out+sizeof(uint32_t)*i) =
                                                   to_network_byte_order32(tmp);
    }

    // Add magic words
    ((unsigned char*)out)[0]     = 0xff;
    ((unsigned char*)out)[43436] = 0xff;
    ((unsigned char*)out)[43437] = 0xff;
    ((unsigned char*)out)[43438] = 0xff;
    ((unsigned char*)out)[43439] = 0xff;
    
    return 1;
}

// check_ramp_function get two waveforms v1 and v2 and perfom test for absolute 
// values, maximal step and angularity (how fast ramp curve can bend).
// It's assumed that v2 goes after v1 in time and this function also perform the
// conjunction of these functions.
// 
// Args: 
// v1 and v2  - ramp functions
// isUniPol   - is this PSC/PSI unipolar?
// max_delta1 - defines the maximal step in volts per one time step
// max_delta2 - defines the maximal change in volts between two consecutive
//              steps, i.e. (y1-y0) - (y2-y1)
//
// Return values:
// 0  - ok
// <0 - everything ok, but it's needed to calc transient table between v1 and v2
// 1  - absoulete value of some point of v1 is out of allowed range
// 2  - somewhere step exceeded max_delta1
// 3  - angularity too high

int check_ramp_function(const std::vector<float>& v1, const std::vector<float>& v2,
                        bool isUniPol, float max_delta1, float max_delta2)
{
    std::vector<float> vector(v1.begin(), v1.end());
    vector.push_back(v2[0]);
    vector.push_back(v2[1]);

    // vector.end()-4 - 10148-th point
    // vector.end()-3 - 10149-th point
    // vector.end()-2 - 0-th point
    // vector.end()-1 - 1-st point
    std::vector<float>::iterator it_x0 = vector.begin();
    std::vector<float>::iterator it_x1 = vector.begin() + 1;
    std::vector<float>::iterator it_x2 = vector.begin() + 2;
    int count = 0;
    for (; it_x0 != vector.end()-2; ++it_x0, ++it_x1, ++it_x2, count++)
    {

//        timeprintf("(%f, %f, %f) ", *it_x0, *it_x1, *it_x2);
        if ( *it_x0 > 9.9999 || (isUniPol && *it_x0 < 0.0) || (!isUniPol && *it_x0 < -10.0 ) )
        {
            if (PSCTblCtlDebug > 2)
                timefprintf(stderr, "Target table has at least one point (x=%d, y=%f) outside of the allowed range. Table will be declined.\n", count, *it_x0);
            return 1;
        }

        // delta check ("first derivative")
        if (fabsf(*it_x1 - *it_x0) > max_delta1)
        {
            if (it_x0 == vector.end()-3)
            {
                if (PSCTblCtlDebug > 2)
                    timefprintf(stderr, "Volts per step exceeded maximal allowed value (%f v/step) at the ramp closing.\n", max_delta1);
                return -1;
            }

            if (PSCTblCtlDebug > 2)
                timefprintf(stderr, "Volts per step exceeded maximal allowed value (%f v/step) at point x=%d.\n", max_delta1, count);
            
            return 2;
        }

        // curvature check ("second derivative")
        if (fabsf((*it_x1 - *it_x0) - (*it_x2 - *it_x1)) > max_delta2)
        {
            if (it_x0 == vector.end()-4)
            {
                if (PSCTblCtlDebug > 2)
                    timefprintf(stderr, "Ramp function angularity exceeded maximal allowed value (%f) at the very end.\n", max_delta2);
                return -2;
            }
            
            if (it_x0 == vector.end()-3)
            {
                if (PSCTblCtlDebug > 2)
                    timefprintf(stderr, "Ramp function angularity exceeded maximal allowed value (%f) at the very begin.\n", max_delta2);
                return -3;
            }
            
            if (PSCTblCtlDebug > 2)
                timefprintf(stderr, "Ramp function angularity exceeded maximal allowed value (%f) at point x=%d.\n", max_delta2, count);

            return 3;
        }
    }
    
    return 0; 
}

long proc_update_target(waveformRecord* prec)
{
    int const status_message_id     = 64;
    int const reg_polarity_offset   = 40;
    int const reg_mask_polarity     = 0x10000;
    int const psc_table_mode_offset = 4;
    
    
    enum {
        decline             = 0,
        cantSend            = 1,
        targetTableWrong    = 2,
        transientTableWrong = 3,
        transientTableUsed  = 4,
        unexplainable       = 5 
    };

    TableController *priv=(TableController*)prec->dpvt;
    if(!priv)
        return 0;
    try {
        bool isUniPol;
        priv->alarmBits = 0;
        
        {
            Guard g(priv->rxpsc->lock);
        
            Block *regblk = priv->rxpsc->getRecv(status_message_id);
            
            if(!regblk || regblk->data.size() < reg_polarity_offset+sizeof(epicsUInt32)) 
            {
                priv->alarmBits |= 1 << cantSend;
                priv->alarmBits |= 1 << decline;
                scanIoRequest(priv->errorscan);
                if (PSCTblCtlDebug > 0)
                    timefprintf(stderr, "%s: Can't send dac before first register read! %lu\n",
                            prec->name, (unsigned long)regblk->data.size());
                
                ALARM_RET(prec, WRITE_ALARM, MAJOR_ALARM)
            }
            
            // extract PSI polarity from the received message form PSC
            epicsUInt32 regval = ntohl(*(epicsUInt32*) & regblk->data[reg_polarity_offset]);
            isUniPol = regval & reg_mask_polarity;
        }
        
        Guard g(priv->psc->lock);
        
        // tr - transient table
        // tg - target table

        bool send_tr=false;
        bool send_tg=false;
        
        priv->next_tg.resize(prec->nord);
        priv->prev_tg.resize(prec->nord);
        priv->next_tr.resize(prec->nord);
        
        std::copy((float*)prec->bptr, (float*)prec->bptr + prec->nord, 
                                       priv->next_tg.begin());
           
        int res = check_ramp_function(priv->next_tg, priv->next_tg, isUniPol, 
                                      priv->max_delta1, priv->max_delta2);
        if (PSCTblCtlDebug > 3)
            timeprintf("target table general check = %d\n", res);
        
        if ( res != 0 ) {
            // target table is wrong
            if (PSCTblCtlDebug > 2)
                timefprintf(stderr, "Target table was declined.\n");
            priv->alarmBits |= 1 << decline;
            priv->alarmBits |= 1 << targetTableWrong;
            scanIoRequest(priv->errorscan);
            ALARM_RET(prec, WRITE_ALARM, MAJOR_ALARM)
        }
        
        // well, target table is ok
        
        res = check_ramp_function(priv->prev_tg, priv->next_tg, isUniPol, 
                                  priv->max_delta1, priv->max_delta2);
        if (PSCTblCtlDebug > 3)
            timeprintf("target table conjunction check = %d\n", res);
             
        if ( priv->curmode == TableController::modeImm )
        {   
            if ( res > 0 ) { // we allow discontinuity during switching to the
                             // new ramp
                // if we are here it means that something unexpected occured
                if (PSCTblCtlDebug > 2)
                    timefprintf(stderr, "Target table was declined.\n");
                priv->alarmBits |= 1 << decline;
                priv->alarmBits |= 1 << unexplainable; // unexplainable error
                scanIoRequest(priv->errorscan);
                ALARM_RET(prec, WRITE_ALARM, MAJOR_ALARM)
            }
            
            if (PSCTblCtlDebug > 2 && res < 0)
                timefprintf(stderr, "Target table was accepted because of the Immediate mode.\n");
                
            send_tg = true;
        }
        else if ( priv->curmode == TableController::modeAuto )
        {
            if ( res < 0 ) {
                // we need to calc and insert transient table
                calcTransientTable(priv);
                if (PSCTblCtlDebug > 2)
                    timeprintf("Transient table was calculated\n");
                
                res = check_ramp_function(priv->next_tr, priv->next_tg, isUniPol, 
                                          priv->max_delta1, priv->max_delta2);
                if (PSCTblCtlDebug > 3)                         
                    timeprintf("transient table check = %d\n", res);
                
                if ( res != 0 )
                {   
                    if (PSCTblCtlDebug > 2)
                        timefprintf(stderr, "Transient table is not correct. Decline all.\n");
                    priv->alarmBits |= 1 << decline;
                    priv->alarmBits |= 1 << transientTableWrong; // transient table wrong
                    scanIoRequest(priv->errorscan);
                    ALARM_RET(prec, WRITE_ALARM, MAJOR_ALARM)
                }
                
                send_tg = true;
                send_tr = true;
            }
            else if ( res == 0 )
            {
                // we don't need to calc transient table because there is no 
                // gap between previous target table and a new one
                send_tg = true;
                send_tr = false;
            } else if ( res > 0 )
            {
                if (PSCTblCtlDebug > 1)
                    timefprintf(stderr, "Strange condition with ramp downloading\n");
                priv->alarmBits |= 1 << decline;
                priv->alarmBits |= 1 << unexplainable;
                scanIoRequest(priv->errorscan);
                ALARM_RET(prec, WRITE_ALARM, MAJOR_ALARM)
            }
            
        }
        
        if(send_tg)
        {
            // we have to send to PSC as much data (points) as maxPscPointCount
            // each point span 4 bytes
            std::vector<char> tmp;
            tmp.resize(maxPscPointCount * sizeof(epicsUInt32));
            
            if(send_tr)
            {
                priv->alarmBits |= 1 << transientTableUsed;
                
                // convert to PSC format
                // third argument menans a valid number of points
                // all remaining points will be filled with the las valid value
                sub10kPack__(priv->next_tr.data(), tmp.data(), prec->nord, isUniPol);
                
                tmp[psc_table_mode_offset] = 0x02; // mark this table as 
                                                   // transinet table
                
                // for sending targent and transient uses same MSGID (first arg)
                // meaning (type) of table is inside the table itself
                priv->psc->PSCBase::queueSend(priv->bid, tmp.data(), tmp.size());
                priv->psc->flushSend();
                
                if (PSCTblCtlDebug > 2)
                    timeprintf("Transient table has been sent\n");
            }
            
            sub10kPack__(priv->next_tg.data(), tmp.data(), prec->nord, isUniPol);
            
            if (send_tr)
                tmp[psc_table_mode_offset] = 0x01; // mark this table as 
                                                   // target table
            else
                tmp[psc_table_mode_offset] = 0x00; // mark this table as 
                                                   // immediate target table
            
            priv->psc->PSCBase::queueSend(priv->bid, tmp.data(), tmp.size());
            priv->psc->flushSend();
            
            if (PSCTblCtlDebug > 2)
                timeprintf("Target table has been sent\n");

            priv->prev_tg = priv->next_tg;
        }
        
        scanIoRequest(priv->errorscan);
        
    }CATCH(proc_update_target, prec)
    return 0;
}

long proc_read_alarms(mbbiDirectRecord *prec)
{
    TableController *priv=(TableController*)prec->dpvt;
    if(!priv)
        return 0;
    try {
        Guard g(priv->psc->lock);
        prec->rval = priv->alarmBits;
    }CATCH(proc_update_target, prec)
    return 0;
}

long intr_info_alarms(int dir, dbCommon *prec, IOSCANPVT *io)
{
    TableController *priv=(TableController*)prec->dpvt;
    if(!priv)
        return 0;
    
    *io = priv->errorscan;
    
    return 0;
}

MAKEDSET(waveform, devPSCTblOutWfTG,  &init_wf_record,   NULL, &proc_update_target);
MAKEDSET(mbbo,     devPSCTblMbboMode, &init_mbbo_record, NULL, &proc_set_mode);
MAKEDSET(ao,       devPSCTblAo1Mode,  &init_ao_record,   NULL, &proc_set_max_delta1);
MAKEDSET(ao,       devPSCTblAo2Mode,  &init_ao_record,   NULL, &proc_set_max_delta2);
MAKEDSET(longout,  devPSCTblLoStart,  &init_lo_record,   NULL, &proc_set_start_point);

MAKEDSET(mbbiDirect, devPSCTblMbbiDirectAlarms, &init_mbbiDirect_record, &intr_info_alarms, &proc_read_alarms);

} // namespace

#include <epicsExport.h>

epicsExportAddress(dset, devPSCTblOutWfTG);
epicsExportAddress(dset, devPSCTblMbboMode);
epicsExportAddress(dset, devPSCTblAo1Mode);
epicsExportAddress(dset, devPSCTblAo2Mode);
epicsExportAddress(dset, devPSCTblLoStart);

epicsExportAddress(dset, devPSCTblMbbiDirectAlarms);

epicsExportAddress(int,  PSCTblCtlDebug);
