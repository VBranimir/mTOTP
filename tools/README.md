# mTOTP Testing Tool Usage

The `mtotp.py` tool is a **helper and verifier** for the mTOTP algorithm.  
It mirrors the human steps exactly and is intended for testing, learning, and validation.


## Help

For help and additional funcionalities use command:

```
tools/mtotp.py --help
```



---

## Basic Usage

Generate an mTOTP for a given key and planned login time:

```
tools/mtotp.py gen --key <SECRET_KEY> --at <ISO_TIME>
```

### Example

```
tools/mtotp.py gen --key 1234598760 --at 2026-01-17T17:00
```

Output:

```
510769
```

---

## Parameters

### --key

The **secret numeric key** used to derive:
- the S-box
- the merged digits
- the final OTP

Requirements:
- digits only
- expected length: **10 digits**
- must match the key used mentally

Example:

```
--key 1234598760
```

---

### --at

The **planned login time**, in ISO format:

```
YYYY-MM-DDTHH:MM
```

This value is internally converted to the mTOTP time vector:

```
YYMMDDHHMM
```

Example:

```
--at 2026-01-17T17:00
```

---

## What the Tool Does Internally

The tool performs the same steps as the human procedure:

1. Build the time vector (YYMMDDHHMM)
2. Derive the S-box from the key
3. Combine time and key (mod 10)
4. Apply S-box substitution
5. Apply diffusion
6. Fold to 5 digits
7. Compute the final checksum digit

No randomness is used.

---

## Intended Use

- Verify manual calculations
- Learn the algorithm step-by-step
- Debug mistakes in human execution
- Confirm expected OTPs during development

The tool is **not required** for normal use once the human process is learned.

---

## Notes

- The tool assumes exact alignment with the README algorithm.
- If tool output differs from a manual result, the manual steps should be rechecked.
- The tool is deterministic: same key + same time ⇒ same OTP.

---