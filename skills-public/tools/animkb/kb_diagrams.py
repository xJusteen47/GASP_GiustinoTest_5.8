#!/usr/bin/env python3
"""kb_diagrams.py <out_dir> - draws the synthetic teaching diagrams of docs/knowledge (no source footage needed).

D-spacing-charts.png   linear / ease-in / ease-out / ease-in-out / accelerate-into-hit, one tick per frame
D-favouring.png        breakdown at 50% vs favouring a key (same timing, different character)
D-overlap-chain.png    successive breaking of joints: root leads, each child lags and overshoots
D-vfx-envelope.png     anticipation / impact / dissipation envelopes for size, opacity, brightness
D-dust-growth.png      dust and smoke: grows fast, keeps growing while fading, never shrinks
"""
import sys, os, numpy as np
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt

out = sys.argv[1]; os.makedirs(out, exist_ok=True)

def ruler(ax, y, xs, label, col):
    ax.plot(xs, np.full_like(xs, y), '|', ms=16, color=col); ax.plot([xs[0], xs[-1]], [y, y], color=col, lw=0.6)
    ax.text(-0.02, y, label, ha='right', va='center', fontsize=9)

# spacing charts
t = np.linspace(0, 1, 13)
curves = [('linear (mechanical)', t), ('ease-in / slow-in (settles onto B)', 1 - (1 - t) ** 2.4),
          ('ease-out / slow-out (leaves A heavy)', t ** 2.4), ('ease-in-out (default spline: generic)', t * t * (3 - 2 * t)),
          ('accelerate into hit (no slow-in)', t ** 3)]
fig, ax = plt.subplots(figsize=(11, 4))
for i, (n, x) in enumerate(curves): ruler(ax, -i, x, n, plt.cm.tab10(i))
ax.text(0, 0.6, 'A', fontsize=12, ha='center'); ax.text(1, 0.6, 'B', fontsize=12, ha='center')
ax.set_xlim(-0.55, 1.05); ax.set_ylim(-4.6, 1); ax.axis('off')
ax.set_title('Same timing (12 intervals), five spacings: one tick per frame')
fig.tight_layout(); fig.savefig(f'{out}/D-spacing-charts.png', dpi=110); plt.close(fig)

# favouring
fig, ax = plt.subplots(figsize=(11, 2.8))
for i, (n, f) in enumerate([('breakdown at 50% (bland)', 0.5), ('favours B: 75% at half time (decisive)', 0.75), ('favours A: 25% at half time (hesitant, then snaps)', 0.25)]):
    a = np.linspace(0, f, 7); b = np.linspace(f, 1, 7)[1:]
    ruler(ax, -i, np.r_[a, b], n, plt.cm.tab10(i)); ax.plot([f], [-i], 'o', color='k')
ax.set_xlim(-0.6, 1.05); ax.set_ylim(-2.6, 0.6); ax.axis('off'); ax.set_title('Breakdown placement (dot = breakdown on the middle frame)')
fig.tight_layout(); fig.savefig(f'{out}/D-favouring.png', dpi=110); plt.close(fig)

# overlap chain
fr = np.arange(0, 60)
fig, ax = plt.subplots(1, 2, figsize=(12, 3.8))
def segment(lag, over):
    x = np.clip((fr - lag) / 14, 0, None); y = 1 - np.exp(-3 * x) * np.cos(4.2 * x * over)
    return np.where(fr < lag, 0, y)
for i, n in enumerate(['hips (lead)', 'chest', 'upper arm', 'forearm', 'hand']):
    ax[0].plot(fr + 1, 0.98 * (1 - (1 - np.clip(fr / 14, 0, 1)) ** 2), label=n)
    ax[1].plot(fr + 1, segment(i * 3, 0.15 + 0.25 * i), label=n)
ax[0].set_title('Lockstep: every segment starts, peaks and stops together'); ax[1].set_title('Successive breaking: each child lags 2-4 f and overshoots more')
for a in ax: a.set_xlabel('frame (60 fps)'); a.set_ylabel('rotation toward target (normalised)'); a.legend(fontsize=7)
fig.tight_layout(); fig.savefig(f'{out}/D-overlap-chain.png', dpi=110); plt.close(fig)

# vfx envelope
f = np.arange(0, 40)
size = np.where(f < 4, 0.3 * f / 4, np.where(f < 6, 0.3 + 0.7 * (f - 4) / 2, 1 + 0.25 * (1 - np.exp(-(f - 6) / 8))))
alpha = np.where(f < 6, np.minimum(1, f / 4), np.exp(-(f - 6) / 9))
bright = np.where(f < 4, 0.2, np.where(f < 8, 1.0, np.exp(-(f - 8) / 3)))
fig, ax = plt.subplots(figsize=(10, 3.6))
ax.plot(f + 1, size, label='size'); ax.plot(f + 1, alpha, label='opacity'); ax.plot(f + 1, bright, label='brightness / emissive')
for x, n in ((1, 'anticipation'), (5, 'impact'), (9, 'dissipation')): ax.axvline(x, color='#999', lw=0.6); ax.text(x + 0.3, 1.27, n, fontsize=8)
ax.set_xlabel('frame (60 fps)'); ax.set_ylim(0, 1.4); ax.legend(fontsize=8)
ax.set_title('Effect envelope: short build-up, peak on the impact frame, long readable decay')
fig.tight_layout(); fig.savefig(f'{out}/D-vfx-envelope.png', dpi=110); plt.close(fig)

# dust growth
f = np.arange(0, 90)
good_r = 1 - np.exp(-f / 7) + 0.004 * f; good_a = np.exp(-f / 30)
bad_r = np.where(f < 30, 1 - np.exp(-f / 7), (1 - np.exp(-30 / 7)) * np.exp(-(f - 30) / 25)); bad_a = np.ones_like(f, float) * 0.9
fig, ax = plt.subplots(1, 2, figsize=(12, 3.6), sharey=True)
ax[0].plot(f + 1, good_r, label='radius'); ax[0].plot(f + 1, good_a, label='opacity'); ax[0].set_title('Right: fast burst, keeps expanding slowly, fades to 0')
ax[1].plot(f + 1, bad_r, label='radius'); ax[1].plot(f + 1, bad_a, label='opacity'); ax[1].set_title('Wrong: shrinks back while still opaque (reads as a balloon)')
for a in ax: a.set_xlabel('frame (60 fps)'); a.legend(fontsize=8)
fig.tight_layout(); fig.savefig(f'{out}/D-dust-growth.png', dpi=110); plt.close(fig)
print('wrote 5 diagrams to', out)
