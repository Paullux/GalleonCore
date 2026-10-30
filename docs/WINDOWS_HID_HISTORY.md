# Historical Windows HID observations

These notes describe the October 2026 investigation, not current release gates.
See ROADMAP.md at the repository root for current status.

Windows HID diagnostics: the owner reproduced initial inactive controls after
PR #29, both after ten passive minutes and after manual start on a ready
session; images still work and the PID remains stable through a temporary
Corsair-logo recovery. Do not claim early startup ordering solved it or infer
handle reopening from the PID. The bounded, local Windows counter log contains
no report contents. Compare failing-first-open and recovered snapshots before
changing protocol bytes, HID ownership or retry policy. Keep the publication
gate until the owner confirms physical Windows behavior.

On 21 October, the owner reported working LCD keys and dials after updating the
Stream Deck firmware through Elgato's Windows app and then uninstalling that
app. The screenshot shows `3.06.005`. Those two interventions were not tested
separately; record the result as an observed recovery, not a proven firmware
root cause. Continue cold-start and long-idle hardware checks before the first
tag or distribution publication.
