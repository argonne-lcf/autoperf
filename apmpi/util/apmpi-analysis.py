#!/usr/bin/env python
import argparse
import darshan

# base counter names
cnames = ['MPI_SEND',
          'MPI_RECV',
          'MPI_ALLREDUCE']

def main():
  
  parser = argparse.ArgumentParser()
  parser.add_argument('--quiet', dest='quiet', action='store_true', default=False, help='Surpress zero count calls')
  parser.add_argument('logname', metavar="logname", type=str, nargs=1, help='Logname to parse')
  args = parser.parse_args()

  report = darshan.DarshanReport(args.logname[0], read_all=False)
  report.info()

  if ('APMPI' not in report.modules):
    print ("This log does not contain AutoPerf MPI data")
    return

  hdr = darshan.backend.cffi_backend.log_get_apmpi_record(report.log)

  print("{:<8}{:<16}{:<10}{:<15}{:<18}{:<10}{:<10}{:<10}{:<10}{:<10}{:<10}\n{}".format(
     "Rank","Call", "Count", "Total Bytes", "Total Time", "0-256", "256-1K", "1K-8K", "8K-256K", "256K-1M", "1M+", "="*120))

  r = darshan.backend.cffi_backend.log_get_apmpi_record(report.log)
  
  while (r):
    for c in cnames:
      # counter fields for each base type
      ncall  = c
      ncount = c + '_CALL_COUNT'
      nsize  = c + '_TOTAL_BYTES'
      ntime  = c + '_TOTAL_TIME'
      h0     = c + '_MSG_SIZE_AGG_0_256'
      h1     = c + '_MSG_SIZE_AGG_256_1K'
      h2     = c + '_MSG_SIZE_AGG_1K_8K'
      h3     = c + '_MSG_SIZE_AGG_8K_256K'
      h4     = c + '_MSG_SIZE_AGG_256K_1M'
      h5     = c + '_MSG_SIZE_AGG_1M_PLUS'

      if (r['counters'][ncount] > 0 or not args.quiet):
        print("{rank:<8}{call:<16}{count:<10}{size:<15}{time:<18.6f}{h0:<10}{h1:<10}{h2:<10}{h3:<10}{h4:<10}{h5:<10}".format(
        rank=r['rank'],
        call=ncall,
        count=r['counters'][ncount],
        size=r['counters'][nsize],
        time=r['fcounters'][ntime],
        h0=r['counters'][h0],
        h1=r['counters'][h1],
        h2=r['counters'][h2],
        h3=r['counters'][h3],
        h4=r['counters'][h4],
        h5=r['counters'][h5])) 
    r = darshan.backend.cffi_backend.log_get_apmpi_record(report.log)

  return

if __name__ == '__main__':
  main()
