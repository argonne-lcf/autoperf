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

  if ('APXC' not in report.modules):
    print ("This log does not contain AutoPerf XC data")
    return

  hdr = darshan.backend.cffi_backend.log_get_apxc_record(report.log)

  #print("{:<8}{:<16}{:<10}{:<15}{:<18}{:<10}{:<10}{:<10}{:<10}{:<10}{:<10}\n{}".format(
  #   "Rank","Call", "Count", "Total Bytes", "Total Time", "0-256", "256-1K", "1K-8K", "8K-256K", "256K-1M", "1M+", "="*120))

  r = darshan.backend.cffi_backend.log_get_apxc_record(report.log)
  print(r)
  return

if __name__ == '__main__':
  main()
