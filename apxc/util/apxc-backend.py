import cffi
import ctypes

import numpy as np
import darshan.backend.cffi_backend

# apxc structure defs
structdefs = '''
struct darshan_apxc_perf_record
{
    struct darshan_base_record base_rec;
    int group;
    int chassis; 
    int blade; 
    int node;
    int marked;
    uint64_t counters[392];
};
struct darshan_apxc_header_record
{
    struct darshan_base_record base_rec;
    int64_t magic;
    int nblades;
    int nchassis;
    int ngroups;
    int memory_mode;
    int cluster_mode;
    uint64_t appid;
};

extern char *apxc_counter_names[];

'''

def get_apxc_defs():
  return structdefs


# load header record
def log_get_apxc_record(log, dtype='dict'):
    from darshan.backend.cffi_backend import ffi, libdutil, log_get_modules, counter_names

    mod_name = 'APXC'
    modules = log_get_modules(log)


    rec = {}
    buf = ffi.new("void **")
    r = libdutil.darshan_log_get_record(log['handle'], modules[mod_name]['idx'], buf)
    if r < 1:
        return None

    hdr = ffi.cast('struct darshan_apxc_header_record **', buf)
    prf = ffi.cast('struct darshan_apxc_perf_record **', buf)
    memory_modes = ['unknown', 'flat', 'equal', 'split', 'cache']
    cluster_modes = ['unknown', 'all2all', 'quad', 'hemi', 'snc4', 'snc2']
    rec['id'] = hdr[0].base_rec.id
    rec['rank'] = hdr[0].base_rec.rank

    if hdr[0].magic == 4707761685111591494:
      mm = hdr[0].memory_mode & ~(1 << 31) 
      cm = hdr[0].cluster_mode & ~(1 << 31) 
      rec['nblades'] = hdr[0].nblades
      rec['nchassis'] = hdr[0].nchassis
      rec['ngroups'] = hdr[0].ngroups
      rec['memory_mode'] = memory_modes[mm]
      rec['cluster_mode'] = cluster_modes[cm]
      rec['appid'] = hdr[0].appid
    else:
      rec['group'] = prf[0].group
      rec['chassis'] = prf[0].chassis
      rec['blade'] = prf[0].blade
      rec['node'] = prf[0].node
      lst = []
      for i in range(0, len(prf[0].counters)):
        lst.append(prf[0].counters[i])
      np_counters = np.array(lst, dtype=np.uint64)
      d_counters = dict(zip(counter_names(mod_name), np_counters))

      if dtype == 'numpy':
        rec['counters'] = np_counters
      else:
        rec['counters'] = d_counters
    
    return rec

