import re
import csv

inf = open("results/15counts.txt", 'r')
filetext = inf.read()
inf.close()

outf = 'SWAPS_15_counts_parsed.csv'

stats=1

count=0

KV_SIZE = '1000'
THREADS = ['1', '2', '14', '28', '56', '112']
ITERS = ['2000']
SWAPS = ['100', '250', '500']
VARIANT = ['2']

tuples = []

for v in VARIANT:
    for t in THREADS:
        for s in SWAPS:
            for i in ITERS:
                tuples.append((KV_SIZE, s, t, i, v))

header = ['KV_SIZE', 'SWAPS', 'THREADS', 'ITERATIONS','VARIATION','TIME','COMMITS','ABORTS','FALLBACKS']


times = []
commits = []
aborts = []
fallbacks = []
cas = []
count2=0
for time in re.finditer(r' \d{1}:\d{2}\D{1}\d{2}', filetext):
    secs = 60*(int(time.group()[1])) + 10*(int(time.group()[3])) + (int(time.group()[4])) + (int(time.group()[6]))/10.0 + (int(time.group()[7]))/100.0
    #if((count2+1) % 4 ==0):
    times.append(round(secs,2))
    count2+=1
    print(time.group())
    print(secs)

count3=0
for commit in re.finditer(r'Commits: \d+', filetext):
    if((count3+1) % 4 ==0):
        commits.append(commit.group().replace('Commits: ', '', 1))
    count3+=1

count4=0
for abort in re.finditer(r', Aborts: \d+', filetext):
    if((count4+1) % 4 ==0):
        aborts.append(abort.group().replace(', Aborts: ', '', 1))
    count4+=1

count5=0
for fallback in re.finditer(r'Fallback: \d+', filetext):
    if((count5+1) % 4 ==0):
        fallbacks.append(fallback.group().replace('Fallback: ', '', 1))
    count5+=1


count6=0
for ca in re.finditer(r'Conflict Aborts: \d+', filetext):
    if((count6+1) % 4 ==0):
        cas.append(ca.group().replace('Conflict Aborts: ', '', 1))
    count6+=1

if(stats):

    with open('results/'+outf, 'w') as f:
        writer = csv.writer(f)

        writer.writerow(header)

        for tuple in tuples:
            data = [tuple[0], tuple[1], tuple[2], tuple[3], tuple[4], commits[count], aborts[count], fallbacks[count], cas[count]]
            writer.writerow(data)
            count+=1

else:

    with open('results/'+outf, 'w') as f:
        writer = csv.writer(f)

        writer.writerow(header)

        for tuple in tuples:
            data = [tuple[0], tuple[1], tuple[2], tuple[3], tuple[4], times[count]]
            writer.writerow(data)
            count+=1
            