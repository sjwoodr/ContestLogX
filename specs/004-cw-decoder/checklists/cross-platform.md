# Cross-Platform Checklist: CW Decoder

**Purpose**: Validate that the CW Decoder spec's cross-platform (Linux / macOS / Windows) requirements are complete, clear, consistent, and measurable — before implementation begins. Items test the REQUIREMENTS, not the implementation.
**Created**: 2026-04-21
**Feature**: [spec.md](../spec.md)

---

## Requirement Completeness — Build & Dependencies

- [ ] CHK139 Are the required Qt6::Multimedia development packages documented for every supported platform (Linux apt, macOS Homebrew, Windows Qt installer)? [Completeness, Research §R7, Quickstart §1]
- [ ] CHK140 Are CMake requirements specified — is `find_package(Qt6 REQUIRED COMPONENTS Multimedia)` listed, and is the link target (`Qt6::Multimedia`) specified? [Completeness, Spec §FR-030]
- [ ] CHK141 Are CI-level build requirements specified — does the spec require that Linux, macOS, and Windows CI builds all pass with Qt6::Multimedia linked? [Completeness, Spec §SC-008]
- [ ] CHK142 Are runtime dependency requirements specified — does the spec state that end-user installers (AppImage, macOS bundle, Inno Setup EXE) MUST bundle Qt6::Multimedia libraries? [Completeness, Gap]
- [ ] CHK143 Is the macOS-specific `NSMicrophoneUsageDescription` Info.plist requirement documented as a build-time artifact requirement? [Completeness, Research §R7]
- [ ] CHK144 Are Windows-specific linker/manifest requirements (if any) for audio capture documented (e.g., no UAC manifest change required, no admin-privilege escalation)? [Completeness, Gap]

## Requirement Completeness — Platform Audio APIs

- [ ] CHK145 Are requirements specified for Linux PipeWire vs PulseAudio behavior — does the decoder need to work identically on both, or is one required? [Completeness, Research §R7, Quickstart §4]
- [ ] CHK146 Are requirements specified for Linux audio device enumeration when neither PipeWire nor PulseAudio is running (e.g., bare ALSA only)? [Completeness, Gap]
- [ ] CHK147 Are requirements specified for macOS Aggregate Device and Multi-Output Device compatibility (BlackHole, Loopback.app routing)? [Completeness, Research §R7]
- [ ] CHK148 Are requirements specified for Windows WASAPI shared-mode vs exclusive-mode operation, and which is the decoder's chosen mode? [Completeness, Research §R7]
- [ ] CHK149 Are requirements specified for Windows virtual audio cable compatibility (VB-CABLE, Voicemeeter) — identical enumeration/behavior as real devices? [Completeness, Quickstart §4]
- [ ] CHK150 Are requirements specified for hot-plug behavior on each platform — when an audio device appears or disappears during a session, does Qt6::Multimedia emit a signal that the decoder can subscribe to? [Completeness, Gap]

## Requirement Completeness — Device Identifier Behavior

- [ ] CHK151 Are requirements specified for how the decoder recovers when a device description string changes across platform versions (OS update renames a device)? [Completeness, Research §R5]
- [ ] CHK152 Are requirements specified for case-sensitivity and whitespace normalization in device descriptions — does "USB Audio CODEC" match "usb audio codec"? [Completeness, Gap]
- [ ] CHK153 Are requirements specified for device-name uniqueness — if two identical USB-audio devices are plugged in (e.g., two IC-7300s), does `QAudioDevice::description()` disambiguate them, and how does the spec handle collisions? [Completeness, Gap]
- [ ] CHK154 Are requirements specified for non-ASCII device descriptions (e.g., a Japanese-locale device name, a device with accented characters)? [Completeness, Gap]

## Requirement Completeness — Distribution

- [ ] CHK155 Are requirements specified for the AppImage build — must it include Qt6::Multimedia's platform plugin for audio capture? [Completeness, Gap]
- [ ] CHK156 Are requirements specified for the macOS .app bundle — must Qt6::Multimedia framework and plugins be bundled, and is code-signing handled identically to existing modules? [Completeness, Gap]
- [ ] CHK157 Are requirements specified for the Windows Inno Setup installer — must `Qt6Multimedia.dll` and multimedia plugin DLLs be bundled via `windeployqt`? [Completeness, Gap]

## Requirement Clarity

- [ ] CHK158 Is "the feature builds and runs on Linux, macOS, and Windows with identical user-facing behavior" quantified — what specifically counts as "user-facing behavior" (UI layout? decode accuracy? device names shown)? [Clarity, Ambiguity, Spec §SC-008]
- [ ] CHK159 Is "identical user-facing behavior" clarified with respect to platform-specific audio routing UX (since the routing tools differ per platform — VB-CABLE, BlackHole, PipeWire)? [Clarity, Ambiguity, Spec §SC-008]
- [ ] CHK160 Is the "default" audio format fallback (when requested format is not accepted) specified uniformly across platforms, or can each platform's default differ? [Clarity, Ambiguity, Spec §Edge Case "Unsupported format"]
- [ ] CHK161 Is the documented platform-minimum version specified (e.g., Ubuntu 22.04+ / macOS 12+ / Windows 10+)? [Clarity, Gap]

## Requirement Consistency

- [ ] CHK162 Is the FR-030 platform requirement ("Linux / macOS / Windows") consistent with the Qt6 version required (is Qt 6.5+ needed for Multimedia parity across platforms, or is 6.2+ sufficient)? [Consistency, Gap]
- [ ] CHK163 Is the "Qt6::Multimedia is the sole audio API" constraint consistent across all three platforms — no platform-specific fallback permitted? [Consistency, Spec §FR-001, §FR-030]
- [ ] CHK164 Does the cross-platform device-name persistence (R5: store by description) remain consistent with platform-specific device-naming conventions (Linux: `card.device`, macOS: framework-provided, Windows: WASAPI-provided)? [Consistency, Research §R5]
- [ ] CHK165 Is the "Mute on PTT" behavior consistent across all three platforms — do all three rig backends (which run on all three platforms) emit `pttStateChanged` with the same semantics? [Consistency, Research §R4]

## Acceptance Criteria Quality

- [ ] CHK166 Can SC-008 ("builds and runs with identical user-facing behavior") be objectively verified — is there a specific cross-platform smoke test procedure documented? [Measurability, Gap, Spec §SC-008]
- [ ] CHK167 Can the CI acceptance criterion be objectively verified — is the GitHub Actions workflow update for the three platforms specified as part of this feature's deliverables? [Measurability, Gap]
- [ ] CHK168 Is there a measurable acceptance criterion for "the feature works on a specific reference hardware configuration per platform" — e.g., "verified on an Elecraft K4 USB audio on Linux / macOS / Windows"? [Measurability, Gap]

## Scenario Coverage — Platform-Specific Paths

- [ ] CHK169 Are requirements specified for the Linux scenario where PipeWire runs AND PulseAudio runs simultaneously (some distros transition states) — which does Qt6::Multimedia select? [Coverage, Gap]
- [ ] CHK170 Are requirements specified for the macOS scenario where the user has granted microphone permission once, but later revokes it via System Settings? [Coverage, Gap]
- [ ] CHK171 Are requirements specified for the Windows scenario where Stereo Mix is available but not recommended — does the decoder surface a warning, hide the device, or allow it with a quality caveat? [Coverage, Gap]
- [ ] CHK172 Are requirements specified for each platform's "default device" concept — does selecting "(default)" produce a durable setting or change across reboots when the OS default changes? [Coverage, Gap]
- [ ] CHK173 Are requirements specified for the Wayland-vs-X11 case on Linux (ContestLogX already has a "Use X11 backend" option per CLAUDE.md) — does Qt6::Multimedia behave identically under both display servers? [Coverage, Gap]

## Edge Case Coverage

- [ ] CHK174 Is the edge case specified for a device whose name contains a backslash, quote, or other character that might be escaped in `QSettings`? [Edge Case, Gap]
- [ ] CHK175 Is the edge case specified for device description length limits (some OSes truncate, some don't)? [Edge Case, Gap]
- [ ] CHK176 Is the edge case specified for running the decoder on a Linux system with no sound server at all (dev machine without audio hardware)? [Edge Case, Gap]
- [ ] CHK177 Is the edge case specified for running the decoder on Windows in a remote-desktop session where audio devices may be RDP-redirected from the client? [Edge Case, Gap]
- [ ] CHK178 Is the edge case specified for macOS Gatekeeper / code-signing interaction with Qt6::Multimedia plugins (unsigned plugins may be blocked in hardened builds)? [Edge Case, Gap]

## Non-Functional / Platform Hygiene

- [ ] CHK179 Are requirements specified that no platform-specific audio code (direct ALSA / CoreAudio / WASAPI calls) is allowed — enforcing the Qt6::Multimedia-only constraint as a code-review gate? [Non-Functional, Spec §FR-015 (spirit), Principle II]
- [ ] CHK180 Are requirements specified for `#ifdef Q_OS_*` discipline — is it required that platform-specific code is gated per CLAUDE.md cross-platform rules, and that such blocks are reviewed? [Non-Functional, CLAUDE.md Cross-Platform Rules]
- [ ] CHK181 Are requirements specified for file-path handling in any decoder-related paths (audio test recordings, debug logs) using `QDir`/`QStandardPaths` per CLAUDE.md? [Non-Functional, CLAUDE.md Cross-Platform Rules]

## Dependencies & Assumptions

- [ ] CHK182 Is the assumption that Qt6::Multimedia's audio-capture API has feature parity across Linux / macOS / Windows (no capability differences that would require platform branches) documented? [Assumption, Research §R1, §R7]
- [ ] CHK183 Is the assumption that the existing CI (per `.github/workflows` and the recent `Update CI action versions` commit) already has build capability for all three target platforms documented? [Assumption, Gap]
- [ ] CHK184 Is the dependency on platform-level audio routing tools (PipeWire module-loopback, BlackHole, VB-CABLE) documented as an OPERATOR concern (not a ContestLogX dependency) in the quickstart? [Dependency, Assumption, Quickstart §4]

## Ambiguities & Conflicts

- [ ] CHK185 The spec says the feature "MUST function on Linux, macOS, and Windows using system audio input devices exposed by the OS" (FR-030) — is "system audio input devices" clear enough to include virtual devices (BlackHole, VB-CABLE) as first-class, or could an implementation reasonably interpret this as real hardware only? [Ambiguity, Spec §FR-030]
- [ ] CHK186 SC-008 says "identical user-facing behavior" but the quickstart documents three different per-platform audio-routing setups — is the "identical" scope limited to the decoder itself, or does it extend to device enumeration labels and the Rig Connection Settings dropdown contents (which will naturally differ per OS)? [Ambiguity, Spec §SC-008, Quickstart §4]
- [ ] CHK187 Is there a potential conflict between "no third-party dependencies" (Principle II) and the operator's need for a virtual-audio-cable tool to route radio audio (VB-CABLE, BlackHole) — is that dependency the operator's responsibility or ContestLogX's? (The quickstart implies operator's, but it's not called out explicitly in the spec.) [Conflict, Ambiguity, Spec §FR-001]

---

## Notes

- Check items off as completed: `[x]`
- Items tagged `[Gap]` indicate a cross-platform requirement currently absent that likely needs spec, plan, or quickstart addition
- Items tagged `[Ambiguity]` indicate a cross-platform requirement present but needing tightening
- Items tagged `[Conflict]` indicate two requirements that appear to compete when read together
- Checklist total: 49 items across 9 categories
