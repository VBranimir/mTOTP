# mTOTP

# mTOTP – Mental / Human‑Executable One‑Time Password

mTOTP is an experimental, manual variant of TOTP designed to be computed by a human without electronic devices. It explores the limits of time-based authentication under strict human constraints and makes no claims of cryptographic equivalence to standard TOTP.

mTOTP is a **human‑executable OTP scheme** designed to be:
- deterministic
- mentally doable (with practice)
- auditable and explainable
- reproducible by both humans and software

This protocol intentionally allows OTPs to be calculated for future times.
Rather than treating this as a limitation, it makes it a requirement: the user must know when they intend to authenticate, and the verifier checks against that agreed moment. Time is therefore not an approximation, but an explicit part of the protocol - Turning authentication time from reactive to intentional.
This document describes the **exact algorithm used by the tool**, written for humans first.

---

## Overview

An mTOTP is generated from:
- a **secret numeric key**
- a **planned login time**

The algorithm uses:
- a key‑derived digit S‑box
- digitwise modular arithmetic
- a simple diffusion step
- a deterministic fold into a 6‑digit OTP

No randomness is involved during generation.

---

## Example Inputs

- **Secret key (10 digits):** `1234598760`  
  *(If your key is shorter than 10 digits, pad or derive it consistently before use.)*

- **Planned login time:**  
  `2026‑01‑17 17:00`

---

## Step 1 - Build Time Vector

Convert the planned login time into the format, take into account that you are calculating for the server-side set time:

```
YYMMDDHHMM
```

Example:
```
2026‑01‑17 17:00 → 2601171700
```

Result:
```
T = 2601171700
```

---

## Step 2 - Build S‑box from the Secret Key

The S‑box is a **digit substitution table (0–9 → 0–9)** derived **only from the secret key**.
S-box (Substitution Box) is a digit-remapping table that replaces each digit (0–9) with another digit to introduce non-linearity.
It is derived deterministically from the secret key by writing down each digit the first time it appears in the key, then appending any missing digits (0–9) in order; the position is the input digit and the value is the output digit.
### Procedure

1. Read the key **left to right**
2. Write down each digit **the first time it appears**
3. Ignore repeated digits
4. Append any **missing digits (0–9)** at the end, in normal order
5. The **position** is the input digit  
   The **value** is the output digit

### Example

Secret key:
```
1234598760
```

Unique digits in order:
```
1 2 3 4 5 9 8 7 6 0
```

Final S‑box list:
```
1234598760
```

S‑box table:
```
Input :  0 1 2 3 4 5 6 7 8 9
Output:  1 2 3 4 5 9 8 7 6 0
```

---

## Step 3 - Combine Time and Key (mod 10)

Add the time digits and key digits **position‑by‑position**, using mod 10.

```
Time: 2601171700
Key : 1234598760
----------------
Result: 3835669460
```

Call this:
```
C = 3835669460
```

---

## Step 4 - Apply S‑box Substitution

Replace **each digit** of `C` using the S‑box table.

Mapping from Step 2:
```
0→1  1→2  2→3  3→4  4→5
5→9  6→8  7→7  8→6  9→0
```

Apply to:
```
3835669460
```

Result:
```
4649880581
```

---

## Step 5 - Diffusion (Digit Mixing)

Diffusion mixes the digits so each position depends on the previous result: starting with the last digit, each digit is replaced by the sum of itself and the previous output (mod 10).
This ensures that changing a single digit affects all following digits while remaining simple enough to do mentally.

### Rule

1. Set:
```
a = last digit
```

2. For each digit from **left to right**:
```
new_digit = (current_digit + a) mod 10
a = new_digit
```

### Example

Start:
```
4649880581
```

Diffused result:
```
5154200534
```

*(Length is always preserved.)*

---

## Step 6 - Fold to 5 Digits

Pair digits from the **front and back** and add them mod 10 like folded in half:
```
51542
00534
```
or:
```
(c1+c6) (c2+c7) (c3+c8) (c4+c9) (c5+c10)
```

From our last step result:
```
5154200534
```

Calculation:
```
5+0 = 5
1+0 = 1
5+5 = 0
4+3 = 7
2+4 = 6

```

Result:
```
OTP5 = 51076
```

---

## Step 7 - Calculate Final Digit (o6)

Add the **five OTP digits** and take mod 10:

```
(5+1+0+7+6) mod 10 = 19 mod 10 = 9
```

---

## Final mTOTP

```
51076 + 9 → 510769
```

---

## Invariants & Sanity Checks

- S‑box is derived **only from the key**
- Length stays **10 digits** until folding
- S‑box substitution never changes length
- Diffusion always uses the **previous result**
- Final OTP is always **6 digits**

---

## Notes

This algorithm is designed for **human execution first**, with software acting as a helper and verifier.  
Clarity, determinism, and mental tractability are intentional design goals.



## Testing tool usage

Read `tools/README.md`

## PAM plugin - TBD

## Keycloak plugin - TBD