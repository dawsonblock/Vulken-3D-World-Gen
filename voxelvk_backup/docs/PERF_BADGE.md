
# Performance Badge

After `main` runs, CI publishes a `badge.json` alongside perf artifacts on GitHub Pages.

Add this to your README (replace `<OWNER_OR_USER>` and `<REPO>`):

```md
![frametime](https://img.shields.io/endpoint?url=https://<OWNER_OR_USER>.github.io/<REPO>/badge.json)
```

Color thresholds (by mean frametime, lower is better):
- **< 12 ms** → brightgreen
- **< 16 ms** → green
- **< 22 ms** → yellow
- **≥ 22 ms** → red
