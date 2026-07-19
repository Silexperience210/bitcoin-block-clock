# train_trend_model.py — entraîne un mini MLP (6->10->3) sur 1 an de BTC
# et exporte les poids + normalisation dans trend_model.h
# Usage : .venv/Scripts/python train_trend_model.py
import json, math, urllib.request
import numpy as np

DAYS = 1095
HIDDEN = 8
EPOCHS = 250
LR = 0.01
L2 = 2e-3
SEED = 42

# ---------- 1. Données : prix daily Kraken (OHLC, ~720 jours, sans clé) ----------
url = "https://api.kraken.com/0/public/OHLC?pair=XBTUSD&interval=1440"
req = urllib.request.Request(url, headers={"User-Agent": "blockclock-train/1.0"})
raw = json.load(urllib.request.urlopen(req, timeout=30))
p = np.array([float(d[4]) for d in raw["result"]["XXBTZUSD"]], dtype=np.float64)
print("points daily:", len(p))

# ---------- 2. Features / labels ----------
def rsi(closes, n=14):
    d = np.diff(closes)
    up = np.where(d > 0, d, 0.0)
    dn = np.where(d < 0, -d, 0.0)
    ru = up[-n:].mean(); rd = dn[-n:].mean()
    if rd == 0: return 100.0
    return 100.0 - 100.0 / (1.0 + ru / rd)

X, Y = [], []
for t in range(30, len(p) - 1):
    w = p[t - 29:t + 1]                       # 30 derniers jours
    rets = np.diff(p[t - 14:t + 1]) / p[t - 14:t]
    f = [p[t] / p[t - 1] - 1.0,               # chg 1j
         p[t] / p[t - 7] - 1.0,               # chg 7j
         rsi(p[t - 14:t + 1]) / 100.0,        # RSI14
         rets.std(),                          # volatilité 14j
         p[t] / w.max() - 1.0,                # dist au plus haut 30j
         p[t] / w.min() - 1.0]                # dist au plus bas 30j
    r = p[t + 1] / p[t] - 1.0
    X.append(f)
    Y.append(0 if r > 0.01 else (2 if r < -0.01 else 1))   # 0=up 1=flat 2=down
X = np.array(X); Y = np.array(Y)
print("echantillons:", len(X), " classes:", np.bincount(Y))

# ---------- 3. Split + normalisation ----------
n = len(X); ntr = int(n * 0.8)
mu = X[:ntr].mean(axis=0); sd = X[:ntr].std(axis=0) + 1e-9
Xs = (X - mu) / sd
Yoh = np.eye(3)[Y]

# ---------- 4. MLP numpy (tanh -> softmax), Adam full-batch ----------
rng = np.random.default_rng(SEED)
W1 = rng.normal(0, 0.5, (HIDDEN, 6)); b1 = np.zeros(HIDDEN)
W2 = rng.normal(0, 0.5, (3, HIDDEN)); b2 = np.zeros(3)
params = [W1, b1, W2, b2]
mt = [np.zeros_like(q) for q in params]; vt = [np.zeros_like(q) for q in params]

def forward(Xb):
    z1 = Xb @ W1.T + b1
    a1 = np.tanh(z1)
    z2 = a1 @ W2.T + b2
    z2 -= z2.max(axis=1, keepdims=True)
    e = np.exp(z2); sm = e / e.sum(axis=1, keepdims=True)
    return a1, sm

for ep in range(1, EPOCHS + 1):
    a1, sm = forward(Xs[:ntr])
    m = ntr
    dz2 = (sm - Yoh[:ntr]) / m
    gW2 = dz2.T @ a1; gb2 = dz2.sum(axis=0)
    da1 = dz2 @ W2; dz1 = da1 * (1 - a1 ** 2)
    gW1 = dz1.T @ Xs[:ntr]; gb1 = dz1.sum(axis=0)
    grads = [gW1 + L2 * W1, gb1, gW2 + L2 * W2, gb2]
    for i in range(4):
        mt[i] = 0.9 * mt[i] + 0.1 * grads[i]
        vt[i] = 0.999 * vt[i] + 0.001 * grads[i] ** 2
        mh = mt[i] / (1 - 0.9 ** ep); vh = vt[i] / (1 - 0.999 ** ep)
        params[i] -= LR * mh / (np.sqrt(vh) + 1e-8)

# ---------- 5. Accuracy ----------
def acc(sl):
    _, sm = forward(Xs[sl])
    return (sm.argmax(axis=1) == Y[sl]).mean()
acc_tr = acc(slice(0, ntr)); acc_te = acc(slice(ntr, n))
baseline = np.bincount(Y[ntr:]).max() / (n - ntr)
print("accuracy train %.1f%%  test %.1f%%  (baseline majoritaire %.1f%%)"
      % (acc_tr * 100, acc_te * 100, baseline * 100))

# ---------- 6. Export C ----------
def arr(name, a, fmt="%.6ff"):
    flat = a.flatten()
    lines = ["const float %s[%d] PROGMEM = {" % (name, flat.size)]
    for i in range(0, flat.size, 8):
        lines.append("  " + ",".join(fmt % v for v in flat[i:i + 8]) + ",")
    lines.append("};")
    return "\n".join(lines)

with open("trend_model.h", "w", encoding="ascii") as f:
    f.write("// trend_model.h - MLP 6->%d->3 (up/flat/down), genere par train_trend_model.py\n" % HIDDEN)
    f.write("// train %.1f%% / test %.1f%% sur %d jours BTC\n" % (acc_tr * 100, acc_te * 100, len(X)))
    f.write("#pragma once\n#include <pgmspace.h>\n\n")
    f.write("#define TM_IN 6\n#define TM_H %d\n\n" % HIDDEN)
    f.write(arr("TM_MU", mu) + "\n\n")
    f.write(arr("TM_SD", sd) + "\n\n")
    f.write(arr("TM_W1", W1) + "\n\n")
    f.write(arr("TM_B1", b1) + "\n\n")
    f.write(arr("TM_W2", W2) + "\n\n")
    f.write(arr("TM_B2", b2) + "\n")
print("trend_model.h ecrit")
