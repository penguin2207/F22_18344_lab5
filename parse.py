import re
import csv

inf = open("results/SWAPS_ece031_MAX_TRIES_2.txt", 'r')
filetext = inf.read()
inf.close()

outf = 'SWAPS_1_parsed.csv'

stats=0

count=0

KV_SIZE = '1000'
THREADS = ['1', '2', '14', '28', '56', '112']
ITERS = ['50000', '100000', '200000', '400000']
VARIANT = ['1', '2', '3','4']

tuples = []

for v in VARIANT:
    for t in THREADS:
        for i in ITERS:
            tuples.append((KV_SIZE, t, i, v))

header = ['KV_SIZE', 'THREADS', 'ITERATIONS','VARIATION','TIME','COMMITS','ABORTS','FALLBACKS']


times = []
commits = []
aborts = []
fallbacks = []

for time in re.finditer(r'\b\d{1}:\d{2}\D{1}\d{2}', filetext):
    secs = 60*(int(time.group()[0])) + 10*(int(time.group()[2])) + (int(time.group()[3])) + (int(time.group()[5]))/10.0 + (int(time.group()[6]))/100.0
    times.append(round(secs,2))

for commit in re.finditer(r'Commits: \d+', filetext):
    commits.append(commit.group().replace('Commits: ', '', 1))

for abort in re.finditer(r'Aborts: \d+', filetext):
    aborts.append(abort.group().replace('Aborts: ', '', 1))

for fallback in re.finditer(r'Fallbacks: \d+', filetext):
    fallbacks.append(fallback.group().replace('Fallbacks: ', '', 1))

if(stats):

    with open('results/'+outf, 'w') as f:
        writer = csv.writer(f)

        writer.writerow(header)

        for tuple in tuples:
            data = [tuple[0], tuple[1], tuple[2], tuple[3], times[count], commits[count], aborts[count], fallbacks[count]]
            writer.writerow(data)
            count+=1
else:

    with open('results/'+outf, 'w') as f:
        writer = csv.writer(f)

        writer.writerow(header)

        for tuple in tuples:
            data = [tuple[0], tuple[1], tuple[2], tuple[3], times[count]]
            writer.writerow(data)
            
            count+=1