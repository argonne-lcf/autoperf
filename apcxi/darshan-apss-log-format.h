/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef __APCXI_LOG_FORMAT_H
#define __APCXI_LOG_FORMAT_H

/* current AutoPerf Cray XC log format version */
#define APCXI_VER 1

#define APCXI_MAGIC ('A'*0x100000000000000+\
                            'U'*0x1000000000000+\
                            'T'*0x10000000000+\
                            'O'*0x100000000+\
                            'P'*0x1000000+\
                            'E'*0x10000+\
                            'R'*0x100+\
                            'F'*0x1)

#define APCXI_PERF_COUNTERS \
    /* PAPI counters */\
    X(CQ_CQ_OXE_NUM_STALLS) \
    X(CQ_CQ_OXE_NUM_FLITS) \
    X(CQ_CYCLES_BLOCKED_0) \
    X(CQ_CYCLES_BLOCKED_1) \
    X(CQ_CYCLES_BLOCKED_2) \
    X(CQ_CYCLES_BLOCKED_3) \
    X(HNI_RX_PAUSED_0) \
    X(HNI_RX_PAUSED_1) \
    X(HNI_RX_PAUSED_2) \
    X(HNI_RX_PAUSED_3) \
    X(HNI_RX_PAUSED_4) \
    X(HNI_RX_PAUSED_5) \
    X(HNI_RX_PAUSED_6) \
    X(HNI_RX_PAUSED_7) \
    X(HNI_TX_PAUSED_0) \
    X(HNI_TX_PAUSED_1) \
    X(HNI_TX_PAUSED_2) \
    X(HNI_TX_PAUSED_3) \
    X(HNI_TX_PAUSED_4) \
    X(HNI_TX_PAUSED_5) \
    X(HNI_TX_PAUSED_6) \
    X(HNI_TX_PAUSED_7) \
    X(HNI_PKTS_SENT_BY_TC_0) \
    X(HNI_PKTS_SENT_BY_TC_1) \
    X(HNI_PKTS_SENT_BY_TC_2) \
    X(HNI_PKTS_SENT_BY_TC_3) \
    X(HNI_PKTS_SENT_BY_TC_4) \
    X(HNI_PKTS_SENT_BY_TC_5) \
    X(HNI_PKTS_SENT_BY_TC_6) \
    X(HNI_PKTS_SENT_BY_TC_7) \
    X(HNI_PKTS_RECV_BY_TC_0) \
    X(HNI_PKTS_RECV_BY_TC_1) \
    X(HNI_PKTS_RECV_BY_TC_2) \
    X(HNI_PKTS_RECV_BY_TC_3) \
    X(HNI_PKTS_RECV_BY_TC_4) \
    X(HNI_PKTS_RECV_BY_TC_5) \
    X(HNI_PKTS_RECV_BY_TC_6) \
    X(HNI_PKTS_RECV_BY_TC_7) \
    X(HNI_TX_OK_27) \
    X(HNI_TX_OK_35) \
    X(HNI_TX_OK_36_TO_63) \
    X(HNI_TX_OK_64) \
    X(HNI_TX_OK_65_TO_127) \
    X(HNI_TX_OK_128_TO_255) \
    X(HNI_TX_OK_256_TO_511) \
    X(HNI_TX_OK_512_TO_1023) \
    X(HNI_TX_OK_1024_TO_2047) \
    X(HNI_TX_OK_2048_TO_4095) \
    X(HNI_TX_OK_4096_TO_8191) \
    X(HNI_TX_OK_8192_TO_MAX) \
    /* end of counters */\
    Z(APCXI_NUM_INDICES)


#define X(a) a,
#define Z(a) a
/* integer counters for the "APCXI" example module */
enum darshan_apcxi_perf_indices
{
    APCXI_PERF_COUNTERS
};

#undef Z
#undef X

/* the darshan_apcxi_router_record structure encompasses the data/counters
 * which would actually be logged to file by Darshan for the AP HPE Slingshot
 * module. This example implementation logs the following data for each
 * record:
 *      - a darshan_base_record structure, which contains the record id & rank
 *      - integer I/O counters 
 *      - floating point I/O counters 
 */
struct darshan_apcxi_perf_record
{
    struct darshan_base_record base_rec;
    int64_t group;
    int64_t chassis;
    int64_t slot;
    int64_t blade;
    int64_t node;
    uint64_t counters[APCXI_NUM_INDICES];
};

struct darshan_apcxi_header_record
{
    struct darshan_base_record base_rec;
    int64_t magic;
    int64_t nblades;
    int64_t nchassis;
    int64_t nslots;
    int64_t ngroups;
    uint64_t appid;
};

#endif /* __APCXI_LOG_FORMAT_H */
