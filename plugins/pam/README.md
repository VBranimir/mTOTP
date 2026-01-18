# pam_mtotp

A PAM module that adds an extra one time password (OTP) prompt to SSH logins using the mTOTP v2-min protocol (digits only, server local time).

Status
- Experimental.
- Tested as working in a Debian VM with OpenSSH and PAM.
- Not security audited. Treat as a prototype until reviewed.

What it does
- During SSH login, prompts for a 6 digit mTOTP code.
- Then continues with the normal system authentication (for example, the usual Unix password), so the effective flow is password plus OTP.

Safety note
- Strongly recommended to test in a VM or a disposable server snapshot first.
- PAM mistakes can lock you out. Keep a console or snapshot rollback path.

## Requirements

- OpenSSH with PAM enabled (UsePAM yes).
- SSH keyboard-interactive enabled (so a PAM module can prompt).
- Build dependencies:
  - Debian: build-essential, libpam0g-dev
  - Fedora: gcc, make, pam-devel, glibc-devel

## Build

### Debian build (example for amd64)

Install build deps:

```bash
sudo apt update
sudo apt install -y build-essential libpam0g-dev
```

Build:

```bash
gcc -fPIC -c pam_mtotp.c -o pam_mtotp.o
gcc -shared -o pam_mtotp.so pam_mtotp.o -lpam
```

### Fedora build

Install build deps:

```bash
sudo dnf install -y gcc make pam-devel glibc-devel
```

Build:

```bash
gcc -fPIC -c pam_mtotp.c -o pam_mtotp.o
gcc -shared -o pam_mtotp.so pam_mtotp.o -lpam
```

## Install

### Debian install path

```bash
sudo install -m 644 pam_mtotp.so /usr/lib/x86_64-linux-gnu/security/pam_mtotp.so
```

### Fedora install path

```bash
sudo install -m 644 pam_mtotp.so /usr/lib64/security/pam_mtotp.so
```

Verify the module links correctly:

```bash
ldd /usr/lib/x86_64-linux-gnu/security/pam_mtotp.so 2>/dev/null || true
ldd /usr/lib64/security/pam_mtotp.so 2>/dev/null || true
```

## Configure the shared secret key

Create a root-only key file with exactly 10 digits:

```bash
sudo install -m 600 -o root -g root /dev/stdin /etc/mtotp.key <<'EOF'
1234598760
EOF
```

The module reads this file at authentication time.

## Integrate with SSH (password plus OTP)

1) Ensure sshd allows PAM prompts

Edit `/etc/ssh/sshd_config` and ensure these are set:

```conf
UsePAM yes
KbdInteractiveAuthentication yes
```

For testing, also ensure password auth is enabled:

```conf
PasswordAuthentication yes
```

Restart sshd:
- Debian:

```bash
sudo systemctl restart ssh
```

- Fedora:

```bash
sudo systemctl restart sshd
```

2) Add the PAM line for sshd

Edit `/etc/pam.d/sshd`.

Add the mTOTP line above the default password stack so it cannot be skipped.

For Debian (uses @include common-auth):

```pam
@include common-auth
auth  required  pam_mtotp.so keyfile=/etc/mtotp.key window=1 prompt=mTOTP:
```

For Fedora (usually uses substack password-auth):

```pam
auth  substack  password-auth
auth  required  pam_mtotp.so keyfile=/etc/mtotp.key window=1 prompt=mTOTP:
```

Notes:
- prompt= must not contain spaces because PAM arguments are whitespace-separated.
- window=1 means accept current minute plus or minus 1 minute (3-minute effective window). Use window=0 for strict 1-minute validity.

Restart sshd again after editing PAM:
- Debian:

```bash
sudo systemctl restart ssh
```

- Fedora:

```bash
sudo systemctl restart sshd
```

## Test

From a client, force keyboard-interactive and password so you see both prompts:

```bash
ssh -o PubkeyAuthentication=no USER@HOST
```

Expected interaction:
- First prompt: mTOTP:
- Second prompt: Password:

If you want to generate expected codes for testing, use the helper tool from this repo (if included) or compute manually per the mTOTP v2-min spec. Make sure you use the server timezone and the correct minute.

## VM-first workflow recommendation

- Install Debian or Fedora in a VM.
- Enable SSH.
- Create a snapshot.
- Integrate PAM changes.
- Keep a VM console open for recovery.

## Troubleshooting

No mTOTP prompt appears
- Confirm sshd supports keyboard-interactive:
  - `sshd -T | grep -i kbdinteractiveauthentication`
- Confirm your PAM line is above the default auth stack.
- Use the SSH test command that forces keyboard-interactive.

Login fails even with correct code
- Check the server time and timezone.
- Increase window temporarily, for example window=2.
- Confirm `/etc/mtotp.key` is exactly 10 digits.

Recover from lockout
- Use the VM console or recovery access.
- Remove or comment the pam_mtotp.so line in `/etc/pam.d/sshd`.
- Restart sshd.

## Module options

- keyfile=/path/to/key (default /etc/mtotp.key)
- window=N (default 1)
- prompt=TEXT (default mTOTP:)

## Publishing note

This module is experimental but tested as working. If you use it outside a VM, use snapshots and a rollback plan.