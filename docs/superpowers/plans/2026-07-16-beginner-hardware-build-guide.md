# Beginner Hardware Build Guide Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current technician-oriented hardware guide with a complete zero-experience, illustrated, offline HTML + Markdown manual for assembling the first-tank ESP32-S3 dual-DS18B20 low-voltage monitor.

**Architecture:** Keep `HANDOVER_HARDWARE_20260716.md` and `docs/hardware-installation.md` as the authoritative hardware contract and concise installation reference. Rebuild `docs/hardware-build-guide.md` as the complete instructional source, then mirror it into a standalone HTML document with embedded CSS/SVG diagrams and independently test both files against deterministic beginner-safety contracts.

**Tech Stack:** Markdown, standalone HTML5, embedded CSS/SVG, POSIX shell checks, Node.js static HTML checks, Python `html.parser`, official manufacturer documentation, verified YouTube links.

## Global Constraints

- Audience: a reader with no electronics assembly experience who does not remember how to use a multimeter.
- Hardware contract: Waveshare `ESP32-S3-DEV-KIT-N16R8-M`, DFRobot `KIT0021`, MB-102, one external 4.7kOhm pull-up, WAGO `221-413`, RVV three-core 0.3mm2, `2.0USB-C` panel data lead, ANENG `616`, and Pro'sKit `8PK-3001D`.
- Wiring contract: both installed probes use three-wire non-parasitic power; VCC to `3V3`, GND to `GND`, DATA to `GPIO4`; only one external 4.7kOhm resistor from DATA to 3V3.
- DFRobot DFR0055 adapters and their pull-up jumpers are not used in the WAGO prototype.
- No relay, SSR, heater control, pump control, power-strip wiring, mains measurement, or any other 220V operation.
- Exact MB-102 hole positions, ANENG 616 dial positions, probe batch wiring and enclosure cutout locations must be calibrated from user-supplied photos before being represented as one-to-one physical locations.
- Existing documentation may use SVG role colors, but every line must also be labeled `VCC`, `GND`, or `DATA`; role colors must not be presented as guaranteed probe wire colors.
- Repository rules prohibit Git write operations; do not add, commit, switch, push, merge, rebase, stash, reset, or run any other state-changing Git command.

---

### Task 1: Lock the Beginner-Manual Contract Before Rewriting

**Files:**
- Modify: `test/verify_docs.sh`
- Verify: `docs/superpowers/specs/2026-07-16-hardware-build-guide-design.md`

**Interfaces:**
- Consumes: the approved zero-experience design specification.
- Produces: a deterministic documentation check that fails against the current short guide and prevents future regressions to technician shorthand.

- [x] **Step 1: Add per-file beginner markers**

For both `docs/hardware-build-guide.md` and `docs/hardware-build-guide.html`, require these literal concepts to appear independently:

```text
黑表笔
COM
USB 已拔掉
立即停止
Type A
Type B
A-VCC
A-GND
A-DATA
总-VCC
总-GND
总-DATA
第十个 WAGO
15 分钟
120 分钟
48–72 小时
```

- [x] **Step 2: Add beginner-safety rejection checks**

Reject the phrases `按常规接线`, `自行选择一个 GPIO`, and `万用表测量市电` from both guide files. Require at least five direct `https://www.youtube.com/watch?` links in each guide and require every guide to state that videos are supplementary rather than execution authority.

- [x] **Step 3: Run the documentation check and observe the expected failure**

Run:

```sh
sh test/verify_docs.sh
```

Expected: non-zero exit with the first missing beginner marker, proving that the current short guide does not satisfy the new specification.

### Task 2: Verify Product Facts, Controls and Learning Sources

**Files:**
- Modify later: `docs/hardware-build-guide.md`
- Modify later: `docs/hardware-build-guide.html`

**Interfaces:**
- Consumes: current manufacturer pages and exact purchased model names.
- Produces: a source ledger used by Tasks 3 and 4; it does not change the hardware contract.

- [x] **Step 1: Verify the Waveshare board labels**

Open the official Waveshare product page and schematic for `ESP32-S3-DEV-KIT-N16R8-M`. Confirm that `3V3`, `GND`, and GPIO4 are exposed and that USB-C supports power/programming. Do not infer exact breadboard row numbers without the user's board and MB-102 photos.

- [x] **Step 2: Verify KIT0021 variants and included adapter**

Open the official DFRobot KIT0021 product page. Record Type A as red VCC / black GND / yellow DATA and Type B as red VCC / yellow GND / green DATA. Confirm the kit includes DFR0198, DFR0055 and FIT0011, and preserve the decision not to combine DFR0055 pull-up jumpers with the single external 4.7kOhm resistor.

- [x] **Step 3: Verify WAGO handling and conductor limits**

Open WAGO's official 221 handling page and 221-413 product page. Record the official approximately 11mm strip length, lever-up / insert / lever-down sequence and fine-stranded conductor range. Keep the no-power trial-clamp stop condition for the unknown probe inner-conductor area.

- [x] **Step 4: Verify the ANENG 616 physical control labels**

Search for an official or seller-supplied ANENG 616 manual matching the purchased unit. Do not write a one-to-one dial diagram unless the control layout can be matched to the user's front photo. Until then, use an explicit photo-confirmation gate while still teaching black lead to `COM`, continuity practice and low-voltage DC measurement concepts.

- [x] **Step 5: Curate and open video candidates**

Select at least one direct YouTube video for each topic: solderless breadboard, digital multimeter continuity/low-voltage DC, WAGO 221 handling, ESP32 connection/programming, and DS18B20 three-wire/OneWire wiring. Open every selected URL, record the exact title/channel, and reject videos that require mains work, parasite-power wiring, 5V DATA, a different pull-up topology, or unexplained GPIO substitutions.

- [x] **Step 6: Record project-specific differences beside every video**

For each accepted video, state which segment/topic is useful and which details must not be copied. The manual must override videos with `GPIO4`, `3V3`, the KIT0021 Type A/Type B branch, the single external 4.7kOhm pull-up, and the no-mains boundary.

### Task 3: Rewrite the Markdown Manual as a Zero-Experience Procedure

**Files:**
- Modify: `docs/hardware-build-guide.md`
- Reference: `HANDOVER_HARDWARE_20260716.md`
- Reference: `docs/superpowers/specs/2026-07-16-hardware-build-guide-design.md`

**Interfaces:**
- Consumes: the fact and video ledger from Task 2.
- Produces: the authoritative instructional source copied into the HTML presentation in Task 4.

- [x] **Step 1: Replace the opening and safety model**

Begin with an explicit novice promise, the meaning of “断电 = 拔掉 ESP32 的 USB-C”, a dry-worktable checklist, and three repeated callouts: “可以继续”, “USB 已拔掉”, and “立即停止并拍照”. State that the manual never asks the reader to measure or modify 220V.

- [x] **Step 2: Add one identification card per purchased part**

Add separate recognition sections for the ESP32-S3 board, MB-102, KIT0021 probe and its unused accessories, 4.7kOhm resistor, male/male and male/female Dupont ends, WAGO 221-413, RVV cable, USB cables, panel lead, M12/PG7, ABS boxes, ANENG 616, 8PK-3001D, heat-shrink tubing, cable-tie bases and step drill. Each card must say the quantity, visual cues, current use and common mix-up.

- [x] **Step 3: Teach breadboard connectivity before using it**

Explain A-E and F-J groups, the centre trench and potentially split power rails. Include a no-power continuity exercise: touch meter probes together, test a Dupont wire, test two holes in one five-hole group, and test opposite sides of the centre trench. For each test give the expected beep/no-beep result and a stop rule.

- [x] **Step 4: Teach only the ANENG 616 functions used here**

Explain black lead to `COM`, the correct voltage/resistance/continuity jack after photo/manual confirmation, power-off, continuity/beeper and low-voltage DC. Include “never insert the red lead into the current jack for this project” and “never touch wall outlets, power strips, heaters or any 220V equipment.”

- [x] **Step 5: Expand board power-up and breadboard insertion**

Split board-only USB power-up, port detection, unplugging, board orientation, insertion across the MB-102 centre trench and a pre-wiring photo checkpoint into individual actions. Do not assign final row/column numbers before photos are available.

- [x] **Step 6: Expand the three-node and pull-up build**

Define the physical male/female Dupont ends. Add one action each for the 3V3 lead, GND lead, GPIO4/DATA lead and 4.7kOhm resistor. State that the resistor has no polarity. Add a pre-power short-circuit check and explicit continue/stop results.

- [x] **Step 7: Add the Type A/Type B one-probe branch**

Present two mutually exclusive wiring tables. Require the reader to identify their batch before selecting the table. Add one action per probe conductor, a pre-power photo checkpoint, first power-up, expected single-ROM output and troubleshooting stop conditions.

- [x] **Step 8: Add the second probe, ROM and timed records**

Add the second probe only with USB unplugged, repeat the selected line-role table, state “do not add another resistor”, explain ROM as a sensor identity, and include fillable 15-minute calibration plus 0/15/30/60/120-minute dry-run tables.

- [x] **Step 9: Add WAGO/RVV practice and numbered assembly**

Start with RVV scrap. Teach 8PK-3001D trial stripping and WAGO's strip/lever/insert/close/inspect/pull sequence. Number A-VCC, A-GND, A-DATA, B-VCC, B-GND, B-DATA, total-VCC, total-GND and total-DATA; state that the tenth WAGO is spare. For each WAGO list conductor 1, conductor 2 and conductor 3 or “leave empty”.

- [x] **Step 10: Add enclosure, irreversible-drilling and tank stages**

Require dry layout, user-photo approval before marking/drilling, eye protection, an empty box, incremental step-drill testing, USB panel orientation, cable strain relief and correct/incorrect drip loops. Finish with main-tank/sump placement and a fillable 48–72-hour observation table.

- [x] **Step 11: Add the curated video appendix**

Include at least five verified direct YouTube links, covering all five required learning topics. Beside each link state the useful topic, recommended section to watch and project-specific differences. State that the manual—not the video—owns the final wiring contract.

### Task 4: Rebuild the Offline HTML with Beginner Visuals

**Files:**
- Modify: `docs/hardware-build-guide.html`
- Consume: `docs/hardware-build-guide.md`

**Interfaces:**
- Consumes: the complete Markdown source from Task 3.
- Produces: a standalone desktop-readable guide with embedded diagrams and the same safety/wiring semantics.

- [x] **Step 1: Build persistent navigation and callout styles**

Create a desktop sidebar or sticky chapter navigation, plus visually distinct styles for “USB 已拔掉”, “可以插 USB”, “正确现象”, “错误现象”, “立即停止并拍照”, “实物照片确认后填写” and completion checkboxes. Keep all CSS inline.

- [x] **Step 2: Add part-identification SVG cards**

Draw recognizable but schematic SVGs for the ESP32 board, MB-102, resistor, Dupont ends, KIT0021 probe, WAGO, RVV, meter, stripper, panel lead, gland and box. Every image must have adjacent text and accessible `aria-label` content.

- [x] **Step 3: Add breadboard and meter lesson diagrams**

Draw connected five-hole groups, disconnected centre-trench groups and potentially split rails. Draw a generic ANENG 616 front panel labelled as “waiting for real-photo alignment” until the real dial layout is confirmed; show `COM`, the intended low-voltage jack and forbidden current/mains regions textually.

- [x] **Step 4: Add board and single-probe sequence diagrams**

Show board-only USB power, board across the centre trench, the 3V3/GND/GPIO4 nodes, one non-polarised 4.7kOhm resistor and the two mutually exclusive KIT0021 line variants. Avoid fake MB-102 row numbers before photo calibration.

- [x] **Step 5: Add dual-probe, WAGO and enclosure diagrams**

Draw dual-probe parallel wiring with one pull-up, all nine labelled WAGO nodes, per-port conductor labels, dry-box layout, USB-C panel orientation, strain relief, correct/incorrect drip loops and main-tank/sump probe positions.

- [x] **Step 6: Mirror records, stops and videos**

Copy the Markdown calibration/dry-run/soak tables, photo checkpoints and video appendix without changing the hardware semantics. Direct links must use visible titles and channels, not generic “click here” labels.

### Task 5: Validate Content, Links and Visual Layout

**Files:**
- Verify: `docs/hardware-build-guide.md`
- Verify: `docs/hardware-build-guide.html`
- Verify: `test/verify_docs.sh`

**Interfaces:**
- Consumes: the completed Markdown and HTML guides.
- Produces: fresh evidence for content consistency, standalone HTML structure, link availability and desktop/narrow-window readability.

- [x] **Step 1: Run the deterministic documentation contract**

Run:

```sh
sh test/verify_docs.sh
```

Expected: `documentation checks passed`.

- [x] **Step 2: Check standalone HTML structure and resources**

Run this Node static check for the standalone HTML contract:

```sh
node -e "const fs=require('fs');const s=fs.readFileSync('docs/hardware-build-guide.html','utf8');for(const x of ['<!doctype html>','</html>','<svg','</svg>','黑表笔','COM','USB 已拔掉','立即停止','A-VCC','总-DATA','48–72 小时'])if(!s.includes(x))throw new Error('missing '+x);if(/<(script|link)\\b/i.test(s))throw new Error('external runtime tag found');console.log('HTML standalone check passed')"
```

Run this Python nesting check for unclosed or misnested tags:

```sh
python3 - <<'PY'
from html.parser import HTMLParser
from pathlib import Path
VOID={'meta','link','img','br','hr','input','source','area','base','col','embed','param','track','wbr'}
class Checker(HTMLParser):
    def __init__(self): super().__init__(); self.tags=[]
    def handle_starttag(self, tag, attrs):
        if tag not in VOID: self.tags.append(tag)
    def handle_endtag(self, tag):
        if not self.tags or self.tags[-1] != tag: raise ValueError(f'unexpected </{tag}>')
        self.tags.pop()
p=Checker(); p.feed(Path('docs/hardware-build-guide.html').read_text())
if p.tags: raise ValueError('unclosed tags: '+str(p.tags))
print('HTML nesting check passed')
PY
```

Expected: both commands exit 0 and print their pass messages.

- [x] **Step 3: Verify selected YouTube URLs**

Open every direct video URL again and confirm the page title/channel still matches the manual. If YouTube blocks automated fetching, preserve the exact URLs but mark link availability as requiring manual browser confirmation rather than claiming it passed.

- [x] **Step 4: Render at desktop and narrow widths**

Open `docs/hardware-build-guide.html` at approximately 1440px and 760px widths. Check navigation, callouts, tables, SVG labels, Chinese text, horizontal diagram scrollers and print layout. Record any renderer dependency that prevents screenshot-based review.

- [x] **Step 5: Perform line-by-line hardware-contract review**

Confirm both files independently contain: GPIO4, 3V3, GND, one external 4.7kOhm resistor, Type A/Type B branches, DFR0055 exclusion, nine named WAGOs plus one spare, no wet splice, no 220V measurement/control, 15-minute calibration, 120-minute dry run and 48–72-hour observation.

- [x] **Step 6: Report the photo-gated limitations**

Do not call final row/column pin positions, meter dial positions or enclosure cutout locations complete until the required user photos have been received and incorporated. Hand off the usable draft with an explicit list of remaining photo confirmations.
