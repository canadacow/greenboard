/*
 * hostfs.c -- DOS network redirector TSR (OpenWatcom, 16-bit small model).
 *
 * Hooks INT 2Fh/AH=11h and forwards file operations to the HostFS
 * ISA test card at ports 0xE0-0xE4.  The host side (C++20) does all
 * real filesystem work; this TSR just marshals DOS structures to/from
 * the card's port I/O protocol.
 *
 * DOS structure layouts from EtherDFS (Mateusz Viste, 2017).
 *
 * Build:
 *   set WATCOM=c:\dev\watcom
 *   %WATCOM%\binnt\wcc -bt=dos -ms -0 -s -ox hostfs.c
 *   %WATCOM%\binnt\wlink format dos libpath %WATCOM%\lib286\dos
 *       libpath %WATCOM%\lib286 file hostfs.obj
 */

#include <dos.h>
#include <conio.h>
#include <string.h>
#include <i86.h>
#include "chint.h"    /* _mvchain_intr -- resident-safe chain */

/* All resident code goes to BEGTEXT segment (before _TEXT/transient) */
#pragma code_seg(BEGTEXT, CODE)

/* ---- Card port protocol ---- */
#define PORT_CMD    0xE0
#define PORT_DATA   0xE1
#define PORT_LEN_LO 0xE2
#define PORT_LEN_HI 0xE3
#define PORT_RESET  0xE4

#define CMD_FINDFIRST 0x01
#define CMD_FINDNEXT  0x02
#define CMD_OPEN      0x03
#define CMD_CLOSE     0x04
#define CMD_READ      0x05
#define CMD_WRITE     0x06
#define CMD_GETATTR   0x07
#define CMD_CHDIR     0x08
#define CMD_DISKINFO  0x09
#define CMD_SEEK      0x0A
#define CMD_CREATE    0x0B
#define CMD_MKDIR     0x0C
#define CMD_RMDIR     0x0D
#define CMD_DELETE    0x0E
#define CMD_RENAME    0x0F

/* ================================================================
 * DOS internal structures (from EtherDFS DOSSTRUC.H)
 * ================================================================ */
#pragma pack(1)

/* CDS flags */
#define CDSFLAG_NET 0x8000u
#define CDSFLAG_PHY 0x4000u

struct cdsstruct {
    unsigned char current_path[67];
    unsigned short flags;
    unsigned char far *dpb;
    union {
        struct {
            unsigned short start_cluster;
            unsigned long unknown;
        } LOCAL;
        struct {
            unsigned long redirifs_record_ptr;
            unsigned short parameter;
        } NET;
    } u;
    unsigned short backslash_offset;
    unsigned char f2[7];   /* DOS 4+ extra bytes */
};  /* 88 bytes for DOS 4+; 81 bytes for DOS 3.x (no f2) */

struct sdbstruct {
    unsigned char drv_lett;
    unsigned char srch_tmpl[11];
    unsigned char srch_attr;
    unsigned short dir_entry;
    unsigned short par_clstr;
    unsigned char f1[4];
};

struct foundfilestruct {
    unsigned char fname[11];
    unsigned char fattr;
    unsigned char f1[10];
    unsigned short time_lstupd;
    unsigned short date_lstupd;
    unsigned short start_clstr;
    unsigned long fsize;
};

/* SFT entry -- all DOS versions */
struct sftstruct {
    unsigned int handle_count;
    unsigned int open_mode;
    unsigned char file_attr;
    unsigned int dev_info_word;
    unsigned char far *dev_drvr_ptr;
    unsigned int start_sector;      /* we stash our card handle here */
    unsigned long file_time;        /* date+time packed as dword */
    unsigned long file_size;
    unsigned long file_pos;
    unsigned int rel_sector;
    unsigned int abs_sector;
    unsigned int dir_sector;
    unsigned char dir_entry_no;
    char file_name[11];
};

#pragma pack()

/* ================================================================
 * SDA offsets -- differ by DOS version
 *
 *                         DOS 4+   DOS 3.x
 * DTA ptr                  0Ch      0Ch
 * fn1 (first filename)     9Eh      92h
 * fn2 (second filename)   11Eh     112h
 * SDB (search block)      19Eh     192h
 * Found file              1B3h     1A7h
 * Search attributes       24Dh     23Ah
 * CDS pointer             282h     26Ch
 * ================================================================ */
#define SDA_DTA_OFF         0x0C

static unsigned short sda_fn1_off;
static unsigned short sda_fn2_off;
static unsigned short sda_sdb_off;
static unsigned short sda_found_off;
static unsigned short sda_srchattr_off;
static unsigned short sda_fcbfn1_off;
static unsigned short sda_cds_off;

/* ---- Saved state ---- */
static void (_interrupt far *old_int2f)(void);
static unsigned char our_drive = 4;  /* E: */
static unsigned char far *sda_ptr = 0;
static unsigned char dos_major = 0;
static unsigned char vollabel_pending = 0; /* 1 = FindFirst needs real first entry */

/* ---- Port I/O helpers ---- */

static void card_reset(void)
{
    outp(PORT_RESET, 0);
}

static void card_send_byte(unsigned char b)
{
    outp(PORT_DATA, b);
}

static void card_send_string(const char far *s)
{
    while (*s) {
        outp(PORT_DATA, *s);
        s++;
    }
    outp(PORT_DATA, 0);
}

static void card_send_u16(unsigned short v)
{
    outp(PORT_DATA, v & 0xFF);
    outp(PORT_DATA, (v >> 8) & 0xFF);
}

static unsigned char card_exec(unsigned char cmd)
{
    outp(PORT_CMD, cmd);
    return (unsigned char)inp(PORT_CMD);
}

static unsigned short card_result_len(void)
{
    unsigned short lo = (unsigned char)inp(PORT_LEN_LO);
    unsigned short hi = (unsigned char)inp(PORT_LEN_HI);
    return lo | (hi << 8);
}

static unsigned char card_read_byte(void)
{
    return (unsigned char)inp(PORT_DATA);
}

static unsigned short card_read_u16(void)
{
    unsigned short lo = (unsigned char)inp(PORT_DATA);
    unsigned short hi = (unsigned char)inp(PORT_DATA);
    return lo | (hi << 8);
}

static unsigned long card_read_u32(void)
{
    unsigned long b0 = (unsigned char)inp(PORT_DATA);
    unsigned long b1 = (unsigned char)inp(PORT_DATA);
    unsigned long b2 = (unsigned char)inp(PORT_DATA);
    unsigned long b3 = (unsigned char)inp(PORT_DATA);
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

/* ---- SDA accessors ---- */

static unsigned char far *get_dta(void)
{
    unsigned long addr;
    if (!sda_ptr) return 0;
    addr = *(unsigned long far *)(sda_ptr + SDA_DTA_OFF);
    return (unsigned char far *)addr;
}

static char far *get_fn1(void)
{
    if (!sda_ptr) return 0;
    return (char far *)(sda_ptr + sda_fn1_off);
}

static char far *get_fn2(void)
{
    if (!sda_ptr) return 0;
    return (char far *)(sda_ptr + sda_fn2_off);
}

static struct sdbstruct far *get_sdb(void)
{
    if (!sda_ptr) return 0;
    return (struct sdbstruct far *)(sda_ptr + sda_sdb_off);
}

static struct foundfilestruct far *get_found(void)
{
    if (!sda_ptr) return 0;
    return (struct foundfilestruct far *)(sda_ptr + sda_found_off);
}

/* ---- Drive checks ---- */

static int is_our_drive(const char far *fn)
{
    char dl;
    if (!fn) return 0;
    dl = fn[0];
    if (dl >= 'a' && dl <= 'z') dl -= 32;
    return (dl - 'A') == our_drive;
}

static int is_our_sft(struct sftstruct far *sft)
{
    if (!(sft->dev_info_word & 0x8000)) return 0;
    if ((sft->dev_info_word & 0x1F) != our_drive) return 0;
    return 1;
}

/* ---- Fill SFT from card open/create result ---- */
/* Card returns: handle(2), attr(1), time(2), date(2), size(4) */
static void fill_sft(struct sftstruct far *sft, const char far *fn)
{
    const char far *p;
    const char far *last;
    unsigned short h, t, d;
    unsigned long sz;
    int i;

    h  = card_read_u16();
    sft->file_attr     = card_read_byte();
    t = card_read_u16();
    d = card_read_u16();
    sft->file_time     = ((unsigned long)d << 16) | t;
    sz = card_read_u32();
    sft->file_size     = sz;
    sft->dev_info_word = 0x8040 | our_drive;
    sft->dev_drvr_ptr  = 0;
    sft->start_sector  = h;   /* stash card handle */
    sft->file_pos      = 0;
    sft->open_mode     &= 0xFF00u;  /* preserve high byte (FCB flag) */
    sft->open_mode     |= 0x02;     /* read/write */
    sft->rel_sector    = 0xFFFF;
    sft->abs_sector    = 0xFFFF;
    sft->dir_sector    = 0;
    sft->dir_entry_no  = 0xFF;

    /* FCB-format name from last path component */
    fmemset_(sft->file_name, ' ', 11);
    last = fn;
    for (p = fn; *p; p++) {
        if (*p == '\\' || *p == '/') last = p + 1;
    }
    for (i = 0; i < 8 && last[i] && last[i] != '.'; i++)
        sft->file_name[i] = last[i];
    p = last;
    while (*p && *p != '.') p++;
    if (*p == '.') {
        p++;
        for (i = 0; i < 3 && *p; i++)
            sft->file_name[8 + i] = *p++;
    }
}

/* far memcpy -- resident safe, no libc dependency */
static void copybytes(void far *d, void far *s, unsigned short len)
{
    unsigned char far *dp = (unsigned char far *)d;
    unsigned char far *sp = (unsigned char far *)s;
    while (len--) *dp++ = *sp++;
}

/* far memset -- resident safe replacement for _fmemset */
static void fmemset_(void far *d, unsigned char val, unsigned short len)
{
    unsigned char far *dp = (unsigned char far *)d;
    while (len--) *dp++ = val;
}

/* Convert ASCIIZ "FILE.EXT" to FCB format "FILE    EXT" (11 bytes) */
static void name_to_fcb(char far *fcb, const char *name)
{
    int i;
    const char *dot;

    fmemset_(fcb, ' ', 11);
    for (i = 0; i < 8 && name[i] && name[i] != '.'; i++)
        fcb[i] = name[i];
    dot = name;
    while (*dot && *dot != '.') dot++;
    if (*dot == '.') {
        dot++;
        for (i = 0; i < 3 && *dot; i++)
            fcb[8 + i] = *dot++;
    }
}

/* ---- Fill SDA found_file + DTA from card findfirst/findnext result ---- */
/* Card returns: attr(1), time(2), date(2), size(4), ASCIIZ name */
/* Per EtherDFS: we must fill BOTH the SDA found_file AND the DTA.
 * DTA layout:
 *   +00h: SDB (21 bytes) -- search state
 *   +15h: found_file (32 bytes) -- copy of SDA found_file
 */
static void fill_found(struct sdbstruct far *dta,
                       struct foundfilestruct far *sda_ff,
                       unsigned char subfn)
{
    int i;
    static char name_buf[13]; /* must be static: SS != DS in ISR context */
    char c;

    /* Read card result */
    sda_ff->fattr = card_read_byte();
    sda_ff->time_lstupd = card_read_u16();
    sda_ff->date_lstupd = card_read_u16();
    sda_ff->start_clstr = 0;
    sda_ff->fsize = card_read_u32();

    /* Read ASCIIZ filename */
    for (i = 0; i < 12; i++) {
        c = (char)card_read_byte();
        name_buf[i] = c;
        if (c == 0) break;
    }
    name_buf[12] = 0;

    /* FCB-format name into SDA found_file */
    name_to_fcb(sda_ff->fname, name_buf);
    fmemset_(sda_ff->f1, 0, 10);

    /* Advance DTA dir entry counter */
    dta->dir_entry++;

    /* Copy found_file (32 bytes) into DTA+0x15 */
    copybytes((unsigned char far *)dta + 0x15, sda_ff, 32);
}

/* Synthesize a volume label entry into found_file + DTA */
static void fill_vollabel(struct sdbstruct far *dta,
                          struct foundfilestruct far *sda_ff)
{
    /* "HOSTFS     " in FCB format (8+3, space-padded) */
    static char vollabel[11] = {'H','O','S','T','F','S',' ',' ',' ',' ',' '};
    copybytes(sda_ff->fname, vollabel, 11);
    sda_ff->fattr = 0x08;       /* volume label attribute */
    fmemset_(sda_ff->f1, 0, 10);
    sda_ff->time_lstupd = 0;
    sda_ff->date_lstupd = 0;
    sda_ff->start_clstr = 0;
    sda_ff->fsize = 0;
    dta->dir_entry++;
    copybytes((unsigned char far *)dta + 0x15, sda_ff, 32);
}


/* ================================================================
 * INT 2Fh handler
 *
 * On redirector calls (AH=11h):
 *   ES:DI -> SFT (for file ops)
 *   SS = DOS DS (SDA accessible)
 *   SDA fn1 -> fully qualified filename
 *   CX = byte count (read/write)
 * Return: CF clear = success, CF set = error (AX = error code)
 * ================================================================ */
static void _interrupt far int2f_handler(union INTPACK r)
{
    unsigned char subfn;
    char far *fn1;
    char far *fn2;
    struct sftstruct far *sft;
    struct sdbstruct far *sdb;
    struct foundfilestruct far *ff;
    unsigned char far *dta;
    unsigned char status;
    unsigned short count, got, i;
    unsigned char reqdrv;

    if (r.h.ah != 0x11) {
        _mvchain_intr((void far *)old_int2f);
        return;
    }

    subfn = r.h.al;

    /* Chain unsupported or install-check immediately */
    if (subfn == 0x00 || subfn > 0x2E) {
        _mvchain_intr((void far *)old_int2f);
        return;
    }

    /* ---- Centralized drive determination (per EtherDFS) ----
     * Determine the drive BEFORE touching any DOS state.
     * Chain immediately if not our drive. */
    sft = (struct sftstruct far *)MK_FP(r.w.es, r.w.di);
    fn1 = get_fn1();

    if ((subfn >= 0x06 && subfn <= 0x0B) || subfn == 0x21) {
        /* SFT-based ops: close, commit, read, write, lock, unlock, seek */
        reqdrv = sft->dev_info_word & 0x3F;
    } else {
        switch (subfn) {
        case 0x1C: /* FindNext: drive from DTA at ES:DI */
            sdb = (struct sdbstruct far *)MK_FP(r.w.es, r.w.di);
            reqdrv = sdb->drv_lett & 0x1F;
            break;
        case 0x01: case 0x03: case 0x05:  /* rmdir, mkdir, chdir */
        case 0x0E: case 0x0F:             /* set/get attr */
        case 0x11: case 0x13:             /* rename, delete */
        case 0x16: case 0x17:             /* open, create */
        case 0x1B:                        /* findfirst */
        case 0x2E:                        /* spopnfil (DOS 4+ open) */
            /* fn1-based: drive letter is first char */
            {
                char dl;
                if (!fn1) goto chain;
                dl = fn1[0];
                if (dl >= 'a' && dl <= 'z') dl -= 32;
                reqdrv = dl - 'A';
            }
            break;
        case 0x0C: /* disk info: CDS at ES:DI */
            {
                struct cdsstruct far *dcds =
                    (struct cdsstruct far *)MK_FP(r.w.es, r.w.di);
                char dl = dcds->current_path[0];
                if (dl >= 'a' && dl <= 'z') dl -= 32;
                reqdrv = dl - 'A';
            }
            break;
        case 0x1D: /* Close all files -- always ours (no-op) */
            reqdrv = our_drive;
            break;
        default:
            goto chain;
        }
    }

    /* Not our drive? Chain to previous handler with registers UNTOUCHED */
    if (reqdrv != our_drive) goto chain;

    /* ---- From here on, it IS our drive. Safe to modify DOS state. ---- */

    /* Normalize fcb_fn1 from fn1 (EtherDFS lines 1034-1059).
     * Only after confirming it's our drive. */
    if (subfn != 0x0C && fn1) {
        unsigned char far *fcb = sda_ptr + sda_fcbfn1_off;
        const char far *p = fn1;
        const char far *last = fn1;
        int j;
        while (*p) {
            if (*p == '\\') last = p + 1;
            p++;
        }
        fmemset_(fcb, ' ', 11);
        for (j = 0; *last && *last != '.'; last++) {
            if (j < 8) fcb[j++] = *last;
        }
        if (*last == '.') {
            last++;
            for (j = 0; *last; last++) {
                if (j < 3) fcb[8 + j++] = *last;
            }
        }
    }

    /* Default: success (AX=0, CF clear). Failures override. */
    r.w.ax = 0;
    r.w.flags &= ~1u;

    switch (subfn) {

    /* ---- Directory ops ---- */

    case 0x01:  /* Rmdir */
        card_reset();
        card_send_string(fn1);
        if (card_exec(CMD_RMDIR) != 1) goto err;
        return;

    case 0x03:  /* Mkdir */
        card_reset();
        card_send_string(fn1);
        if (card_exec(CMD_MKDIR) != 1) goto err;
        return;

    case 0x05:  /* ChDir */
        card_reset();
        card_send_string(fn1);
        if (card_exec(CMD_CHDIR) != 1) goto err;
        return;

    /* ---- File ops ---- */

    case 0x16:  /* Open existing file */
    case 0x2E:  /* SPOPNFIL (DOS 4+ multipurpose open) */
        card_reset();
        card_send_string(fn1);
        if (card_exec(CMD_OPEN) != 1) goto err;
        fill_sft(sft, fn1);
        if (subfn == 0x2E) r.w.cx = 1; /* action: file opened */
        return;

    case 0x17:  /* Create/truncate file */
        card_reset();
        card_send_string(fn1);
        if (card_exec(CMD_CREATE) != 1) goto err;
        fill_sft(sft, fn1);
        return;

    case 0x06:  /* Close */
        if (sft->handle_count > 0) sft->handle_count--;
        card_reset();
        card_send_u16(sft->start_sector);
        card_exec(CMD_CLOSE);
        return;

    case 0x07:  /* Commit/flush */
        return;

    case 0x08:  /* Read */
        count = r.w.cx;
        card_reset();
        card_send_u16(sft->start_sector);
        card_send_u16(count);
        status = card_exec(CMD_READ);
        if (status != 1) {
            r.w.cx = 0;
            return;
        }
        got = card_result_len();
        if (got > count) got = count;
        dta = get_dta();
        for (i = 0; i < got; i++)
            dta[i] = card_read_byte();
        sft->file_pos += got;
        r.w.cx = got;
        return;

    case 0x09:  /* Write */
        count = r.w.cx;
        card_reset();
        card_send_u16(sft->start_sector);
        dta = get_dta();
        for (i = 0; i < count; i++)
            card_send_byte(dta[i]);
        status = card_exec(CMD_WRITE);
        if (status != 1) goto err;
        got = card_read_u16();
        sft->file_pos += got;
        if (sft->file_pos > sft->file_size)
            sft->file_size = sft->file_pos;
        r.w.cx = got;
        return;

    case 0x21:  /* Seek from end */
        card_reset();
        card_send_u16(sft->start_sector);
        card_send_u16(r.w.dx);   /* offset low */
        card_send_u16(r.w.cx);   /* offset high */
        card_send_byte(2);       /* SEEK_END */
        status = card_exec(CMD_SEEK);
        if (status != 1) goto err;
        sft->file_pos = card_read_u32();
        r.w.ax = (unsigned short)(sft->file_pos & 0xFFFF);
        r.w.dx = (unsigned short)(sft->file_pos >> 16);
        return;

    /* ---- Attributes ---- */

    case 0x0E:  /* Set attributes -- accept silently */
        return;

    case 0x0F:  /* Get attributes + size */
        card_reset();
        card_send_string(fn1);
        status = card_exec(CMD_GETATTR);
        if (status != 1) goto err;
        {
            unsigned char attr;
            unsigned long sz;
            unsigned short date, time;
            attr = card_read_byte();
            sz   = card_read_u32();
            date = card_read_u16();
            time = card_read_u16();
            r.w.ax = attr;
            r.w.bx = (unsigned short)(sz >> 16);
            r.w.di = (unsigned short)(sz & 0xFFFF);
            r.w.cx = time;
            r.w.dx = date;
        }
        return;

    /* ---- Search ---- */

    case 0x1B:  /* FindFirst */
        ff  = get_found();
        if (!ff) goto err;
        dta = get_dta();
        sdb = (struct sdbstruct far *)dta;
        card_reset();
        card_send_string(fn1);
        status = card_exec(CMD_FINDFIRST);
        if (status != 1) { r.w.ax = 2; r.w.flags |= 1u; return; }
        sdb->drv_lett = 0x80 | our_drive;
        copybytes(sdb->srch_tmpl, sda_ptr + sda_fcbfn1_off, 11);
        sdb->srch_attr = *(sda_ptr + sda_srchattr_off);
        sdb->dir_entry = 0;
        sdb->par_clstr = 0;
        fmemset_(sdb->f1, 0, 4);
        /* If search includes volume label bit, return synthetic label first.
         * The real first directory entry is deferred to FindNext. */
        if (sdb->srch_attr & 0x08) {
            vollabel_pending = 1;
            fill_vollabel(sdb, ff);
        } else {
            vollabel_pending = 0;
            fill_found(sdb, ff, 0x1B);
        }
        return;

    case 0x1C:  /* FindNext */
        /* sdb already set during drive determination above */
        ff = get_found();
        if (!ff) goto err;
        /* If we returned a volume label for FindFirst, the card already
         * has the first real result buffered -- just read it now. */
        if (vollabel_pending) {
            vollabel_pending = 0;
            fill_found(sdb, ff, 0x1C);
            return;
        }
        card_reset();
        status = card_exec(CMD_FINDNEXT);
        if (status != 1) { r.w.ax = 18; r.w.flags |= 1u; return; }
        fill_found(sdb, ff, 0x1C);
        return;

    /* ---- Misc ---- */

    case 0x0C:  /* Get disk info */
        card_reset();
        status = card_exec(CMD_DISKINFO);
        if (status != 1) goto err;
        r.w.ax = card_read_u16();   /* sectors per cluster */
        r.w.bx = card_read_u16();   /* total clusters */
        r.w.cx = card_read_u16();   /* bytes per sector */
        r.w.dx = card_read_u16();   /* free clusters */
        return;

    case 0x11:  /* Rename */
        fn2 = get_fn2();
        card_reset();
        card_send_string(fn1);
        card_send_string(fn2);
        if (card_exec(CMD_RENAME) != 1) goto err;
        return;

    case 0x13:  /* Delete */
        card_reset();
        card_send_string(fn1);
        if (card_exec(CMD_DELETE) != 1) goto err;
        return;

    case 0x1D:  /* Close all files for process */
        return;

    default:
        break;
    }

    /* Should not reach here (drive was ours but unhandled subfn) -- chain */
chain:
    _mvchain_intr((void far *)old_int2f);
    return;

err:
    r.w.ax = 0x0005;   /* access denied */
    r.w.flags |= 1u;   /* CF */
}

/* ================================================================
 * CDS registration -- mark drive as network in DOS CDS array.
 *
 * INT 21h/AH=52h -> ES:BX = List of Lists
 * LoL+0x16 = far ptr to CDS array
 * CDS entry size: 81 bytes (DOS 3.x) or 88 bytes (DOS 4+)
 * ================================================================ */

static struct cdsstruct far *getcds(unsigned char drive)
{
    /* Following EtherDFS approach: inline asm for INT 21h/52h because
     * int86x can mangle segment registers. */
    static unsigned char far *dir = 0;
    static unsigned char lastdrv = 0;
    static int inited = 0;
    unsigned short cds_entry_size;

    if (!inited) {
        inited = 1;
        _asm {
            push si
            mov ah, 52h
            int 21h
            /* lastdrv at LoL+21h (DOS 3.1+) */
            mov si, 21h
            mov ah, byte ptr es:[bx+si]
            mov lastdrv, ah
            /* CDS array pointer at LoL+16h (DOS 3.1+) */
            mov si, 16h
            les bx, es:[bx+si]
            mov word ptr dir+2, es
            mov word ptr dir, bx
            pop si
        }
        if (dir == (unsigned char far *)-1L) dir = 0;
    }

    if (!dir) return 0;
    if (drive >= lastdrv) return 0;

    /* DOS 3.x: 0x51 bytes per CDS entry.  DOS 4+: 0x58 */
    cds_entry_size = (dos_major >= 4) ? 0x58 : 0x51;
    return (struct cdsstruct far *)((unsigned char far *)dir +
            (unsigned long)drive * cds_entry_size);
}

static int register_drive(unsigned char drive)
{
    struct cdsstruct far *cds;

    cds = getcds(drive);
    if (!cds) return 0;

    /* Check drive not already in use */
    if (cds->flags != 0) return 0;

    fmemset_(cds->current_path, 0, 67);
    cds->current_path[0] = 'A' + drive;
    cds->current_path[1] = ':';
    cds->current_path[2] = '\\';
    cds->current_path[3] = 0;

    /* Must set BOTH net + physical flags (MS-DOS ignores without PHY) */
    cds->flags = CDSFLAG_NET | CDSFLAG_PHY;
    cds->backslash_offset = 2;

    return 1;
}

/* ============= END OF RESIDENT CODE ============= */
#pragma code_seg("_TEXT", "CODE")

/* This marks the boundary -- everything after here is transient */
static void _resident_end(void) {}

/* ================================================================
 * Install (transient -- freed after TSR)
 * ================================================================ */
void main(void)
{
    union REGS regs;
    struct SREGS sregs;

    cputs("HostFS redirector v0.6\r\n");

    /* ---- Probe card before doing anything ---- */
    {
        unsigned char probe;
        card_reset();
        probe = card_exec(CMD_DISKINFO);
        if (probe != 1) {
            cputs("ERROR: HostFS card not responding on port 0xE0\r\n");
            cputs("  card_reset -> OUT 0xE4, 0\r\n");
            cputs("  card_exec  -> OUT 0xE0, 0x09; IN 0xE0 = 0x");
            {
                char hex[3];
                hex[0] = "0123456789ABCDEF"[(probe >> 4) & 0xF];
                hex[1] = "0123456789ABCDEF"[probe & 0xF];
                hex[2] = 0;
                cputs(hex);
            }
            cputs("\r\n");
            return;
        }
        cputs("Card probe OK (DiskInfo returned status=1)\r\n");
    }

    /* Get DOS version */
    regs.h.ah = 0x30;
    int86(0x21, &regs, &regs);
    dos_major = regs.h.al;
    if (dos_major < 3) {
        cputs("ERROR: DOS 3.1+ required\r\n");
        return;
    }

    /* Set SDA offsets based on DOS version */
    if (dos_major >= 4) {
        sda_fn1_off      = 0x9E;
        sda_fn2_off      = 0x11E;
        sda_sdb_off      = 0x19E;
        sda_found_off    = 0x1B3;
        sda_fcbfn1_off   = 0x22B;
        sda_srchattr_off = 0x24D;
        sda_cds_off      = 0x282;
    } else {
        sda_fn1_off      = 0x92;
        sda_fn2_off      = 0x112;
        sda_sdb_off      = 0x192;
        sda_found_off    = 0x1A7;
        sda_fcbfn1_off   = 0x218;
        sda_srchattr_off = 0x23A;
        sda_cds_off      = 0x26C;
    }

    /* Get SDA address: INT 21h/AX=5D06h */
    regs.x.ax = 0x5D06;
    int86x(0x21, &regs, &regs, &sregs);
    sda_ptr = (unsigned char far *)MK_FP(sregs.ds, regs.x.si);

    /* Register drive E: in DOS CDS */
    if (!register_drive(our_drive)) {
        cputs("Failed to register drive.\r\n");
        return;
    }

    /* Save old INT 2Fh */
    regs.h.ah = 0x35;
    regs.h.al = 0x2F;
    int86x(0x21, &regs, &regs, &sregs);
    old_int2f = (void (_interrupt far *)())MK_FP(sregs.es, regs.x.bx);

    /* Install new INT 2Fh */
    regs.h.ah = 0x25;
    regs.h.al = 0x2F;
    sregs.ds = FP_SEG((void far *)int2f_handler);
    regs.x.dx = FP_OFF((void far *)int2f_handler);
    int86x(0x21, &regs, &regs, &sregs);

    cputs("Drive E: -> HostFS (port 0xE0)\r\n");

    /* TSR: keep BEGTEXT segment (all resident code + data).
     * _resident_end is the first function in _TEXT, so everything
     * before it is resident. */
    {
        unsigned short resident_paras;
        resident_paras = (FP_OFF(_resident_end) + 0x10F) >> 4;
        _dos_keep(0, resident_paras);
    }
}
