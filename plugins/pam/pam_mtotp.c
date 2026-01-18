/*
 * pam_mtotp.c — PAM module implementing mTOTP v2-min (password + OTP friendly)
 *
 * What it does:
 * - Prompts user for a 6-digit OTP via PAM conversation (keyboard-interactive).
 * - Computes expected OTP from:
 *     - server local time YYMMDDHHMM
 *     - 10-digit shared secret key (digits-only) loaded from a root-only key file
 * - Verifies within an optional +/- minute window.
 *
 * mTOTP v2-min steps implemented:
 *  1) t[10] = YYMMDDHHMM digits from server local time
 *  2) k[10] = key digits from file
 *  3) c[i] = (t[i] + k[i]) mod 10
 *  4) S-box from key digits (unique in order, then missing digits 0..9); c[i]=S(c[i])
 *  5) diffusion: a=c[9]; for i=0..9: c[i]=(c[i]+a) mod 10; a=c[i]
 *  6) fold:
 *     o1=(c1+c6), o2=(c2+c7), o3=(c3+c8), o4=(c4+c9), o5=(c5+c10), o6=sum(c)   all mod 10
 *     OTP=o1..o6
 *
 * Build (Debian):
 *   sudo apt install -y build-essential libpam0g-dev
 *   gcc -fPIC -fno-stack-protector -c pam_mtotp.c -o pam_mtotp.o
 *   gcc -shared -o pam_mtotp.so pam_mtotp.o -lpam
 *   sudo install -m 644 pam_mtotp.so /usr/lib/x86_64-linux-gnu/security/pam_mtotp.so
 *
 * Key file:
 *   sudo install -m 600 -o root -g root /dev/stdin /etc/mtotp.key <<'EOF'
 *   1234598760
 *   EOF
 *
 * PAM config (sshd):
 *   Put ABOVE @include common-auth (so it cannot be skipped):
 *     auth required pam_mtotp.so keyfile=/etc/mtotp.key window=1 prompt=mTOTP:
 *   Then keep password:
 *     @include common-auth
 *
 * Notes:
 * - prompt=... must NOT contain spaces (PAM args are whitespace-separated).
 * - Use keyboard-interactive on the SSH client to see the OTP prompt.
 */

#define _GNU_SOURCE
#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef PAM_EXTERN
#define PAM_EXTERN
#endif

/* ---------- small utilities ---------- */

static int consttime_str_eq6(const char *a, const char *b) {
    /* constant-time compare for 6 chars */
    unsigned char diff = 0;
    for (int i = 0; i < 6; i++) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

static int parse_int_arg(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (!s || *s == '\0' || (end && *end != '\0')) return -1;
    if (v < -1440 || v > 1440) return -1; /* sanity */
    *out = (int)v;
    return 0;
}

static void get_arg_kv(const char *arg, const char **k, const char **v) {
    const char *eq = strchr(arg, '=');
    if (!eq) { *k = arg; *v = ""; return; }
    *k = arg;
    *v = eq + 1;
}

/* ---------- PAM conversation prompt ---------- */

static int pam_prompt_otp(pam_handle_t *pamh, const char *prompt, char **out_resp) {
    const struct pam_conv *conv = NULL;
    int rc = pam_get_item(pamh, PAM_CONV, (const void **)&conv);
    if (rc != PAM_SUCCESS || !conv || !conv->conv) return PAM_CONV_ERR;

    struct pam_message msg;
    const struct pam_message *msgp = &msg;
    struct pam_response *resp = NULL;

    msg.msg_style = PAM_PROMPT_ECHO_OFF;
    msg.msg = prompt;

    rc = conv->conv(1, &msgp, &resp, conv->appdata_ptr);
    if (rc != PAM_SUCCESS || !resp) return PAM_CONV_ERR;

    if (!resp[0].resp) {
        free(resp);
        return PAM_CONV_ERR;
    }

    *out_resp = resp[0].resp; /* ownership transferred to us */
    resp[0].resp = NULL;
    free(resp);
    return PAM_SUCCESS;
}

/* ---------- key + time -> digits ---------- */

static int read_key_digits(const char *path, int k[10]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    char buf[128];
    size_t n = fread(buf, 1, sizeof(buf)-1, f);
    fclose(f);
    buf[n] = '\0';

    /* strip whitespace */
    char digits[32];
    size_t di = 0;
    for (size_t i = 0; i < n && di < sizeof(digits)-1; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') digits[di++] = buf[i];
        else if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' ' || buf[i] == '\t') continue;
        else continue;
    }
    digits[di] = '\0';

    if (di != 10) return -1;
    for (int i = 0; i < 10; i++) k[i] = digits[i] - '0';
    return 0;
}

static void time_digits_local(time_t t, int td[10]) {
    struct tm tmv;
    localtime_r(&t, &tmv);

    /* YYMMDDHHMM */
    int yy = tmv.tm_year % 100;
    int mo = tmv.tm_mon + 1;
    int dd = tmv.tm_mday;
    int hh = tmv.tm_hour;
    int mm = tmv.tm_min;

    td[0] = (yy / 10) % 10;
    td[1] = yy % 10;
    td[2] = (mo / 10) % 10;
    td[3] = mo % 10;
    td[4] = (dd / 10) % 10;
    td[5] = dd % 10;
    td[6] = (hh / 10) % 10;
    td[7] = hh % 10;
    td[8] = (mm / 10) % 10;
    td[9] = mm % 10;
}

/* ---------- mTOTP core ---------- */

static void build_sbox_from_key(const int k[10], int sbox[10]) {
    int seen[10] = {0};
    int p[10];
    int pi = 0;

    for (int i = 0; i < 10; i++) {
        int d = k[i];
        if (!seen[d]) { seen[d] = 1; p[pi++] = d; }
    }
    for (int d = 0; d < 10; d++) if (!seen[d]) p[pi++] = d;

    for (int i = 0; i < 10; i++) sbox[i] = p[i]; /* S(x) = sbox[x] */
}

static void mtotp_compute_6(time_t when, const int k[10], char out6[7]) {
    int t[10], c[10], sbox[10];

    time_digits_local(when, t);
    for (int i = 0; i < 10; i++) c[i] = (t[i] + k[i]) % 10;

    build_sbox_from_key(k, sbox);
    for (int i = 0; i < 10; i++) c[i] = sbox[c[i]];

    int a = c[9];
    for (int i = 0; i < 10; i++) {
        c[i] = (c[i] + a) % 10;
        a = c[i];
    }

    int o1 = (c[0] + c[5]) % 10;
    int o2 = (c[1] + c[6]) % 10;
    int o3 = (c[2] + c[7]) % 10;
    int o4 = (c[3] + c[8]) % 10;
    int o5 = (c[4] + c[9]) % 10;
    int sum = 0; for (int i = 0; i < 10; i++) sum += c[i];
    int o6 = sum % 10;

    out6[0] = (char)('0' + o1);
    out6[1] = (char)('0' + o2);
    out6[2] = (char)('0' + o3);
    out6[3] = (char)('0' + o4);
    out6[4] = (char)('0' + o5);
    out6[5] = (char)('0' + o6);
    out6[6] = '\0';
}

/* ---------- args ---------- */

typedef struct {
    const char *keyfile;
    const char *prompt;
    int window;
} cfg_t;

static void cfg_init(cfg_t *c) {
    c->keyfile = "/etc/mtotp.key";
    c->prompt  = "mTOTP: ";
    c->window  = 1;
}

/* NOTE: prompt MUST be a single token in PAM config (no spaces), unless you do escaping.
 * Keep it simple: prompt=mTOTP:
 */
static void cfg_parse(cfg_t *c, int argc, const char **argv) {
    for (int i = 0; i < argc; i++) {
        const char *k = NULL, *v = NULL;
        get_arg_kv(argv[i], &k, &v);

        if (strncmp(k, "keyfile=", 8) == 0) {
            c->keyfile = v;
        } else if (strncmp(k, "window=", 7) == 0) {
            int w = 0;
            if (parse_int_arg(v, &w) == 0) c->window = w;
        } else if (strncmp(k, "prompt=", 7) == 0) {
            /* If they pass prompt=mTOTP: we append a space for nicer UX */
            c->prompt = v;
        }
    }
}

/* ---------- PAM entrypoints ---------- */

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags,
                                  int argc, const char **argv) {
    (void)flags;

    cfg_t cfg;
    cfg_init(&cfg);
    cfg_parse(&cfg, argc, argv);

    int key[10];
    if (read_key_digits(cfg.keyfile, key) != 0) {
        return PAM_AUTH_ERR;
    }

    /* prompt string: allow prompt=mTOTP: and we add a trailing space if absent */
    char prompt_buf[128];
    const char *p = cfg.prompt ? cfg.prompt : "mTOTP:";
    size_t pn = strlen(p);
    if (pn + 2 >= sizeof(prompt_buf)) pn = sizeof(prompt_buf) - 3;
    memcpy(prompt_buf, p, pn);
    prompt_buf[pn] = '\0';
    if (pn == 0 || prompt_buf[pn-1] != ' ') {
        if (pn + 1 < sizeof(prompt_buf)) {
            prompt_buf[pn] = ' ';
            prompt_buf[pn+1] = '\0';
        }
    }

    char *resp = NULL;
    int rc = pam_prompt_otp(pamh, prompt_buf, &resp);
    if (rc != PAM_SUCCESS || !resp) return PAM_AUTH_ERR;

    /* validate 6 digits */
    size_t rlen = strlen(resp);
    if (rlen != 6) { free(resp); return PAM_AUTH_ERR; }
    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)resp[i])) { free(resp); return PAM_AUTH_ERR; }
    }

    time_t now = time(NULL);
    int w = cfg.window;
    if (w < 0) w = -w;

    char expected[7];
    int ok = 0;
    for (int dm = -w; dm <= w; dm++) {
        time_t t = now + (time_t)dm * 60;
        mtotp_compute_6(t, key, expected);
        if (consttime_str_eq6(resp, expected)) { ok = 1; break; }
    }

    free(resp);

    return ok ? PAM_SUCCESS : PAM_AUTH_ERR;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags,
                             int argc, const char **argv) {
    (void)pamh; (void)flags; (void)argc; (void)argv;
    return PAM_SUCCESS;
}
