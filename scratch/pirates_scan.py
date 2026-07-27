import sys, collections
sys.stdout.reconfigure(encoding='utf-8')

d = open('assets/pirates_1.img','rb').read()

# Autocorrelation over candidate region: for each lag, count matching bytes
region = d[0x1c000:0x32000]
n = len(region)
print(f'region len {n}')
best = []
for lag in range(16, 1200):
    match = sum(1 for i in range(0, n - lag, 7) if region[i] == region[i+lag])
    total = len(range(0, n - lag, 7))
    best.append((match/total, lag))
best.sort(reverse=True)
print('top strides:')
for score, lag in best[:20]:
    print(f'  lag {lag:5d}  match {score:.4f}')
