# The Target-First Philosophy

## Why Most Security Tools Get It Wrong

Every major pentest tool - from Kali's menu system to smartphone apps to even Bruce itself - follows the same pattern:

1. Present a menu of attack categories (WiFi, BLE, RF...)
2. User picks a category
3. Present a menu of attacks (Deauth, Beacon, Evil Twin...)
4. User picks an attack
5. Tool scans for viable targets
6. User picks a target
7. Attack executes

**This is backwards.**

The user must decide what they want to do *before* they know what's possible. They commit to an attack type, then discover whether any valid targets exist.

The result? Wasted time. Failed attacks. "No clients connected" errors. Confusion.

---

## The Assessor's Inversion

We flip the entire model:

1. Device boots → **Auto-scan begins immediately**
2. All targets appear → **User sees reality**
3. User taps a target → **Context-aware actions appear**
4. User picks an action → **It works, guaranteed**

The difference is subtle but profound:

| Traditional | Target-First |
|-------------|--------------|
| "I want to deauth" → find something | "What's around me?" → what can I do to it? |
| Attack-centric | Target-centric |
| User drives | Environment drives |
| Menu hierarchy | Flat target list |
| Failures possible | Only valid options shown |

---

## The Grammar of Hacking

Think of it linguistically:

- **Traditional:** Verb first, then noun
  - *"Deauth [something]"*
  - *"Beacon flood [something]"*

- **Target-First:** Noun first, then verb
  - *"This network → [deauth it / clone it / monitor it]"*
  - *"This device → [spam it / track it]"*

In natural language, we usually identify the subject before the action. "I want to open *the door*" not "I want to open, and the subject is a door."

The Assessor speaks naturally.

---

## Context-Aware Actions

The killer feature: **actions that can't work are hidden.**

### Example: Deauthentication

Deauth attacks disconnect clients from an access point. If there are no connected clients, the attack does nothing.

**Traditional tool:**
- Shows "Deauth" option always
- User selects it
- Scan happens
- User picks target
- Attack runs
- "Error: No clients connected"
- User's time wasted

**The Assessor:**
- Scans first
- Counts clients per AP
- If `clientCount == 0`: Deauth button hidden
- If `clientCount > 0`: Deauth button shown
- User taps Deauth
- Attack works

No error messages. No wasted attempts. If you see it, it works.

---

## Signal-Based Prioritization

Not all targets are equal. A network at -40dBm is practically sitting on your lap. A network at -85dBm is barely detectable.

The Assessor sorts by signal strength:
- **Strongest targets first** - most likely to succeed
- **Weakest targets last** - fade into the list
- **Visual indicators** - ● strong, ◐ medium, ○ weak, ◦ barely there

This isn't just convenience. Strong signals mean:
- Higher success rate for attacks
- Faster data capture
- More reliable deauth
- Better handshake quality

---

## The State Table

Internally, The Assessor maintains a "State Table" - a real-time snapshot of the RF environment:

```
┌─────────────────────────────────────────────────────────────────┐
│ BSSID             │ SSID           │ RSSI │ Clients │ Security │
├───────────────────┼────────────────┼──────┼─────────┼──────────┤
│ AA:BB:CC:DD:EE:FF │ HomeNetwork    │ -42  │ 3       │ WPA2-PSK │
│ 11:22:33:44:55:66 │ GuestWiFi      │ -58  │ 0       │ OPEN     │
│ DE:AD:BE:EF:CA:FE │ [Hidden]       │ -71  │ 1       │ WPA3     │
└─────────────────────────────────────────────────────────────────┘
```

When you tap a target, the Assessor Engine queries this table and asks:
- "What type is it?" (AP, Station, BLE device)
- "What security?" (Open, WPA2, WPA3)
- "Any clients?" (for deauth eligibility)
- "Have we seen handshakes?" (for further action)

The answers determine which actions appear.

---

## Implementation Principles

### 1. Reality Before Interface
The scan completes before the menu appears. Never show UI for non-existent targets.

### 2. No Dead Ends
Every tap leads somewhere useful. If an action can't work, it doesn't exist.

### 3. Signal Truth
RSSI values are shown raw. No "bars" abstraction. Hackers need precision.

### 4. Minimal Hierarchy
Two levels maximum: Target List → Target Detail. No deeper menus.

### 5. Instant Feedback
Attack starts? Show progress. Attack fails? Show why. Never leave the user guessing.

---

## Why This Matters

The Assessor isn't just a reskin of Bruce. It's a fundamental rethinking of how security tools should work.

When your mental model matches the tool's model, you move faster. You make fewer mistakes. You understand what's happening.

**Target-first isn't just an interface choice. It's a philosophy.**

Know your target first. Then act.

---

<p align="center"><i>"The environment is the menu."</i></p>
