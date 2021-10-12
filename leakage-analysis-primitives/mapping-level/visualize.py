import matplotlib.pyplot as plt
import sys
import csv 


fn = "timing.csv"
if len(sys.argv) == 2:
    fn = sys.argv[1]

tlb = []
err_tlb = []

with open(fn, "r") as c:
    reader = csv.DictReader(c)
    for row in reader:
        tlb.append(int(row["tlb"]))
        err_tlb.append(float(row["errtlb"]))

ind = range(len(tlb))

# correct offset
base = min(tlb) * 0.8
plt.bar(ind, [x - base for x in tlb], 0.25, label='TLB', color='blue')
plt.errorbar(ind, [x - base for x in tlb], label='TLB', color='black', yerr=err_tlb, fmt="x")

#plt.plot(data)
plt.legend(loc='best')

plt.show()

