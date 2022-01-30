/*
 * Copyright (C) 2015 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include "darshan-runtime-config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "darshan.h"
#include "darshan-dynamic.h"

/* data structure for recording gpu metrics */
typedef struct Tau_autoperf_gpu_metric_data {
  double total_kernel_exec_time;
  unsigned long int total_bytes_transferred_HtoD;
  unsigned long int total_bytes_transferred_DtoH;
  double total_memcpy_time;
} Tau_autoperf_gpu_metric_data;

#define APNVGPU_VERSION 0.1

DARSHAN_FORWARD_DECL(Tau_darshan_export_plugin, void, (Tau_autoperf_gpu_metric_data **data, double ver));

/* The apnvgpu_record_ref structure maintains necessary runtime metadata
 * for the APNVGPU module record (darshan_apnvgpu_record structure, defined in
 * darshan-apnvgpu-log-format.h) pointed to by 'record_p'. This metadata
 * assists with the instrumenting of specific statistics in the record.
 *
 * RATIONALE: the APNVGPU module needs to track some stateful, volatile 
 * information about each record it has registered (for instance, most
 * recent  access time, amount of bytes transferred) to aid in instrumentation, 
 * but this information can't be stored in the darshan_apnvgpu_record struct
 * because we don't want it to appear in the final darshan log file.  We 
 * therefore associate a apnvgpu_record_ref struct with each darshan_apnvgpu_record
 * struct in order to track this information (i.e., the mapping between
 * apnvgpu_record_ref structs to darshan_apnvgpu_record structs is one-to-one).
 *
 * NOTE: we use the 'darshan_record_ref' interface (in darshan-common) to
 * associate different types of handles with this apnvgpu_record_ref struct.
 * This allows us to index this struct (and the underlying record) by using
 * either the corresponding Darshan record identifier or by any other arbitrary
 * handle. For the APNVGPU module, the only handle we use to track record
 * references are Darshan record identifiers.
 */
struct apnvgpu_record_ref
{
    /* Darshan record for the "APNVGPU" example module */
    struct darshan_apnvgpu_record *record_p;

    /* ... other runtime data ... */
};

/* The apnvgpu_runtime structure maintains necessary state for storing
 * APNVGPU records and for coordinating with darshan-core at shutdown time.
 */
struct apnvgpu_runtime
{
    /* rec_id_hash is a pointer to a hash table of APNVGPU module record
     * references, indexed by Darshan record id
     */
    void *rec_id_hash;
    /* number of records currently tracked */
    int rec_count;
};

/* helper functions for the APNVGPU module */
void apnvgpu_runtime_initialize(
    void);
static struct apnvgpu_record_ref *apnvgpu_track_new_record(
    darshan_record_id rec_id, const char *name);

/* forward declaration for APNVGPU output/cleanup functions needed to interface
 * with darshan-core
 */
static void apnvgpu_output(void **apnvgpu_buf, int *apnvgpu_buf_sz);
static void apnvgpu_cleanup(void);

/* apnvgpu_runtime is the global data structure encapsulating "APNVGPU" module state */
static struct apnvgpu_runtime *apnvgpu_runtime = NULL;
/* The apnvgpu_runtime_mutex is a lock used when updating the apnvgpu_runtime global
 * structure (or any other global data structures). This is necessary to avoid race
 * conditions as multiple threads may execute function wrappers and update module state.
 * NOTE: Recursive mutexes are used in case functions wrapped by this module call
 * other wrapped functions that would result in deadlock, otherwise. This mechanism
 * may not be necessary for all instrumentation modules.
 */
static pthread_mutex_t apnvgpu_runtime_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
/* my_rank indicates the MPI rank of this process */
static int my_rank = -1;

/* macros for obtaining/releasing the "APNVGPU" module lock */
#define APNVGPU_LOCK() pthread_mutex_lock(&apnvgpu_runtime_mutex)
#define APNVGPU_UNLOCK() pthread_mutex_unlock(&apnvgpu_runtime_mutex)

/* the APNVGPU_PRE_RECORD macro is executed before performing APNVGPU
 * module instrumentation of a call. It obtains a lock for updating
 * module data strucutres, and ensure the APNVGPU module has been properly
 * initialized before instrumenting.
 */
#define APNVGPU_PRE_RECORD() do { \
    APNVGPU_LOCK(); \
    if(!darshan_core_disabled_instrumentation()) { \
        if(!apnvgpu_runtime) apnvgpu_runtime_initialize(); \
        if(apnvgpu_runtime) break; \
    } \
    APNVGPU_UNLOCK(); \
} while(0)

/* the APNVGPU_POST_RECORD macro is executed after performing APNVGPU
 * module instrumentation. It simply releases the module lock.
 */
#define APNVGPU_POST_RECORD() do { \
    APNVGPU_UNLOCK(); \
} while(0)

/* macro for gathering NVGPU metrics */
#define APNVGPU_RECORD_GPU_METRICS(__ret, data, __name) do{ \
    darshan_record_id rec_id; \
    struct apnvgpu_record_ref *rec_ref; \
    /* if bar returns error (return code < 0), don't instrument anything */ \
    if(__ret < 0) break; \
    /* use '__name' to generate a unique Darshan record id */ \
    rec_id = darshan_core_gen_record_id(__name); \
    /* look up a record reference for this record id using darshan rec_ref interface */ \
    rec_ref = darshan_lookup_record_ref(apnvgpu_runtime->rec_id_hash, &rec_id, sizeof(darshan_record_id)); \
    /* if no reference was found, track a new one for this record */ \
    if(!rec_ref) { rec_ref = apnvgpu_track_new_record(rec_id, __name); break; } \
    /* if we still don't have a valid reference, back out */ \
    if(!rec_ref) break; \
    /* record GPU metrics */ \
    rec_ref->record_p->counters[APNVGPU_CPU_GPU_TRANSFER_SIZE] = data->total_bytes_transferred_HtoD; \
    rec_ref->record_p->counters[APNVGPU_GPU_CPU_TRANSFER_SIZE] = data->total_bytes_transferred_DtoH; \
    rec_ref->record_p->fcounters[APNVGPU_F_CPU_GPU_BIDIRECTIONAL_TRANSFER_TIME] = data->total_memcpy_time; \
    rec_ref->record_p->fcounters[APNVGPU_F_KERNEL_EXECUTION_TIME] = data->total_kernel_exec_time; \
    fprintf(stderr, "APNVGPU: Done recording metrics.\n"); \
} while(0)

/**********************************************************
 *    Wrappers for "APNVGPU" module functions of interest    * 
 **********************************************************/


/**********************************************************
 * Internal functions for manipulating APNVGPU module state *
 **********************************************************/

/* Initialize internal APNVGPU module data structures and register with darshan-core. */
void apnvgpu_runtime_initialize()
{
    size_t apnvgpu_buf_size;
    darshan_module_funcs mod_funcs = {
#ifdef HAVE_MPI
    /* NOTE: the redux function can be used to run collective operations prior to
     * shutting down the module. Typically, module developers will want to run a
     * reduction on shared data records (passed in in the 'shared_recs' array),
     * but other collective routines can be run here as well. For a detailed
     * example illustrating how to run shared file reductions, consider the
     * POSIX or MPIIO instrumentation modules, as they both implement this
     * functionality.
     */
    .mod_redux_func = NULL,
#endif
    .mod_output_func = &apnvgpu_output,
    .mod_cleanup_func = &apnvgpu_cleanup
    };

    /* try and store a default number of records for this module */
    apnvgpu_buf_size = DARSHAN_DEF_MOD_REC_COUNT * sizeof(struct darshan_apnvgpu_record);

    /* register the APNVGPU module with the darshan-core component */
    darshan_core_register_module(
        DARSHAN_APNVGPU_MOD,   /* Darshan module identifier, defined in darshan-log-format.h */
        mod_funcs,
        &apnvgpu_buf_size,
        &my_rank,
        NULL);

    /* initialize module's global state */
    apnvgpu_runtime = malloc(sizeof(*apnvgpu_runtime));
    if(!apnvgpu_runtime)
    {
        darshan_core_unregister_module(DARSHAN_APNVGPU_MOD);
        return;
    }
    memset(apnvgpu_runtime, 0, sizeof(*apnvgpu_runtime));

    /* Creating a record here */
    ssize_t ret = 0;
    Tau_autoperf_gpu_metric_data *data = NULL;
    APNVGPU_PRE_RECORD();
    APNVGPU_RECORD_GPU_METRICS(ret, data, "gpuplugin");
    APNVGPU_POST_RECORD();

    return;
}

/* allocate and track a new APNVGPU module record */
static struct apnvgpu_record_ref *apnvgpu_track_new_record(
    darshan_record_id rec_id, const char *name)
{
    struct darshan_apnvgpu_record *record_p = NULL;
    struct apnvgpu_record_ref *rec_ref = NULL;
    int ret;

    rec_ref = malloc(sizeof(*rec_ref));
    if(!rec_ref)
        return(NULL);
    memset(rec_ref, 0, sizeof(*rec_ref));

    /* allocate a new APNVGPU record reference and add it to the hash
     * table, using the Darshan record identifier as the handle
     */
    ret = darshan_add_record_ref(&(apnvgpu_runtime->rec_id_hash), &rec_id,
        sizeof(darshan_record_id), rec_ref);
    if(ret == 0)
    {
        free(rec_ref);
        return(NULL);
    }

    /* register the actual file record with darshan-core so it is persisted
     * in the log file
     */
    record_p = darshan_core_register_record(
        rec_id,
        name,
        DARSHAN_APNVGPU_MOD,
        sizeof(struct darshan_apnvgpu_record),
        NULL);

    if(!record_p)
    {
        /* if registration fails, delete record reference and return */
        darshan_delete_record_ref(&(apnvgpu_runtime->rec_id_hash),
            &rec_id, sizeof(darshan_record_id));
        free(rec_ref);
        return(NULL);
    }

    /* registering this file record was successful, so initialize some fields */
    record_p->base_rec.id = rec_id;
    record_p->base_rec.rank = my_rank;
    rec_ref->record_p = record_p;
    apnvgpu_runtime->rec_count++;

    /* return pointer to the record reference */
    return(rec_ref);
}

/**************************************************************************************
 *    functions exported by the "APNVGPU" module for coordinating with darshan-core      *
 **************************************************************************************/

/* Pass output data for the "APNVGPU" module back to darshan-core to log to file
 */
static void apnvgpu_output(
    void **apnvgpu_buf,
    int *apnvgpu_buf_sz)
{

    ssize_t ret = 0;
    fprintf(stderr, "APNVGPU: Inside finalize.\n");

    Tau_autoperf_gpu_metric_data *data = (Tau_autoperf_gpu_metric_data *)calloc(1, sizeof(Tau_autoperf_gpu_metric_data));
    CUSTOM_MAP_OR_FAIL(Tau_darshan_export_plugin);

    __real_Tau_darshan_export_plugin(&data, APNVGPU_VERSION);

    APNVGPU_PRE_RECORD();
    APNVGPU_RECORD_GPU_METRICS(ret, data, "gpuplugin");
    APNVGPU_POST_RECORD();

    APNVGPU_LOCK();
    assert(apnvgpu_runtime);

    /* Just set the output size according to the number of records currently
     * being tracked. In general, the module can decide to throw out records
     * that have been previously registered by shuffling around memory in
     * 'apnvgpu_buf' -- 'apnvgpu_buf' and 'apnvgpu_buf_sz' both are passed as pointers
     * so they can be updated by the shutdown function potentially. 
     */
    *apnvgpu_buf_sz = apnvgpu_runtime->rec_count * sizeof(struct darshan_apnvgpu_record);
    
    APNVGPU_UNLOCK();
    return;
}

/* Cleanup/free internal data structures
 */
static void apnvgpu_cleanup()
{
    /* cleanup internal structures used for instrumenting */
    APNVGPU_LOCK();

    /* iterate the hash of record references and free them */
    darshan_clear_record_refs(&(apnvgpu_runtime->rec_id_hash), 1);

    free(apnvgpu_runtime);
    apnvgpu_runtime = NULL;

    APNVGPU_UNLOCK();
    return;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
