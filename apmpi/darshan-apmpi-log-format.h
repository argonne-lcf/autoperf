/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef __APMPI_LOG_FORMAT_H
#define __APMPI_LOG_FORMAT_H

/* current AutoPerf MPI log format version */
#define APMPI_VER 1

#define APMPI_MAGIC ('A'*0x100000000+\
                            'P'*0x1000000+\
                            'M'*0x10000+\
                            'P'*0x100+\
                            'I'*0x1)
#define APMPI_MPI_BLOCKING_P2P \
        X(MPI_SEND) \
        /*X(MPI_SSEND) \
        X(MPI_RSEND) \
        X(MPI_BSEND) \
        X(MPI_SEND_INIT) \
        X(MPI_SSEND_INIT) \
        X(MPI_RSEND_INIT) \
        X(MPI_BSEND_INIT) \ */ \
        X(MPI_RECV)  /*
        X(MPI_RECV_INIT) \
        X(MPI_SENDRECV) \
        X(MPI_SENDRECV_REPLACE) \
        Y(MPI_BLOCKING_P2P_CNT) */

#define APMPI_MPI_NONBLOCKING_P2P \
        X(MPI_ISEND) \
        X(MPI_ISSEND) \
        X(MPI_IRSEND) \
        X(MPI_IBSEND) \
        X(MPI_IRECV) /*\
        Y(MPI_NONBLOCKING_P2P_CNT) */

/*
#define AMPI_MPI_P2P_MISC \
        X(MPI_PROBE) \
*/
#define APMPI_MPI_BLOCKING_COLL \
      /*  X(MPI_BCAST) \
        X(MPI_BARRIER) \
        X(MPI_GATHER)   \
        X(MPI_GATHERV)  \
        X(MPI_SCATTER)  \
        X(MPI_SCATTERV) \
        X(MPI_SCAN)     \
        X(MPI_ALLGATHER)        \
        X(MPI_ALLGATHERV)       \
        X(MPI_REDUCE)   \
       */ X(MPI_ALLREDUCE)  /*      \
        X(MPI_REDUCE_SCATTER)   \
        X(MPI_ALLTOALL)         \
        X(MPI_ALLTOALLV)        \
        X(MPI_ALLTOALLW)        \
        X(MPI_EXSCAN)    \
        Y(MPI_BLOCKING_COLL_CNT) */

#define I(a) \
         Y(a ## _CALL_COUNT) \
         Y(a ## _TOTAL_BYTES) \
         Y(a ## _MSG_SIZE_AGG_0_256) \
         Y(a ## _MSG_SIZE_AGG_256_1K) \
         Y(a ## _MSG_SIZE_AGG_1K_8K) \
         Y(a ## _MSG_SIZE_AGG_8K_256K) \
         Y(a ## _MSG_SIZE_AGG_256K_1M) \
         Y(a ## _MSG_SIZE_AGG_1M_PLUS) \

#define APMPI_PERF_COUNTERS \
        APMPI_MPI_BLOCKING_P2P \
        /*APMPI_MPI_NONBLOCKING_P2P */\
        APMPI_MPI_BLOCKING_COLL \
        Z(APMPI_NUM_INDICES)

#define Y(a) a,
#define Z(a) a
#define X I
/* integer counters for the "MPI" example module */
enum darshan_apmpi_mpiop_indices
{
    APMPI_PERF_COUNTERS
};
#undef X

        /* per MPI op max and mean times  */ 
        /* aggregate (across all the ranks) per MPI op times  */ 
#define F(a) \
        Y(a ## _TOTAL_TIME) \
        Y(a ## _MAX_TIME) \
        Y(a ## _AVG_TIME) \
	Y(a ## _AGG_MAX_TIME) \
	Y(a ## _AGG_AVG_TIME) \
        Y(a ## _AGG_F_VARIANCE_TIME) 

#define APMPI_F_GLOBAL_COUNTERS \
        Y(APMPI_F_TOTAL_MPITIME)\
        Y(APMPI_F_VARIANCE_TOTAL_MPITIME) 

#define APMPI_PERF_F_COUNTERS \
        APMPI_MPI_BLOCKING_P2P  /*
        APMPI_MPI_NONBLOCKING_P2P */ \
        APMPI_MPI_BLOCKING_COLL /*
        APMPI_F_GLOBAL_COUNTERS */ \
        Z(APMPI_F_NUM_INDICES) 
        /* variance of total MPI time across all ranks */\
        /* NOTE: for shared records only */\
        /*X(APMPI_F_VARIANCE_RANK_TOTAL_MPITIME) \ */
        /*X(APMPI_F_VARIANCE_RANK_BYTES) */ \

/* float counters for the "MPI" example module */
#define X F
enum darshan_apmpi_f_mpiop_indices
{
    APMPI_PERF_F_COUNTERS
};

#undef Z
#undef Y
#undef X

/* the darshan_apmpi_record structure encompasses the data/counters
 * which would actually be logged to file by Darshan for the AP MPI
 * module. This example implementation logs the following data for each
 * record:
 *      - a darshan_base_record structure, which contains the record id & rank
 *      - integer I/O counters 
 *      - floating point I/O counters 
 */
struct darshan_apmpi_perf_record
{
    struct darshan_base_record base_rec;
    uint64_t counters[APMPI_NUM_INDICES];
    double fcounters[APMPI_F_NUM_INDICES];
};


#endif /* __APMPI_LOG_FORMAT_H */
