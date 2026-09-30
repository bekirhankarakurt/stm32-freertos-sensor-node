#!/usr/bin/env python3
"""Turn a gcovr JSON summary into a shields.io endpoint badge description."""
import json
import sys

summary = json.load(open(sys.argv[1]))
pct = float(summary["line_percent"])
color = "brightgreen" if pct >= 85 else "green" if pct >= 75 else "yellow" if pct >= 60 else "red"
badge = {"schemaVersion": 1, "label": "coverage", "message": f"{pct:.1f}%", "color": color}
with open(sys.argv[2], "w") as f:
    json.dump(badge, f)
print(badge)
