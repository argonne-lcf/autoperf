/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef __DARSHAN_APXC_LOG_FORMAT_H
#define __DARSHAN_APXC_LOG_FORMAT_H

/* current AutoPerf Cray XC log format version */
#define DARSHAN_APXC_VER 1

#define APXC_RTR_COUNTERS \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC0) \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC1) \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC2) \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC3) \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC4) \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC5) \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC6) \
    X(AR_RTR_0_0_INQ_PRF_INCOMING_FLIT_VC7) \
    X(AR_RTR_0_0_INQ_PRF_ROWBUS_STALL_CNT) \
    /* end of counters */\
    X(APXC_RTR_NUM_INDICES)

#define X(a) a,
/* integer counters for the "BGQ" example module */
enum darshan_apxc_rtr_indices
{
    APXC_RTR_COUNTERS
};
#undef X

/* the darshan_apxc_router_record structure encompasses the data/counters
 * which would actually be logged to file by Darshan for the AP Cray XC
 * module. This example implementation logs the following data for each
 * record:
 *      - a darshan_base_record structure, which contains the record id & rank
 *      - integer I/O counters 
 *      - floating point I/O counters 
 */
struct darshan_apxc_router_record
{
    struct darshan_base_record base_rec;
    int64_t coord[4]; /* ugroup, uchassis, ublade, unode */
    int64_t counters[APXC_RTR_NUM_INDICES];
};

struct darshan_apxc_header_record
{
    struct darshan_base_record base_rec;
    int64_t nblades;
    int64_t nchassis;
    int64_t ngroups;
};

#endif /* __DARSHAN_APXC_LOG_FORMAT_H */
