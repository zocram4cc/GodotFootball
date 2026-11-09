#!/usr/bin/env python3
"""plot_engine_pose.py DUMP OUT.png: draws the 20 joints PES's anime engine left for each pose in DUMP as a stick figure (front: x/y, side: z/y).

DUMP is written by `GF_ENGINE_POSE_DUMP=<file> pes_anime_engine_test` (the `parents` row, then one `pose LABEL x y z ...` row per pose; joint order
and parents are the rig's, positions are the control's model-space joint array, PES axes: y up, z forward).
Usage: python3 plot_engine_pose.py DUMP OUT.png [LABEL ...]   (default: every pose)
"""
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    dump, out, only = sys.argv[1], sys.argv[2], sys.argv[3:]
    parents, poses = [], []
    for line in open(dump):
        p = line.split()
        if p[0] == "parents":
            parents = [int(v) for v in p[1:]]
        elif p[0] == "pose" and (not only or p[1] in only):
            v = [float(x) for x in p[2:]]
            poses.append((p[1], [v[i:i + 3] for i in range(0, len(v), 7)]))
    cols = min(4, len(poses))
    rows = (len(poses) + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols * 2, figsize=(3.2 * cols * 2, 4.2 * rows), squeeze=False)
    for k, (label, joints) in enumerate(poses):
        r, c = divmod(k, cols)
        for side, (ax, h) in enumerate(((axes[r][2 * c], 0), (axes[r][2 * c + 1], 2))):
            for j, par in enumerate(parents):
                if par >= 0:
                    ax.plot([joints[par][h], joints[j][h]], [joints[par][1], joints[j][1]], "k-", lw=1.2)
            ax.plot([q[h] for q in joints], [q[1] for q in joints], "r.", ms=4)
            ax.set_xlim(-1.0, 1.0)
            ax.set_ylim(-0.1, 2.0)
            ax.set_aspect("equal")
            ax.set_title("%s %s" % (label, "front (x,y)" if side == 0 else "side (z,y)"), fontsize=8)
    for k in range(len(poses), rows * cols):
        r, c = divmod(k, cols)
        axes[r][2 * c].axis("off")
        axes[r][2 * c + 1].axis("off")
    fig.tight_layout()
    fig.savefig(out, dpi=70)


main()
