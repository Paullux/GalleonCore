# Code signing: drafts for the SignPath Foundation application

**Status: drafts, nothing published, no application sent from this folder.** The owner decided (14 November 2026) to ask the
[SignPath Foundation](https://signpath.org/) for a free code signing certificate. This folder keeps what the application
will need, so that it is ready when the request is made. Nothing here is linked from the website or from the README yet.

## What is here

- `policy.en.md`, `policy.fr.md`, `policy.de.md`, `policy.es.md`: the **code signing policy** (roles, attribution, privacy),
  in the four languages of the site. SignPath asks for it on the home page and on the download/release pages. The German and
  Spanish versions are drafts to be read by a native speaker, as for the README (see `docs/TRANSLATIONS.md`).
- This file: the checklist of what must be true before the request, and where each requirement stands.

## Placeholders

Every `[TO CONFIRM: ...]` in the policies is a decision or a fact that the owner must settle before publishing. Do not
publish a policy that still has one. The list of the network contacts of the programs (privacy section) was written from the
code on 14 November 2026 and must be checked again against the code at the time of the request.

## Requirements of the programme, and where we stand

Read from SignPath's terms on 14 November 2026; check the current page before applying, they change.

| Requirement | Standing |
| --- | --- |
| OSI-approved licence, no commercial dual licensing, for all components | GPL-3.0-or-later and LGPL-3.0-or-later; CC-BY-SA-4.0 for artwork. To confirm: no proprietary component is shipped in the Windows Setup. |
| Actively maintained, already released in the form to be signed, functionality described on the download page | v0.2.3 published; the site describes the programs. |
| Signed binaries built from our own source by an automated, verifiable build | `.github/workflows/windows-core.yml` builds the ZIP and the NSIS Setup. To do: the signing step and the SignPath integration. |
| Metadata enforced: product name identical to the project name, version identical across builds | **Not done.** Only the Setup carries a description today (`tools/packaging/windows/galleoncore.nsi`); `galleond.exe`, `galleon-bosun.exe` and `galleon-keyboard-watchdog.exe` need a version resource with the same product name and version. |
| Each release approved manually before signing | To set up in SignPath. |
| Multi-factor authentication for everyone who signs or touches the repository | [TO CONFIRM: GitHub 2FA of the owner and of every collaborator with write access.] |
| The team that signs also develops and owns the project; clear roles (authors, reviewers, approvers) | The owner is the only author. [TO CONFIRM: who reviews, who approves.] |
| Code signing policy published on the home page and the download/release pages | The drafts in this folder; to add to `site/` (four languages) and to the release notes. |
| Uninstallation possible; system changes announced | The NSIS Setup has an uninstaller. The start-up entry (HKCU Run) and the Linux udev rule must be named in the policy or on the download page. |
| Help investigate complaints | [TO CONFIRM: contact address or issue tracker.] |

## Known limits

- A signature does not remove the Windows SmartScreen warning at once: the reputation of a publisher builds with downloads.
- The certificate names the SignPath Foundation as publisher, not the project.
- Acceptance is at SignPath's discretion, without appeal.
