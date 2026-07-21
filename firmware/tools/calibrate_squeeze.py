# -*- coding: utf-8 -*-
"""
calibrate_squeeze.py — Calibration OFFLINE des signaux momentum/direction.

Philosophie du projet : AUCUN ML embarqué, aucune promesse. On mesure sur
l'historique complet BTCUSDT (Binance, daily, 2017->present) les frequences
REELLES de ce qui suit chaque etat de compression/momentum, et on embarque
ces frequences comme table C (signals_calib.h). Le firmware ne fait que :
  etat courant -> lookup -> "P(direction) = X% (mesure sur N cas)".

Etats calcules (vue LARGE : daily + weekly, horizons 7j et 30j) :
  - TTM Squeeze daily  : Bollinger(20,2) DANS Keltner(20,1.5xATR)
  - TTM Squeeze weekly : idem sur bougies hebdo (resample)
  - BBW percentile     : largeur Bollinger classee sur 120 j glissants
  - Duree du squeeze   : jours consecutifs en compression
  - Momentum TTM       : linreg 20j de (close - (mid_Donchian+SMA)/2),
                         signe = biais directionnel, force en terciles
  - Regime 90j         : ROC 90 jours (tendance de fond)

Sorties mesurees :
  - P(ret_H a le meme signe que le momentum)   [direction]
  - P(|ret_H| > 5% / 10% / 20%)                [expansion]
  - mediane de |ret_H|
  vs BASELINE inconditionnelle (honnetete : on affiche l'edge, pas un chiffre
  sorti du contexte).

Usage :  python3 calibrate_squeeze.py          # ecrit ../bitcoin-block-clock/signals_calib.h
"""
import json, time, urllib.request, sys, os
import numpy as np

API = "https://data-api.binance.vision/api/v3/klines"
SYM = "BTCUSDT"

# ----------------------------------------------------------- telechargement
def fetch_all_daily():
    out = []
    start = 1502928000000  # 2017-08-17 (listing BTCUSDT)
    while True:
        u = f"{API}?symbol={SYM}&interval=1d&startTime={start}&limit=1000"
        d = json.loads(urllib.request.urlopen(u, timeout=20).read())
        if not d:
            break
        out += d
        if len(d) < 1000:
            break
        start = d[-1][0] + 86400000
        time.sleep(0.15)
    # kline: [openTime, open, high, low, close, volume, ...]
    ts = np.array([k[0] // 1000 for k in out], dtype=np.int64)
    o = np.array([float(k[1]) for k in out])
    h = np.array([float(k[2]) for k in out])
    l = np.array([float(k[3]) for k in out])
    c = np.array([float(k[4]) for k in out])
    return ts, o, h, l, c

# ------------------------------------------------------------- indicateurs
def sma(x, n):
    r = np.full(len(x), np.nan)
    cs = np.cumsum(np.insert(x, 0, 0.0))
    r[n - 1:] = (cs[n:] - cs[:-n]) / n
    return r

def ema(x, n):
    r = np.empty(len(x)); a = 2.0 / (n + 1)
    r[0] = x[0]
    for i in range(1, len(x)):
        r[i] = a * x[i] + (1 - a) * r[i - 1]
    return r

def atr(h, l, c, n=20):
    tr = np.maximum(h[1:] - l[1:],
         np.maximum(abs(h[1:] - c[:-1]), abs(l[1:] - c[:-1])))
    tr = np.insert(tr, 0, h[0] - l[0])
    return ema(tr, n)

def rolling_std(x, n):
    r = np.full(len(x), np.nan)
    for i in range(n - 1, len(x)):
        r[i] = np.std(x[i - n + 1:i + 1])
    return r

def linreg_slope_endpoint(x, n):
    """valeur du fit lineaire au dernier point, sur fenetre n (momentum TTM)."""
    r = np.full(len(x), np.nan)
    t = np.arange(n); tm = t.mean()
    den = ((t - tm) ** 2).sum()
    for i in range(n - 1, len(x)):
        w = x[i - n + 1:i + 1]
        b = ((t - tm) * (w - w.mean())).sum() / den
        a = w.mean() - b * tm
        r[i] = a + b * (n - 1)
    return r

def compute_states(h, l, c, bb_n=20, bb_k=2.0, kc_k=1.5, pctl_win=120):
    m = sma(c, bb_n)
    sd = rolling_std(c, bb_n)
    bb_up, bb_lo = m + bb_k * sd, m - bb_k * sd
    e = ema(c, bb_n); a = atr(h, l, c, bb_n)
    kc_up, kc_lo = e + kc_k * a, e - kc_k * a
    squeeze = (bb_up < kc_up) & (bb_lo > kc_lo)          # TTM "on"
    bbw = (bb_up - bb_lo) / m * 100.0
    # percentile glissant du BBW
    pct = np.full(len(c), np.nan)
    for i in range(pctl_win, len(c)):
        w = bbw[i - pctl_win:i]
        pct[i] = (w < bbw[i]).mean() * 100.0
    # duree en squeeze
    dur = np.zeros(len(c), dtype=int)
    for i in range(1, len(c)):
        dur[i] = dur[i - 1] + 1 if squeeze[i] else 0
    # momentum TTM : close - moyenne(mid Donchian, SMA), linreg 20
    hh = np.full(len(c), np.nan); ll = np.full(len(c), np.nan)
    for i in range(bb_n - 1, len(c)):
        hh[i] = h[i - bb_n + 1:i + 1].max(); ll[i] = l[i - bb_n + 1:i + 1].min()
    mid = ((hh + ll) / 2 + m) / 2
    mom = linreg_slope_endpoint(c - mid, bb_n)
    mom_pct = mom / c * 100.0                              # normalise en % prix
    roc90 = np.full(len(c), np.nan)
    roc90[90:] = c[90:] / c[:-90] - 1.0
    return squeeze, bbw, pct, dur, mom_pct, roc90

def weekly_resample(ts, o, h, l, c):
    """bougies hebdo (lundi UTC) ; renvoie aussi map jour->index semaine close."""
    week = (ts - 345600) // 604800     # aligne lundi
    idx = {}
    W = []
    for i in range(len(ts)):
        w = week[i]
        if w not in idx:
            idx[w] = len(W); W.append([o[i], h[i], l[i], c[i]])
        else:
            j = idx[w]
            W[j][1] = max(W[j][1], h[i]); W[j][2] = min(W[j][2], l[i])
            W[j][3] = c[i]
    W = np.array(W)
    day2week = np.array([idx[w] for w in week])
    return W[:, 1], W[:, 2], W[:, 3], day2week   # h,l,c hebdo

# ------------------------------------------------------------- calibration
def pstats(rets, moms):
    """stats direction/expansion sur un sous-ensemble d'evenements."""
    n = len(rets)
    if n == 0:
        return None
    agree = np.sign(rets) == np.sign(moms)
    return dict(
        n=n,
        p_dir=float(agree.mean()),
        p5=float((np.abs(rets) > 0.05).mean()),
        p10=float((np.abs(rets) > 0.10).mean()),
        p20=float((np.abs(rets) > 0.20).mean()),
        med=float(np.median(np.abs(rets))),
    )

def main():
    print("telechargement historique BTCUSDT 1d...", flush=True)
    ts, o, h, l, c = fetch_all_daily()
    print(f"  {len(c)} jours ({time.strftime('%Y-%m-%d', time.gmtime(ts[0]))} -> "
          f"{time.strftime('%Y-%m-%d', time.gmtime(ts[-1]))})")

    sq_d, bbw, pct, dur, mom, roc90 = compute_states(h, l, c)
    wh, wl, wc, d2w = weekly_resample(ts, o, h, l, c)
    sq_w_arr, _, _, _, mom_w, _ = compute_states(wh, wl, wc, pctl_win=52)
    sq_w = sq_w_arr[np.clip(d2w - 1, 0, None)]   # semaine CLOSE (pas de lookahead)

    HORIZONS = {7: "7j", 30: "30j"}
    valid = ~np.isnan(pct) & ~np.isnan(mom) & ~np.isnan(roc90)

    results = {}
    for H in HORIZONS:
        fwd = np.full(len(c), np.nan)
        fwd[:-H] = c[H:] / c[:-H] - 1.0
        ok = valid & ~np.isnan(fwd) & (np.abs(mom) > 1e-9)
        base = pstats(fwd[ok], mom[ok])

        # --- buckets percentile BBW (compression graduee)
        PB = [(0, 5), (5, 10), (10, 20), (20, 101)]
        pcts = []
        for lo_, hi_ in PB:
            m_ = ok & (pct >= lo_) & (pct < hi_)
            pcts.append(pstats(fwd[m_], mom[m_]))

        # --- terciles de force momentum (parmi jours valides)
        am = np.abs(mom[ok])
        t1, t2 = np.percentile(am, [33.3, 66.6])
        moms_ = []
        for lo_, hi_ in [(0, t1), (t1, t2), (t2, 1e9)]:
            m_ = ok & (np.abs(mom) >= lo_) & (np.abs(mom) < hi_)
            moms_.append(pstats(fwd[m_], mom[m_]))

        # --- momentum aligne avec le regime 90j (confluence, vue large)
        conf = ok & (np.sign(mom) == np.sign(roc90)) & (np.abs(mom) >= t2)
        confl = pstats(fwd[conf], mom[conf])

        # --- squeeze stack : daily ET weekly en compression
        stack = ok & sq_d & sq_w
        stck = pstats(fwd[stack], mom[stack])
        sqonly = ok & sq_d & ~sq_w
        sqd = pstats(fwd[sqonly], mom[sqonly])

        # --- liberation de squeeze (dur>=5 hier, off aujourd'hui)
        rel = ok.copy(); rel[:] = False
        for i in range(1, len(c)):
            if dur[i - 1] >= 5 and not sq_d[i]:
                rel[i] = ok[i]
        rls = pstats(fwd[rel], mom[rel])

        results[H] = dict(base=base, pcts=pcts, moms=moms_, confl=confl,
                          stack=stck, sq_daily=sqd, release=rls,
                          mom_t1=float(t1), mom_t2=float(t2))

    # ------------------------------------------------------------ affichage
    for H, r in results.items():
        b = r["base"]
        print(f"\n=== HORIZON {HORIZONS[H]} ===  baseline: dir {b['p_dir']*100:.0f}%"
              f"  P|>10%| {b['p10']*100:.0f}%  (n={b['n']})")
        lab = ["BBW<5p", "5-10p", "10-20p", ">20p"]
        for s, la in zip(r["pcts"], lab):
            if s: print(f"  {la:8s} dir {s['p_dir']*100:.0f}%  P|>10| {s['p10']*100:.0f}%"
                        f"  P|>20| {s['p20']*100:.0f}%  med {s['med']*100:.1f}%  n={s['n']}")
        lab = ["mom faible", "mom moyen", "mom FORT"]
        for s, la in zip(r["moms"], lab):
            if s: print(f"  {la:10s} dir {s['p_dir']*100:.0f}%  n={s['n']}")
        for k, la in [("confl", "mom FORT + regime90 aligne"),
                      ("stack", "squeeze D+W (stack)"),
                      ("sq_daily", "squeeze D seul"),
                      ("release", "liberation (dur>=5)")]:
            s = r[k]
            if s: print(f"  {la:26s} dir {s['p_dir']*100:.0f}%  P|>10| {s['p10']*100:.0f}%  n={s['n']}")

    # ------------------------------------------------------- header C genere
    def row(s, b):
        if s is None: s = b
        return (f"{{{s['p_dir']*100:.0f},{s['p5']*100:.0f},{s['p10']*100:.0f},"
                f"{s['p20']*100:.0f},{s['med']*1000:.0f},{s['n']}}}")

    hpath = os.path.join(os.path.dirname(__file__) or ".",
                         "..", "bitcoin-block-clock", "signals_calib.h")
    with open(hpath, "w") as f:
        f.write("// ============================================================\n")
        f.write("// signals_calib.h — GENERE par tools/calibrate_squeeze.py\n")
        f.write(f"// Donnees: BTCUSDT 1d, {time.strftime('%Y-%m-%d', time.gmtime(ts[0]))}"
                f" -> {time.strftime('%Y-%m-%d', time.gmtime(ts[-1]))} ({len(c)} jours)\n")
        f.write("// Frequences historiques MESUREES, pas des promesses.\n")
        f.write("// Champs: {p_dir%, p_abs5%, p_abs10%, p_abs20%, med_abs_ret x1000, n}\n")
        f.write("// ============================================================\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("struct CalRow { uint8_t pDir, p5, p10, p20; uint16_t med; uint16_t n; };\n\n")
        for H, tag in HORIZONS.items():
            r = results[H]; b = r["base"]
            f.write(f"// ---------- horizon {tag} ----------\n")
            f.write(f"static const CalRow CAL{H}_BASE      = {row(b,b)};\n")
            f.write(f"static const CalRow CAL{H}_PCTL[4]   = {{  // BBW pctl <5,<10,<20,>=20\n")
            for s in r["pcts"]: f.write(f"  {row(s,b)},\n")
            f.write("};\n")
            f.write(f"static const CalRow CAL{H}_MOM[3]    = {{  // |mom| tercile 1..3\n")
            for s in r["moms"]: f.write(f"  {row(s,b)},\n")
            f.write("};\n")
            f.write(f"static const CalRow CAL{H}_CONFL     = {row(r['confl'],b)}; // mom fort+regime\n")
            f.write(f"static const CalRow CAL{H}_STACK     = {row(r['stack'],b)}; // squeeze D+W\n")
            f.write(f"static const CalRow CAL{H}_SQD       = {row(r['sq_daily'],b)};\n")
            f.write(f"static const CalRow CAL{H}_RELEASE   = {row(r['release'],b)};\n")
            f.write(f"static const float  CAL{H}_MOM_T1 = {r['mom_t1']:.4f}f, "
                    f"CAL{H}_MOM_T2 = {r['mom_t2']:.4f}f;\n\n")
    print(f"\n-> {os.path.abspath(hpath)} ecrit.")

if __name__ == "__main__":
    main()
