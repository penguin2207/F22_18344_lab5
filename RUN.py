import os

KV_SIZE = '1000'
THREADS = ['1', '2', '14', '28', '56', '112']
ITERS = ['50000', '100000', '200000', '400000']
VARIANT = ['4']

fout = 'run6_ece030_MAX_TRIES_15_NO_STATS.txt'

for v in VARIANT:
    for t in THREADS:
        for i in ITERS:
            #os.system('./gups ' + KV_SIZE + ' ' + t + ' ' + i + ' ' + v + ' >> ' + 'results/' + fout)
            os.system('/usr/bin/time -o ' + 'results/' + fout + ' --verbose --append ./gups ' + KV_SIZE + ' ' + t + ' ' + i + ' ' + v)

            
