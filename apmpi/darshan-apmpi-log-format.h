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
        X(MPI_SSEND) \
        X(MPI_RSEND) \
        X(MPI_BSEND) \
        /*X(MPI_SEND_INIT) \
        X(MPI_SSEND_INIT) \
        X(MPI_RSEND_INIT) \
        X(MPI_BSEND_INIT) \ */ \
        X(MPI_RECV)  /*
        X(MPI_RECV_INIT) */\
        X(MPI_SENDRECV) \
        X(MPI_SENDRECV_REPLACE) 

#define APMPI_MPI_NONBLOCKING_P2P \
        X(MPI_ISEND) \
        X(MPI_ISSEND) \
        X(MPI_IRSEND) \
        X(MPI_IBSEND) \
        X(MPI_IRECV) 
     /*   C(MPI_NONBLOCKING_P2P_CNT) */

#define AMPI_MPI_P2P_MISC \
        V(MPI_PROBE) \
        V(MPI_IPROBE) \
	V(MPI_TEST) \
	V(MPI_TESTANY) \
	V(MPI_TESTALL) \
	V(MPI_TESTSOME) \
	V(MPI_WAIT) \
	V(MPI_WAITANY) \
	V(MPI_WAITALL) \
	V(MPI_WAITSOME) /*
	V(MPI_START) \
	V(MPI_STARTALL)		*/

#define APMPI_MPI_COLL_SYNC \
        V(MPI_BARRIER) 

#define APMPI_MPI_BLOCKING_COLL \
        X(MPI_BCAST) \
        X(MPI_GATHER)   \
        X(MPI_GATHERV)  \
        X(MPI_SCATTER)  \
        X(MPI_SCATTERV) \
        X(MPI_SCAN)     \
        X(MPI_EXSCAN)     \
        X(MPI_ALLGATHER)        \
        X(MPI_ALLGATHERV)       \
        X(MPI_REDUCE)   \
        X(MPI_ALLREDUCE)        \
        X(MPI_REDUCE_SCATTER)   \
        X(MPI_ALLTOALL)         \
        X(MPI_ALLTOALLV)        \
        X(MPI_ALLTOALLW)   /*    \
        Y(MPI_BLOCKING_COLL_CNT) */
/*
#define APMPI_MPI_ONESIDED \
	X(MPI_PUT) \
	X(MPI_GET) 
*/
#define I(a) \
         Y(a ## _CALL_COUNT) \
         Y(a ## _TOTAL_BYTES) \
         Y(a ## _MSG_SIZE_AGG_0_256) \
         Y(a ## _MSG_SIZE_AGG_256_1K) \
         Y(a ## _MSG_SIZE_AGG_1K_8K) \
         Y(a ## _MSG_SIZE_AGG_8K_256K) \
         Y(a ## _MSG_SIZE_AGG_256K_1M) \
         Y(a ## _MSG_SIZE_AGG_1M_PLUS) \

#define J(a) \
         Y(a ## _CALL_COUNT) \

#define APMPI_MPIOP_COUNTERS \
        APMPI_MPI_BLOCKING_P2P \
        APMPI_MPI_NONBLOCKING_P2P \
	AMPI_MPI_P2P_MISC \
	APMPI_MPI_COLL_SYNC \
        APMPI_MPI_BLOCKING_COLL \
        Z(APMPI_NUM_INDICES)

#define Y(a) a,
#define Z(a) a
#define X I
#define V J
/* integer counters for the "APMPI" module */
enum apmpi_mpiop_indices
{
    APMPI_MPIOP_COUNTERS
};
#undef X
#undef V

	/* per MPI op total times across the calls */
#define F_P2P(a) \
        Y(a ## _TOTAL_TIME) \
        Y(a ## _MIN_TIME) \
        Y(a ## _MAX_TIME) 

#define APMPI_F_MPIOP_TOTALTIME_COUNTERS \
        APMPI_MPI_BLOCKING_P2P  \
        APMPI_MPI_NONBLOCKING_P2P  \
	AMPI_MPI_P2P_MISC \
	APMPI_MPI_COLL_SYNC \
        APMPI_MPI_BLOCKING_COLL \
        Z(APMPI_F_NUM_INDICES) 

/* float counters for the "APMPI" module */
#define X F_P2P
#define V F_P2P
enum apmpi_f_mpiop_totaltime_indices
{
    APMPI_F_MPIOP_TOTALTIME_COUNTERS
};
#undef X
#undef V

#define F_SYNC(a) \
        Y(a ## _TOTAL_SYNC_TIME) 
#define APMPI_F_MPIOP_SYNCTIME_COUNTERS \
	APMPI_MPI_COLL_SYNC \
	APMPI_MPI_BLOCKING_COLL \
        Z(APMPI_F_SYNC_NUM_INDICES) 
/* float counters for the "APMPI" module */
#define X F_SYNC
#define V F_SYNC
enum apmpi_f_mpiop_totalsync_indices
{
    APMPI_F_MPIOP_SYNCTIME_COUNTERS
};
#undef X
#undef V

        /* aggregate (across all the ranks) per MPI op times  */ 
#define APMPI_F_MPI_GLOBAL_COUNTERS \
	Y(RANK_TOTAL_MPITIME) \
	Z(APMPI_F_GLOBAL_NUM_INDICES)
enum apmpi_f_mpi_global_indices
{
    APMPI_F_MPI_GLOBAL_COUNTERS
};
#undef Z
#undef Y
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
    double fsynccounters[APMPI_F_SYNC_NUM_INDICES];
    double fglobalcounters[APMPI_F_GLOBAL_NUM_INDICES];
};
struct darshan_apmpi_header_record
{
    struct darshan_base_record base_rec;
    int64_t magic;
    double apmpi_f_variance_total_mpitime;
    uint64_t appid;
};

#endif /* __APMPI_LOG_FORMAT_H */
