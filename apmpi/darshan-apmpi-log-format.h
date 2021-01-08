/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef __DARSHAN_APMPI_LOG_FORMAT_H
#define __DARSHAN_APMPI_LOG_FORMAT_H

/* current AutoPerf MPI log format version */
#define DARSHAN_APMPI_VER 1

#define DARSHAN_APMPI_MAGIC ('A'*0x100000000+\
                            'P'*0x1000000+\
                            'M'*0x10000+\
                            'P'*0x100+\
                            'I'*0x1)

/*
    X(AMPI_ISEND_COUNT) \
    X(AMPI_IRECV_COUNT) \
    X(AMPI_BARRIER_COUNT) \
    X(AMPI_BCAST_COUNT) \
    X(AMPI_REDUCE_COUNT) \
*/
/*
    X(AMPI_ALLTOALL_COUNT) \
    X(AMPI_ALLTOALLV_COUNT) \
    X(AMPI_ALLGATHER_COUNT) \
    X(AMPI_ALLGATHERV_COUNT) \
    X(AMPI_GATHER_COUNT) \
    X(AMPI_GATHERV_COUNT) \
    X(AMPI_SCATTER_COUNT) \
    X(AMPI_SCATTERV_COUNT) \
    X(AMPI_REDUCE_SCATTER_COUNT) \
    X(AMPI_SCAN_COUNT) \
    X(AMPI_EXSCAN_COUNT) \
*/
#define APMPI_PERF_COUNTERS \
        Z(AMPI_SEND) \
        Z(AMPI_RECV) \
        Z(AMPI_ALLREDUCE) \
        X(APMPI_NUM_INDICES)

#define APMPI_PERF_F_COUNTERS \
        Y(AMPI_SEND) \
        Y(AMPI_RECV) \
        Y(AMPI_ALLREDUCE) \
        X(APMPI_F_NUM_INDICES)

#define Z(a) \
         X(a ## _CALL_COUNT), \
         X(a ## _MSG_SIZE),

#define Y(a) \
	X(a ## _TOTAL_TIME),

#define X(a) a
/* integer counters for the "MPI" example module */
enum darshan_apmpi_mpiop_indices
{
    APMPI_PERF_COUNTERS
};
/* float counters for the "MPI" example module */
enum darshan_apmpi_f_mpiop_indices
{
    APMPI_PERF_F_COUNTERS
};
#undef X
#undef Y
#undef Z
/*
#define X(a) a,
// integer counters for the "MPI" example module 
enum darshan_apmpi_perf_indices
{
    APMPI_PERF_COUNTERS
};

#undef X
*/
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


#endif /* __DARSHAN_APMPI_LOG_FORMAT_H */
