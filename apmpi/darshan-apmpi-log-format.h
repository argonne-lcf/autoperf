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
    X(MPI_ISEND_COUNT) \
    X(MPI_IRECV_COUNT) \
    X(MPI_BARRIER_COUNT) \
    X(MPI_BCAST_COUNT) \
    X(MPI_REDUCE_COUNT) \
*/
/*
    X(MPI_ALLTOALL_COUNT) \
    X(MPI_ALLTOALLV_COUNT) \
    X(MPI_ALLGATHER_COUNT) \
    X(MPI_ALLGATHERV_COUNT) \
    X(MPI_GATHER_COUNT) \
    X(MPI_GATHERV_COUNT) \
    X(MPI_SCATTER_COUNT) \
    X(MPI_SCATTERV_COUNT) \
    X(MPI_REDUCE_SCATTER_COUNT) \
    X(MPI_SCAN_COUNT) \
    X(MPI_EXSCAN_COUNT) \
*/
#define APMPI_PERF_COUNTERS \
        Z(MPI_SEND) \
        Z(MPI_RECV) \
        Z(MPI_ALLREDUCE) \
        X(APMPI_NUM_INDICES)

#define APMPI_PERF_F_COUNTERS \
        Y(MPI_SEND) \
        Y(MPI_RECV) \
        Y(MPI_ALLREDUCE) \
        X(APMPI_F_NUM_INDICES)

#define Z(a) \
         X(a ## _CALL_COUNT), \
	 X(a ## _TOTAL_BYTES), \
         X(a ## _MSG_SIZE_AGG_0_256), \
         X(a ## _MSG_SIZE_AGG_256_1K), \
         X(a ## _MSG_SIZE_AGG_1K_32K), \
         X(a ## _MSG_SIZE_AGG_32K_256K), \
         X(a ## _MSG_SIZE_AGG_256K_1M), \
         X(a ## _MSG_SIZE_AGG_1M_4M), \
         X(a ## _MSG_SIZE_AGG_4M_PLUS), \

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
//undef Y
//undef Z
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
