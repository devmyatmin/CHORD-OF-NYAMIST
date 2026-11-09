# Device profiles

This directory stores small, reviewed, machine-readable hardware profiles.

- Keep shared runtime code outside device profiles.
- Add a profile only from observable device evidence.
- Mark a capability as declared, registered, or executed; do not treat these states as interchangeable.
- Keep raw captures, private dumps, proprietary libraries, firmware, and credentials out of Git.
- Link detailed test conditions and limitations from a separate evidence report.

The initial reference profile is [MediaTek MT6991 / REDMI K80 Ultra](mt6991/profile.json).
