/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Dungeon Tracker: SONG SLOTS. SAVE > PROJECT keeps eleven songs, A..K (1.2: eight, A..H), each whole: the project (the sounds, the
 * mix, the globals, SONG's arrangement, the current pattern's steps) and all six patterns of the PATTERN BANK with
 * which one was current. Loading one brings the whole song back. Included by project.c (RONDO_LAYOUT, FELUCCA_FLASH).
 *
 * The flash: twelve extents of three 4 KiB sectors (12 KiB), eleven in use at most (one per slot) and one free, so a
 * save never writes over the copy it replaces:
 *   extents 0..5  0xB4000..0xC5FFF   user sample slot 2's flash (USR2, retired: eng_sample.c)
 *   extents 6, 7  0x97000..0x9CFFF   the sectors of 1.1's four project objects (OBJ_PROJECT0..3)
 *   extent 8      0x9D000, 0x9E000 (theirs too), 0xC6000 (USR2's)
 *   extents 9..11 0xC7000..0xCFFFF   USR2's last sector and USR3's 32 KiB (1.3: USR3 retired too; its top 48 KiB
 *                                    are the PATTERN BANK's, as before)
 * 1.1's projects move into extents 0..5 at the first power-on (sg_boot); extents 6.. are used once they have.
 * An extent: a 36-byte record (sg_hdr_t) at its start, the body from byte 256 on, across its sectors. The body is the
 * project's 3840 bytes, then each stored pattern's (in key order), each encoded on its own (sg_enc: runs of a byte, or
 * up to 128 bytes as they are), a pattern's steps field by field (all 64 notes, then all 64 velocities, ..), and each
 * pattern after the first as its difference from the one before: a song's six patterns, the power-on song's in every
 * key, take about 1 KiB; six different busy ones about 3 KiB. A save: the free extent erased, the body
 * written, the record last (its CRC over itself; the body's CRC in it). The newest record of a slot wins (seq); a
 * power cut mid-save leaves the old one. An erase writes an empty record (plen 0): the slot's newest, empty.
 * A song too big for 12 032 bytes (six patterns of busy random steps) is refused, nothing written: SONG TOO BIG.
 * A record with no patterns (used 0: a project from before 1.2, or the editor's backup) loads as a project always
 * did: the current pattern's steps only, the bank's other keys kept. */
#define SG_NSLOT 11u                            /* A..K (1.2: A..H) */
#define SG_NEXT 12u
#define SG_SECS 3u
#define SG_BODY 256u
#define SG_CAP (SG_SECS * ST_SECTOR - SG_BODY)
#define SG_MAGIC 0x47535444u                     /* "DTSG" */
#define SG_VER 1u
#define SG_NONE 0xFFu
#define SG_PSTEP (NSTEP * (uint32_t)sizeof(step_t))   /* a track's steps, field by field */
#define SG_PRAW (NTRK * SG_PSTEP + (uint32_t)sizeof(((chain_pattern_t *)0)->timing) + NTRK + (uint32_t)sizeof(motion_store_t))
#define SG_PATS_NONE 0u                          /* sg_put's patterns: none, the bank's, those stored */
#define SG_PATS_LIVE 1u
#define SG_PATS_KEEP 2u
static const uint32_t SG_SEC[SG_NEXT][SG_SECS] = {
    {0xB4000u, 0xB5000u, 0xB6000u}, {0xB7000u, 0xB8000u, 0xB9000u}, {0xBA000u, 0xBB000u, 0xBC000u},
    {0xBD000u, 0xBE000u, 0xBF000u}, {0xC0000u, 0xC1000u, 0xC2000u}, {0xC3000u, 0xC4000u, 0xC5000u},
    {0x97000u, 0x98000u, 0x99000u}, {0x9A000u, 0x9B000u, 0x9C000u}, {0x9D000u, 0x9E000u, 0xC6000u},
    {0xC7000u, 0xC8000u, 0xC9000u}, {0xCA000u, 0xCB000u, 0xCC000u}, {0xCD000u, 0xCE000u, 0xCF000u}};   /* (1.3: USR3's) */
#define SG_LEGACY_FROM 6u                        /* extents 6.. share the old project objects' sectors */

typedef struct {
    uint32_t magic;
    uint8_t slot, ver, used, cur;                /* used: bit p, pattern p stored (0: none); cur: the current one */
    uint32_t seq;
    uint16_t plen, rsv;                          /* the project's length (0: an erased slot) */
    uint32_t pcut, blen, bcrc;                   /* the project's encoded length; the body's length and CRC */
    uint32_t rsv2;
    uint32_t hcrc;
} sg_hdr_t;
_Static_assert(sizeof(sg_hdr_t) == 36u, "song record layout");

static struct {
    uint8_t ext[SG_NSLOT];                       /* the extent holding each slot's newest record, SG_NONE none */
    uint8_t used[SG_NSLOT];                      /* .. its patterns (0 none) */
    uint8_t legacy;                              /* the old project objects still there: extents 6.. not free */
    uint8_t ready;                               /* (sg_scan done: sg_on) */
    uint32_t seq;                                /* the newest record's */
    uint8_t buf[256];                            /* (the main loop's: reads, writes) */
} sg __attribute__((section(".pool")));
static uint32_t sg_addr(uint32_t e, uint32_t l) { return SG_SEC[e][l >> 12] + (l & (ST_SECTOR - 1u)); }

static int sg_head(uint32_t e, sg_hdr_t *h)
{
    if (st_read(SG_SEC[e][0], h, sizeof *h))
        return -1;
    if (h->magic != SG_MAGIC || h->ver != SG_VER || h->slot >= SG_NSLOT || h->blen > SG_CAP ||
        (h->plen && (h->plen > PROJ_STORE_SIZE || h->pcut > h->blen)) || (h->used & ~0x3Fu) || h->cur >= NBANK ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}
static int sg_body_ok(uint32_t e, const sg_hdr_t *h)   /* the body's CRC */
{
    uint32_t c = 0xFFFFFFFFu, l = 0;
    while (l < h->blen) {
        uint32_t n = h->blen - l > sizeof sg.buf ? sizeof sg.buf : h->blen - l;
        if (st_read(sg_addr(e, SG_BODY + l), sg.buf, n))
            return 0;
        c = st_crc32_add(c, sg.buf, n);
        l += n;
    }
    return ~c == h->bcrc;
}
/* every extent's record read: each slot's newest (its body checked) */
static void sg_scan(void)
{
    uint32_t e, s, best[SG_NSLOT];
    sg_hdr_t h;
    memset(sg.ext, SG_NONE, sizeof sg.ext);
    memset(sg.used, 0, sizeof sg.used);
    sg.seq = 0;
    for (e = 0; e < SG_NEXT; e++) {
        if (sg_head(e, &h) || !sg_body_ok(e, &h))
            continue;
        if (h.seq - sg.seq < 0x80000000u)
            sg.seq = h.seq;
        s = h.slot;
        if (sg.ext[s] == SG_NONE || h.seq - best[s] < 0x80000000u) {
            sg.ext[s] = (uint8_t)e;
            sg.used[s] = h.used;
            best[s] = h.seq;
        }
    }
    sg.ready = 1;
    sg.legacy = 0;
    for (s = 0; s < 4u; s++) {
        st_hdr_t o;
        if (!st_head(OBJ_PROJECT0 + s, 0, &o) || !st_head(OBJ_PROJECT0 + s, 1, &o))
            sg.legacy = 1;
    }
}
static void sg_on(void) { if (!sg.ready) sg_scan(); }
static int sg_has(uint32_t slot)
{
    sg_hdr_t h;
    sg_on();
    slot %= SG_NSLOT;
    return sg.ext[slot] != SG_NONE && !sg_head(sg.ext[slot], &h) && h.plen;
}
static int sg_free_ext(void)                     /* an extent no slot's newest record is in */
{
    uint32_t e, s;
    for (e = 0; e < SG_NEXT; e++) {
        if (sg.legacy && e >= SG_LEGACY_FROM)
            break;
        for (s = 0; s < SG_NSLOT && sg.ext[s] != e; s++)
            ;
        if (s == SG_NSLOT)
            return (int)e;
    }
    return -1;
}

/* ---- the body: segments of raw bytes, each encoded on its own. A token t: t < 128, the next t + 1 bytes as they
 * are; t >= 128, the next byte t - 125 times (3..130). A pattern after the first is stored as its difference from the
 * one before it (XOR, byte by byte): a key copied from another and changed a little is mostly runs of 0 */
typedef struct { const uint8_t *proj; uint32_t plen; const chain_pattern_t *pat, *ref; } sg_src_t;
static uint8_t sg_pat_byte(const chain_pattern_t *c, uint32_t j)   /* a pattern's j-th raw byte (SG_PRAW of them) */
{
    if (j < NTRK * SG_PSTEP) {
        uint32_t k = j / SG_PSTEP, r = j % SG_PSTEP;
        return ((const uint8_t *)&c->step[k][r % NSTEP])[r / NSTEP];   /* field r / NSTEP of step r % NSTEP */
    }
    j -= NTRK * SG_PSTEP;
    if (j < sizeof c->timing)
        return ((const uint8_t *)c->timing)[j];
    j -= (uint32_t)sizeof c->timing;
    if (j < NTRK)
        return c->qnt[j];
    return ((const uint8_t *)&c->motion)[j - NTRK];
}
static uint8_t *sg_pat_at(chain_pattern_t *c, uint32_t j)
{
    if (j < NTRK * SG_PSTEP) {
        uint32_t k = j / SG_PSTEP, r = j % SG_PSTEP;
        return &((uint8_t *)&c->step[k][r % NSTEP])[r / NSTEP];
    }
    j -= NTRK * SG_PSTEP;
    if (j < sizeof c->timing)
        return &((uint8_t *)c->timing)[j];
    j -= (uint32_t)sizeof c->timing;
    if (j < NTRK)
        return &c->qnt[j];
    return &((uint8_t *)&c->motion)[j - NTRK];
}
static uint8_t sg_src_byte(const sg_src_t *s, uint32_t i)
{
    return s->pat ? (uint8_t)(sg_pat_byte(s->pat, i) ^ (s->ref ? sg_pat_byte(s->ref, i) : 0u)) : s->proj[i];
}

/* the writer: the encoded bytes into extent e from body offset w (e < 0: only counted); the body's CRC as it goes */
static struct { int e; uint32_t w, crc, fill; int err; } sgw;
static void sgw_flush(void)
{
    if (sgw.fill && sgw.e >= 0 && !sgw.err) {
        uint32_t l = SG_BODY + sgw.w - sgw.fill;          /* (256-byte pages: never across a sector) */
        if (st_prog(sg_addr((uint32_t)sgw.e, l), sg.buf, sgw.fill))
            sgw.err = 1;
    }
    sgw.fill = 0;
}
static void sgw_byte(uint8_t b)
{
    if (sgw.w >= SG_CAP) {
        sgw.err = 2;                                     /* (too big) */
        return;
    }
    sgw.crc = st_crc32_add(sgw.crc, &b, 1);
    sgw.w++;
    if (sgw.e >= 0) {
        sg.buf[sgw.fill++] = b;
        if (sgw.fill == sizeof sg.buf)
            sgw_flush();
    }
}
static void sg_enc(const sg_src_t *s, uint32_t n)
{
    uint32_t i = 0;
    while (i < n && !sgw.err) {
        uint32_t r = 1, st, l;
        uint8_t b = sg_src_byte(s, i);
        while (i + r < n && r < 130u && sg_src_byte(s, i + r) == b)
            r++;
        if (r >= 3u) {
            sgw_byte((uint8_t)(125u + r));
            sgw_byte(b);
            i += r;
            continue;
        }
        st = i;
        l = 0;
        while (i < n && l < 128u) {                     /* literals, up to the next run of 3 */
            uint8_t c = sg_src_byte(s, i);
            if (i + 2u < n && sg_src_byte(s, i + 1u) == c && sg_src_byte(s, i + 2u) == c)
                break;
            i++;
            l++;
        }
        sgw_byte((uint8_t)(l - 1u));
        for (; st < i; st++)
            sgw_byte(sg_src_byte(s, st));
    }
}
/* the reader: extent e's body from offset `at`, decoded into n bytes of a segment (out: project bytes, or pattern c,
 * stored as its difference from ref when ref is not 0) */
static int sg_dec(uint32_t e, uint32_t *at, uint32_t end, uint8_t *out, chain_pattern_t *c, const chain_pattern_t *ref,
                  uint32_t n)
{
    uint32_t o = 0, have = 0, pos = 0;
    uint8_t t, v;
#define SG_GET(x) do { if (pos >= have) { uint32_t q = end - *at > sizeof sg.buf ? sizeof sg.buf : end - *at; \
        if (!q || st_read(sg_addr(e, SG_BODY + *at), sg.buf, q)) return -1; have = q; pos = 0; } \
        (x) = sg.buf[pos++]; (*at)++; } while (0)
    while (o < n) {
        SG_GET(t);
        if (t >= 128u) {
            uint32_t r = t - 125u;
            SG_GET(v);
            if (o + r > n)
                return -1;
            while (r--) { if (out) out[o] = v; else *sg_pat_at(c, o) = (uint8_t)(v ^ (ref ? sg_pat_byte(ref, o) : 0u)); o++; }
        } else {
            uint32_t r = t + 1u;
            if (o + r > n)
                return -1;
            while (r--) { SG_GET(v); if (out) out[o] = v; else *sg_pat_at(c, o) = (uint8_t)(v ^ (ref ? sg_pat_byte(ref, o) : 0u)); o++; }
        }
    }
#undef SG_GET
    return 0;
}

/* slot <- the project (plen bytes; 0: erase) and its patterns: SG_PATS_LIVE the bank's (bank_keep first: the caller),
 * SG_PATS_KEEP those stored with the slot now, SG_PATS_NONE none. 0 done; -9 too big (nothing written); else failed */
static int sg_put(uint32_t slot, const void *proj, uint32_t plen, uint32_t mode)
{
    sg_hdr_t h, old;
    uint32_t p, pass, l, oe = SG_NONE;
    int e;
    sg_src_t s;
    sg_on();
    slot %= SG_NSLOT;
    if (mode == SG_PATS_KEEP && (sg.ext[slot] == SG_NONE || sg_head(sg.ext[slot], &old) || !old.used))
        mode = SG_PATS_NONE;
    if (mode == SG_PATS_KEEP)
        oe = sg.ext[slot];
    e = sg_free_ext();
    if (e < 0)
        return -1;
    memset(&h, 0, sizeof h);
    h.magic = SG_MAGIC;
    h.slot = (uint8_t)slot;
    h.ver = SG_VER;
    h.plen = (uint16_t)plen;
    h.used = !plen ? 0u : mode == SG_PATS_LIVE ? bank.used : mode == SG_PATS_KEEP ? old.used : 0u;
    h.cur = !plen ? 0u : mode == SG_PATS_LIVE ? bank.cur : mode == SG_PATS_KEEP ? old.cur : 0u;
    for (pass = 0; pass < 2u; pass++) {                 /* 0: counted (too big: nothing erased), 1: written */
        memset(&sgw, 0, sizeof sgw);
        sgw.e = pass ? e : -1;
        sgw.crc = 0xFFFFFFFFu;
        if (pass)
            for (l = 0; l < SG_SECS; l++)
                if (st_erase(SG_SEC[e][l]))
                    return -1;
        if (plen) {
            s.proj = (const uint8_t *)proj; s.plen = plen; s.pat = s.ref = 0;
            sg_enc(&s, plen);
            h.pcut = sgw.w;
        }
        if (mode == SG_PATS_LIVE) {
            const chain_pattern_t *ref = 0;
            for (p = 0; p < NBANK; p++)
                if ((h.used >> p) & 1u) {
                    s.proj = 0; s.plen = 0; s.pat = &bank_pat[p]; s.ref = ref;
                    sg_enc(&s, SG_PRAW);
                    ref = &bank_pat[p];
                }
        } else if (mode == SG_PATS_KEEP) {                /* the stored patterns, as they are encoded */
            uint32_t at = old.pcut;
            while (at < old.blen && !sgw.err) {
                uint32_t n = old.blen - at > 64u ? 64u : old.blen - at, i;
                uint8_t tmp[64];
                if (st_read(sg_addr(oe, SG_BODY + at), tmp, n))
                    return -1;
                for (i = 0; i < n; i++)
                    sgw_byte(tmp[i]);
                at += n;
            }
        }
        sgw_flush();
        if (sgw.err == 2)
            return -9;
        if (sgw.err)
            return -1;
    }
    h.blen = sgw.w;
    h.bcrc = ~sgw.crc;
    h.seq = sg.seq + 1u;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if (st_prog(SG_SEC[e][0], &h, sizeof h))             /* the record, last */
        return -1;
    if (sg_head((uint32_t)e, &old) || memcmp(&old, &h, sizeof h) || !sg_body_ok((uint32_t)e, &h))
        return -7;                                      /* (read back: a failing part must not say SAVED) */
    sg.seq = h.seq;
    sg.ext[slot] = (uint8_t)e;
    sg.used[slot] = h.used;
    return 0;
}
/* slot's project -> dst (max bytes): its length, -1 none (empty, erased, unreadable) */
static int sg_get_project(uint32_t slot, void *dst, uint32_t max)
{
    sg_hdr_t h;
    uint32_t at = 0;
    sg_on();
    slot %= SG_NSLOT;
    if (sg.ext[slot] == SG_NONE || sg_head(sg.ext[slot], &h) || !h.plen || h.plen > max)
        return -1;
    if (sg_dec(sg.ext[slot], &at, h.pcut, (uint8_t *)dst, 0, 0, h.plen) || at != h.pcut)
        return -1;
    return h.plen;
}
/* slot's patterns -> the bank (every key: the stored ones, the others empty), its current one current; the tracks
 * are the caller's (the project's steps: the current pattern as saved). 1 done, 0 the slot has none (the bank as
 * it was), -1 unreadable (the bank as it was) */
static int sg_get_patterns(uint32_t slot)
{
    sg_hdr_t h;
    uint32_t p, at;
    sg_on();
    slot %= SG_NSLOT;
    if (sg.ext[slot] == SG_NONE || sg_head(sg.ext[slot], &h) || !h.plen || !h.used)
        return 0;
    if (!sg_body_ok(sg.ext[slot], &h))
        return -1;
    at = h.pcut;
    {
        const chain_pattern_t *ref = 0;
        for (p = 0; p < NBANK; p++)                     /* (the body checked: these decode) */
            if ((h.used >> p) & 1u) {
                if (sg_dec(sg.ext[slot], &at, h.blen, 0, &bank_pat[p], ref, SG_PRAW))
                    return -1;
                ref = &bank_pat[p];
            }
        for (p = 0; p < NBANK; p++)                     /* (bounded after: the next one's reference as stored) */
            if ((h.used >> p) & 1u)
                bank_bound(&bank_pat[p]);
    }
    fm1_irq_off();
    bank.used = h.used;
    bank.cur = h.cur;
    bank.used |= (uint8_t)(1u << bank.cur);
    bank.queue = CHAIN_OFF;
    bank.dirty = (uint8_t)((1u << NBANK) - 1u);        /* (bank_poll: each key's flash copy rewritten, or emptied) */
    bank.gen++;
    fm1_irq_on();
    return 1;
}

/* power-on: the slots read; the projects of 1.1 (A..D in Felucca's project objects) moved into song slots A..D once,
 * without patterns (they load as they did), then their sectors erased (extents 6.. use them). A power cut meanwhile:
 * the next power-on moves those not moved yet */
static void sg_boot(void)
{
    uint32_t s, ok = 1;
    sg_scan();
    if (!sg.legacy)
        return;
    for (s = 0; s < 4u; s++) {
        int n;
        if (sg.ext[s] != SG_NONE)                       /* (moved already) */
            continue;
        proj_wire_gen++;
        n = st_load(OBJ_PROJECT0 + s, &proj_wire, sizeof proj_wire);
        if (n > 0 && proj_import(&proj_scratch, &proj_wire, n)) {   /* (any older format: as FUN10, bounded) */
            proj_bound(&proj_scratch);
            if (proj_pack(&proj_wire, &proj_scratch))
                n = (int)sizeof proj_wire;
        }
        if (n > 0 && sg_put(s, &proj_wire, (uint32_t)n, SG_PATS_NONE))
            ok = 0;
    }
    if (!ok)
        return;                                         /* (the old ones stay, and their sectors: next time) */
    for (s = 0; s < 8u; s++)
        (void)st_erase(0x97000u + s * ST_SECTOR);
    sg_scan();
}
