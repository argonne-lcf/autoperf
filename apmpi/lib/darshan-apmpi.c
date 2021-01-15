/*
 * Copyright (C) 2017 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#define csJOBID_ENV_STR "ALPS_APP_ID"

#include "darshan-runtime-config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <papi.h>

#include "uthash.h"
#include "darshan.h"
#include "darshan-dynamic.h"
#include "darshan-apmpi-log-format.h"

#include "darshan-apmpi-utils.h"

typedef long long ap_bytes_t;

#define BYTECOUNT(TYPE, COUNT) \
          int tsize; \
          PMPI_Type_size(TYPE, &tsize); \
          ap_bytes_t bytes = (COUNT) * tsize

#define BYTECOUNTND(TYPE, COUNT) \
          int tsize2; \
          PMPI_Type_size(TYPE, &tsize2); \
          bytes = (COUNT) * tsize2

/* increment histogram bucket depending on the given __value
 *
 * NOTE: This macro can be used to build a histogram of access
 * sizes, offsets, etc. It assumes a 8-bucket histogram, with
 * __bucket_base_p pointing to the first counter in the sequence
 * of buckets (i.e., the smallest bucket). The size ranges of each
 * bucket are:
 *      * 0 - 256 bytes
 *      * 256 - 1 KiB
 *      * 1 KiB - 32 KiB
 *      * 32 KiB - 256 KiB
 *      * 256 KiB - 1 MiB
 *      * 1 MiB - 4 MiB
 *      * 4 MiB +
 */
#define DARSHAN_MSG_BUCKET_INC(__bucket_base_p, __value) do {\
    if(__value < 257) \
        *(__bucket_base_p) += 1; \
    else if(__value < 1025) \
        *(__bucket_base_p + 1) += 1; \
    else if(__value < 8193) \
        *(__bucket_base_p + 2) += 1; \
    else if(__value < 262145) \
        *(__bucket_base_p + 3) += 1; \
    else if(__value < 1048577) \
        *(__bucket_base_p + 4) += 1; \
    else \
        *(__bucket_base_p + 5) += 1; \
} while(0)


DARSHAN_FORWARD_DECL(PMPI_Send, int, (const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Recv, int, (void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status));
#if 0
DARSHAN_FORWARD_DECL(PMPI_Isend, int, (const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request));
DARSHAN_FORWARD_DECL(PMPI_Irecv, int, (void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request));
DARSHAN_FORWARD_DECL(PMPI_Barrier, int, (MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Bcast, int, (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Reduce, int,  (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm));
#endif
DARSHAN_FORWARD_DECL(PMPI_Allreduce, int,  (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm));
#if 0
DARSHAN_FORWARD_DECL(PMPI_Alltoall, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Alltoallv, int, (const void *sendbuf, const int *sendcounts, const int *sdispls,
                 MPI_Datatype sendtype, void *recvbuf, const int *recvcounts, const int *rdispls,
                 MPI_Datatype recvtype, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Allgather, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Allgatherv, int, ());
DARSHAN_FORWARD_DECL(PMPI_Gather , int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Gatherv , int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int *recvcounts, const int *displs,
                MPI_Datatype recvtype, int root, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Scatter, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
               MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Scatterv, int, (const void *sendbuf, const int *sendcounts, const int *displs,
                 MPI_Datatype sendtype, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype,
                 int root, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Reduce_scatter, int, (const void *sendbuf, void *recvbuf, const int recvcounts[],
                       MPI_Datatype datatype, MPI_Op op, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Scan, int, (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
             MPI_Op op, MPI_Comm comm));
DARSHAN_FORWARD_DECL(PMPI_Exscan, int, (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, MPI_Comm comm));
#endif
//DARSHAN_FORWARD_DECL(PMPI_ , int, ());
/*
 * <Description>
 * 
 * This module does intercept system calls. 
 */


/*
 * Global runtime struct for tracking data needed at runtime
 */
struct apmpi_runtime
{
    struct darshan_apmpi_perf_record *perf_record;
    darshan_record_id rec_id;
};

static struct apmpi_runtime *apmpi_runtime = NULL;
static pthread_mutex_t apmpi_runtime_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* my_rank indicates the MPI rank of this process */
static int my_rank = -1;

/* internal helper functions for the apmpi module */
void apmpi_runtime_initialize(void);

/* forward declaration for shutdown function needed to interface with darshan-core */
#ifdef HAVE_MPI
static void apmpi_mpi_redux(
    void *buffer, 
    MPI_Comm mod_comm,
    darshan_record_id *shared_recs, 
    int shared_rec_count);
#endif
static void apmpi_shutdown(
        void **buffer, 
        int *size);

/* macros for obtaining/releasing the apmpi module lock */
#define APMPI_LOCK() pthread_mutex_lock(&apmpi_runtime_mutex)
#define APMPI_UNLOCK() pthread_mutex_unlock(&apmpi_runtime_mutex)

/*
 * Initialize counters 
 */
static void initialize_counters (void)
{
    int i;
    int code = 0;
    for (i = 0; i < APMPI_NUM_INDICES; i++)
    {
        apmpi_runtime->perf_record->counters[i] = 0; 
    }
    for (i = 0; i < APMPI_F_NUM_INDICES; i++)
    {
        apmpi_runtime->perf_record->fcounters[i] = 0; 
    }
    return;
}

static void finalize_counters (void)
{

    return;
}

/*
 * Function which updates all the counter data
 */
static void capture(struct darshan_apmpi_perf_record *rec,
                    darshan_record_id rec_id)
{
    rec->base_rec.id = rec_id;
    rec->base_rec.rank = my_rank;

    return;
}

void apmpi_runtime_initialize()
{
    int apmpi_buf_size;
    char rec_name[128];

    darshan_module_funcs mod_funcs = {
#ifdef HAVE_MPI
        .mod_redux_func = &apmpi_mpi_redux,
#endif
        .mod_shutdown_func = &apmpi_shutdown
        };

    APMPI_LOCK();

    /* don't do anything if already initialized */
    if(apmpi_runtime)
    {
        APMPI_UNLOCK();
        return;
    }

    apmpi_buf_size = sizeof(struct darshan_apmpi_perf_record);

    /* register the apmpi module with the darshan-core component */
    darshan_core_register_module(
        APMPI_MOD,
        mod_funcs,
        &apmpi_buf_size,
        &my_rank,
        NULL);

    /* not enough memory to fit apmpi module record */
    if(apmpi_buf_size < sizeof(struct darshan_apmpi_perf_record))
    {
        darshan_core_unregister_module(APMPI_MOD);
        APMPI_UNLOCK();
        return;
    }

    /* initialize module's global state */
    apmpi_runtime = malloc(sizeof(*apmpi_runtime));
    if(!apmpi_runtime)
    {
        darshan_core_unregister_module(APMPI_MOD);
        APMPI_UNLOCK();
        return;
    }
    memset(apmpi_runtime, 0, sizeof(*apmpi_runtime));
    //sprintf(rec_name, "darshan-apmpi-%d", my_rank);
    //apmpi_runtime->rec_id = darshan_core_gen_record_id(rec_name);
    apmpi_runtime->rec_id = darshan_core_gen_record_id("APMPI"); //record name

    apmpi_runtime->perf_record = darshan_core_register_record(
        apmpi_runtime->rec_id,
        "APMPI",
        APMPI_MOD,
        sizeof(struct darshan_apmpi_perf_record),
        NULL);
    if(!(apmpi_runtime->perf_record))
    {
        darshan_core_unregister_module(APMPI_MOD);
        free(apmpi_runtime);
        apmpi_runtime = NULL;
        APMPI_UNLOCK();
        return;
    }

    initialize_counters();

    APMPI_UNLOCK();

    return;
}
#if 0
static void apmpi_shared_record_variance(MPI_Comm mod_comm,
    struct darshan_apmpi_perf_record *inrec_array, struct darshan_apmpi_perf_record *outrec_array,
    int shared_rec_count)
{
    MPI_Datatype var_dt;
    MPI_Op var_op;
    int i;
    struct darshan_variance_dt *var_send_buf = NULL;
    struct darshan_variance_dt *var_recv_buf = NULL;

    PMPI_Type_contiguous(sizeof(struct darshan_variance_dt),
        MPI_BYTE, &var_dt);
    PMPI_Type_commit(&var_dt);

    PMPI_Op_create(darshan_variance_reduce, 1, &var_op);

    var_send_buf = malloc(shared_rec_count * sizeof(struct darshan_variance_dt));
    if(!var_send_buf)
        return;

    if(my_rank == 0)
    {   
        var_recv_buf = malloc(shared_rec_count * sizeof(struct darshan_variance_dt));

        if(!var_recv_buf)
            return;
    }   

    /* get total i/o time variances for shared records */

    for(i=0; i<shared_rec_count; i++)
    {   
        var_send_buf[i].n = 1;
        var_send_buf[i].S = 0;
        var_send_buf[i].T = inrec_array[i].fcounters[MPIIO_F_READ_TIME] +
                            inrec_array[i].fcounters[MPIIO_F_WRITE_TIME] +
                            inrec_array[i].fcounters[MPIIO_F_META_TIME];
    }   

    PMPI_Reduce(var_send_buf, var_recv_buf, shared_rec_count,
        var_dt, var_op, 0, mod_comm);

    if(my_rank == 0)
    {   
        for(i=0; i<shared_rec_count; i++)
        {   
            outrec_array[i].fcounters[MPIIO_F_VARIANCE_RANK_TIME] =
                (var_recv_buf[i].S / var_recv_buf[i].n);
        }   
    }   

    /* get total bytes moved variances for shared records */

    for(i=0; i<shared_rec_count; i++)
    {   
        var_send_buf[i].n = 1;
        var_send_buf[i].S = 0;
        var_send_buf[i].T = (double)
                            inrec_array[i].counters[MPIIO_BYTES_READ] +
                            inrec_array[i].counters[MPIIO_BYTES_WRITTEN];
    }

    PMPI_Reduce(var_send_buf, var_recv_buf, shared_rec_count,
        var_dt, var_op, 0, mod_comm);

    if(my_rank == 0)
    {
        for(i=0; i<shared_rec_count; i++)
        {
            outrec_array[i].fcounters[MPIIO_F_VARIANCE_RANK_BYTES] =
                (var_recv_buf[i].S / var_recv_buf[i].n);
        }
    }
    
    PMPI_Type_free(&var_dt);
    PMPI_Op_free(&var_op);
    free(var_send_buf);
    free(var_recv_buf);

    return;
}
#endif


/********************************************************************************
 * shutdown function exported by this module for coordinating with darshan-core *
 ********************************************************************************/

/* Pass data for the apmpi module back to darshan-core to log to file. */
//#ifdef HAVE_MPI
static void apmpi_mpi_redux(
    void *apmpi_buf,
    MPI_Comm mod_comm,
    darshan_record_id *shared_recs,
    int shared_rec_count)
{
    int i;
    struct darshan_apmpi_perf_record *red_send_buf = NULL;
    struct darshan_apmpi_perf_record *red_recv_buf = NULL;
    MPI_Datatype red_type;
    MPI_Op red_op;

    APMPI_LOCK();

    if (!apmpi_runtime)
    {
        APMPI_UNLOCK();
        return;
    }
    /* collect perf counters */
    capture(apmpi_runtime->perf_record, apmpi_runtime->rec_id);
    
    APMPI_UNLOCK();

    return;
}

//#endif
static void apmpi_shutdown(
    void **apmpi_buf,
    int *apmpi_buf_sz)
{
    int apmpi_rec_count;

    APMPI_LOCK();
    assert(apmpi_runtime);
    *apmpi_buf_sz = sizeof( *apmpi_runtime->perf_record);
    finalize_counters();
    free(apmpi_runtime);
    apmpi_runtime = NULL;

    APMPI_UNLOCK();
    return;
}

#define APMPI_PRE_RECORD() do { \
       APMPI_LOCK(); \
       if(!darshan_core_disabled_instrumentation()) { \
           if(!apmpi_runtime) { \
               apmpi_runtime_initialize(); \
           } \
           if(apmpi_runtime) break; \
       } \
       APMPI_UNLOCK(); \
       return(ret); \
   } while(0)

#define APMPI_POST_RECORD() do { \
       APMPI_UNLOCK(); \
   } while(0)

/**********************************************************
 *        Wrappers for MPI functions of interest       * 
 **********************************************************/

int DARSHAN_DECL(MPI_Send)(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm)
{
    int ret;
    double tm1, tm2;
    int size;

    MAP_OR_FAIL(PMPI_Send);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Send(buf, count, datatype, dest, tag, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    // Lock around the count - lock only if MPI_THREAD_MULTIPLE is used ... locking mutex
    apmpi_runtime->perf_record->counters[MPI_SEND_CALL_COUNT]++;
    BYTECOUNT(datatype, count);
    apmpi_runtime->perf_record->counters[MPI_SEND_TOTAL_BYTES] += bytes;
    DARSHAN_MSG_BUCKET_INC(&(apmpi_runtime->perf_record->counters[MPI_SEND_MSG_SIZE_AGG_0_256]), bytes);
    apmpi_runtime->perf_record->fcounters[MPI_SEND_TOTAL_TIME]+=(tm2-tm1);
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Send, int,  (const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm), MPI_Send)

int DARSHAN_DECL(MPI_Recv)(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Recv);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Recv(buf, count, datatype, source, tag, comm, status);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_RECV_CALL_COUNT]++;
    BYTECOUNT(datatype, count);
    apmpi_runtime->perf_record->counters[MPI_RECV_TOTAL_BYTES] += bytes;
    DARSHAN_MSG_BUCKET_INC(&(apmpi_runtime->perf_record->counters[MPI_RECV_MSG_SIZE_AGG_0_256]), bytes);
    apmpi_runtime->perf_record->fcounters[MPI_RECV_TOTAL_TIME]+=(tm2-tm1);
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Recv, int,  (void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status), MPI_Recv)
#if 0
int DARSHAN_DECL(MPI_Isend)(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm, MPI_Request *request)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Isend);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_ISEND_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Isend, int,  (const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request), MPI_Isend)

int DARSHAN_DECL(MPI_Irecv)(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Request * request)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Irecv);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_IRECV_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Irecv, int,  (void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request), MPI_Irecv)


int DARSHAN_DECL(MPI_Barrier)(MPI_Comm comm)
{
    int ret;
    double tm1, tm2;
    
    MAP_OR_FAIL(PMPI_Barrier);
  
    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Barrier(comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_BARRIER_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Barrier, int,  (MPI_Comm comm), MPI_Barrier)

int DARSHAN_DECL(MPI_Bcast)(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;
    
    MAP_OR_FAIL(PMPI_Bcast);
  
    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Bcast(buffer, count, datatype, root, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_BCAST_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Bcast, int, (void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm), MPI_Bcast)

int DARSHAN_DECL(MPI_Reduce)(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root,
             MPI_Comm comm)
{
    int ret;
    double tm1, tm2;
    
    MAP_OR_FAIL(PMPI_Reduce);
  
    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_REDUCE_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Reduce, int,  (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm), MPI_Reduce)
#endif
int DARSHAN_DECL(MPI_Allreduce)(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op,
             MPI_Comm comm)
{
    int ret;
    double tm1, tm2;
    
    MAP_OR_FAIL(PMPI_Allreduce);
  
    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_ALLREDUCE_CALL_COUNT]++;
    BYTECOUNT(datatype, count);
    apmpi_runtime->perf_record->counters[MPI_ALLREDUCE_TOTAL_BYTES] += bytes;
    DARSHAN_MSG_BUCKET_INC(&(apmpi_runtime->perf_record->counters[MPI_ALLREDUCE_MSG_SIZE_AGG_0_256]), bytes);
    apmpi_runtime->perf_record->fcounters[MPI_ALLREDUCE_TOTAL_TIME]+=(tm2-tm1);
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Allreduce, int,  (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm), MPI_Allreduce)

#if 0
int DARSHAN_DECL(MPI_Alltoall)(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 MPI_Comm comm)
{
    int ret;
    double tm1, tm2;
    
    MAP_OR_FAIL(PMPI_Alltoall);
  
    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_ALLTOALL_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Alltoall, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 MPI_Comm comm), MPI_Alltoall)

int DARSHAN_DECL(MPI_Alltoallv)(const void *sendbuf, const int *sendcounts, const int *sdispls,
                MPI_Datatype sendtype, void *recvbuf, const int *recvcounts, const int *rdispls,
                MPI_Datatype recvtype, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;
    
    MAP_OR_FAIL(PMPI_Alltoallv);
  
    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Alltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts, rdispls, recvtype, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_ALLTOALLV_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Alltoallv, int, (const void *sendbuf, const int *sendcounts, const int *sdispls,
                 MPI_Datatype sendtype, void *recvbuf, const int *recvcounts, const int *rdispls,
                 MPI_Datatype recvtype, MPI_Comm comm), MPI_Alltoallv)

int DARSHAN_DECL(MPI_Allgatherv)(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, const int *recvcounts, const int *displs,
                   MPI_Datatype recvtype, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Allgatherv);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_ALLGATHERV_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}

int DARSHAN_DECL(MPI_Allgather)(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Allgather);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_ALLGATHER_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Allgather, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm), MPI_Allgather)

DARSHAN_WRAPPER_MAP(PMPI_Allgatherv, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, const int *recvcounts, const int *displs,
                   MPI_Datatype recvtype, MPI_Comm comm), MPI_Allgatherv)

int DARSHAN_DECL(MPI_Gather)(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Gather);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_GATHER_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Gather , int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm), MPI_Gather )

int DARSHAN_DECL(MPI_Gatherv)(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int *recvcounts, const int *displs,
                MPI_Datatype recvtype, int root, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Gatherv);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Gatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, root, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_GATHERV_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Gatherv, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int *recvcounts, const int *displs,
                MPI_Datatype recvtype, int root, MPI_Comm comm), MPI_Gatherv)

int DARSHAN_DECL(MPI_Scatter)(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
               MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Scatter);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_SCATTER_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Scatter, int, (const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
               MPI_Comm comm), MPI_Scatter)

int DARSHAN_DECL(MPI_Scatterv)(const void *sendbuf, const int *sendcounts, const int *displs,
                 MPI_Datatype sendtype, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype,
                 int root, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Scatterv);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Scatterv(sendbuf, sendcounts, displs, sendtype, recvbuf, recvcount, recvtype, root, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_SCATTERV_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Scatterv, int, (const void *sendbuf, const int *sendcounts, const int *displs,
                 MPI_Datatype sendtype, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype,
                 int root, MPI_Comm comm), MPI_Scatterv)

int DARSHAN_DECL(MPI_Reduce_scatter)(const void *sendbuf, void *recvbuf, const int recvcounts[],
                       MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Reduce_scatter);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Reduce_scatter(sendbuf, recvbuf, recvcounts,
                       datatype, op, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_REDUCE_SCATTER_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Reduce_scatter, int, (const void *sendbuf, void *recvbuf, const int recvcounts[],
                       MPI_Datatype datatype, MPI_Op op, MPI_Comm comm), MPI_Reduce_scatter)

int DARSHAN_DECL(MPI_Scan)(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
             MPI_Op op, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Scan);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Scan(sendbuf, recvbuf, count, datatype, op, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_SCAN_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Scan, int, (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
             MPI_Op op, MPI_Comm comm), MPI_Scan )
int DARSHAN_DECL(MPI_Exscan)(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, MPI_Comm comm)
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_Exscan);

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_Exscan(sendbuf, recvbuf, count, datatype, op, comm);
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_EXSCAN_COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_Exscan, int, (const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, MPI_Comm comm), MPI_Exscan)
#endif
/*
int DARSHAN_DECL(MPI_ )()
{
    int ret;
    double tm1, tm2;

    MAP_OR_FAIL(PMPI_ );

    tm1 = darshan_core_wtime();
    ret = __real_PMPI_();
    tm2 = darshan_core_wtime();
    APMPI_PRE_RECORD();
    apmpi_runtime->perf_record->counters[MPI_ _COUNT]++;
    APMPI_POST_RECORD();
    return ret;
}
DARSHAN_WRAPPER_MAP(PMPI_ , int, (), MPI_ )
*/


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
