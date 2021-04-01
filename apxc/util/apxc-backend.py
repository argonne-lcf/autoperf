import cffi
import ctypes

import numpy as np
import darshan.backend.cffi_backend

# APXC structure defs
structdefs = '''
struct darshan_apxc_perf_record
{
    struct darshan_base_record base_rec;
    int64_t group;
    int64_t chassis; 
    int64_t blade; 
    int64_t node;
    uint64_t counters[392];
};
struct darshan_apxc_header_record
{
    struct darshan_base_record base_rec;
    int64_t magic;
    int64_t nblades;
    int64_t nchassis;
    int64_t ngroups;
    int64_t memory_mode;
    int64_t cluster_mode;
    uint64_t appid;
};

extern char *apxc_counter_names[];

'''

def get_apxc_defs():
  return structdefs


# load header record
def log_get_apxc_record(log, mod_name, structname, dtype='dict'):
    from darshan.backend.cffi_backend import ffi, libdutil, log_get_modules, counter_names, _structdefs

    modules = log_get_modules(log)

    rec = {}
    buf = ffi.new("void **")
    r = libdutil.darshan_log_get_record(log['handle'], modules[mod_name]['idx'], buf)
    mod_type = _structdefs[mod_name+"-"+structname]

    if r < 1:
        return None

    memory_modes = ['unknown', 'flat', 'equal', 'split', 'cache']
    cluster_modes = ['unknown', 'all2all', 'quad', 'hemi', 'snc4', 'snc2']
    
    if mod_type == 'struct darshan_apxc_header_record **':
      hdr = ffi.cast(mod_type, buf)
      rec['id'] = hdr[0].base_rec.id
      rec['rank'] = hdr[0].base_rec.rank
      mm = hdr[0].memory_mode & ~(1 << 31) 
      cm = hdr[0].cluster_mode & ~(1 << 31) 
      rec['nblades'] = hdr[0].nblades
      rec['nchassis'] = hdr[0].nchassis
      rec['ngroups'] = hdr[0].ngroups
      rec['memory_mode'] = memory_modes[mm]
      rec['cluster_mode'] = cluster_modes[cm]
      rec['appid'] = hdr[0].appid
    else:
      prf = ffi.cast(mod_type, buf)
      rec['id'] = prf[0].base_rec.id
      rec['rank'] = prf[0].base_rec.rank
      rec['group'] = prf[0].group
      rec['chassis'] = prf[0].chassis
      rec['blade'] = prf[0].blade
      rec['node'] = prf[0].node
      
      lst = []
      for i in range(0, len(prf[0].counters)):
        lst.append(prf[0].counters[i])
      np_counters = np.array(lst, dtype=np.uint64)
      d_counters = dict(zip(counter_names(mod_name), np_counters))
      
      rec['counters'] = {}
      rec['counters'].update(d_counters)
       
    return rec

