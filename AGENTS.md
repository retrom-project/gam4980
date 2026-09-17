# Retrom GAM4980 fork

`main` mirrors ThisBoringWorld/gam4980 without Retrom patches.
`retrom/geeaa531b55e7` is the maintenance baseline. Feature work belongs on
`feat/*`, `fix/*` or `build/*` branches based on that baseline.

Read `retrom-fork.json` and `docs/MAINTENANCE.md` before changing build inputs.
Core code, native regressions and browser candidate builds belong in this fork.
retrom-runtime only consumes verified artifacts and declares the public Target.
Do not add third-party games, BIOS, generated binaries or credentials to Git.
The inherited upstream example files are excluded from candidate source archives.

Before changing behavior, add a regression that fails on the old implementation.
Run `.github/rpg-runtime/test-native.sh` and build browser candidates through
Retrom `pfb-core-build CORE=gam4980` from the same named PFB.
Verify Review Preview, Product Launch, standard controller input, instant state,
a different Launch restoring that state and continued input in actual Retrom.

Future immutable release tags use `retrom-core-geeaa531b55e7-rN` or `-rc.N`.
Merge and release only with explicit authorization. Never treat a local candidate
as a published release or place unpublished coordinates in production locks.
