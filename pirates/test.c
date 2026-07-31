/* test.c -- Pirates! second-stage loader, from the trace of 0023C-00395.
 *
 * Reconstruction of the boot sector's payload as period C (Microsoft C 5.x /
 * Turbo C, small model, 8088 real mode).  This is what the assembly does, not
 * what the original source looked like -- the original was almost certainly
 * hand-written assembly, and several things here have no C spelling and are
 * left as inline asm or noted in comments.
 *
 * Physical layout at entry:
 *   CS:IP  = 0020:001C  (the boot sector far-jumped here)
 *   The loader's own variables live in the first bytes of segment 0020:
 *     [1]  BYTE  current track / cylinder counter
 *     [2]  WORD  destination segment for the next 8K chunk
 *     [4]  WORD  cleared at init, unused in this path
 *     [6]  BYTE  last track to load (0x15 = 21)
 *     [2c] 11    the relocated INT 1E floppy parameter table
 */

typedef unsigned char  BYTE;
typedef unsigned int   WORD;

/* The loader's data, at the base of its own segment.  In the assembly these
 * are absolute offsets with DS = 0x20; a real build would place them with a
 * linker script or an ORG. */
struct loader_vars {
    BYTE  pad0;            /* 0x00 */
    BYTE  track;           /* 0x01  cylinder counter, 1..[limit] */
    WORD  dest_seg;        /* 0x02  where the next chunk lands */
    WORD  unused;          /* 0x04  zeroed at init */
    BYTE  last_track;      /* 0x06  0x15 */
};

#define VARS  ((struct loader_vars far *) 0x00200000L)

#define DBT_COPY_SEG   0x0020    /* our copy of the floppy parameter table */
#define DBT_COPY_OFF   0x002c
#define SCRATCH_SEG    0x2d80    /* 16K staging buffer for each track pair */
#define CGA_SEG        0xb800
#define FIRST_DEST_SEG 0x0050    /* the game image starts here */
#define LAST_TRACK     0x15

/* ---------------------------------------------------------------- BIOS ---
 * int 13h AH=02: read sectors.
 *   AL = count, CH = cylinder, CL = sector, DH = head, DL = drive,
 *   ES:BX = buffer.  Carry set on error.
 * Returns 0 on success.
 *
 * Note the drive number is never set anywhere in the traced code: DL still
 * holds whatever the BIOS left in it at boot, which is the boot drive.  That
 * is deliberate and load-bearing, so this takes no drive argument.
 */
static int disk_read(WORD seg, WORD off, BYTE count,
                     BYTE cylinder, BYTE sector, BYTE head)
{
    int failed;

    _asm {
        mov     ax, seg
        mov     es, ax
        mov     bx, off
        mov     ah, 2
        mov     al, count
        mov     ch, cylinder
        mov     cl, sector
        mov     dh, head
        int     13h
        sbb     ax, ax          /* CF -> 0 or -1 */
        mov     failed, ax
    }
    return failed;
}

/* --------------------------------------------------- 1. floppy table ----
 * Vector 1Eh does not point at code -- it points at the disk base table the
 * BIOS reads on every access.  Copying it into our own segment and re-
 * pointing the vector lets the loader use a table it controls.
 */
static void steal_disk_table(void)
{
    WORD far *ivt_1e = (WORD far *) 0x00000078L;   /* vector 1Eh */
    BYTE far *src;
    BYTE far *dst;
    int i;

    /* Follow the existing vector, offset then segment. */
    src = (BYTE far *) (((unsigned long) ivt_1e[1] << 16) | ivt_1e[0]);
    dst = (BYTE far *) (((unsigned long) DBT_COPY_SEG << 16) | DBT_COPY_OFF);

    for (i = 0; i < 16; ++i)
        dst[i] = src[i];

    /* Point the vector at the copy. */
    ivt_1e[0] = DBT_COPY_OFF;
    ivt_1e[1] = DBT_COPY_SEG;
}

/* ------------------------------------------------------- 2/3. video ----
 * Two ROM signature bytes decide whether the equipment word needs forcing.
 * If F000:FFFE is not 0xFF, or F000:C000 is not 0x21, the BIOS equipment
 * flags at 0040:0010 are rewritten so the video field says 40-column colour;
 * then mode 4 (320x200x4 CGA) is set.
 *
 * F000:C000 == 0x21 would be an option ROM signature byte -- on a machine
 * that has one, the equipment word is left alone.
 */
static void set_graphics_mode(void)
{
    BYTE far *rom_end  = (BYTE far *) 0x0F00FFFEL;
    BYTE far *rom_opt  = (BYTE far *) 0x0F00C000L;
    WORD far *equip    = (WORD far *) 0x00400010L;
    BYTE far *bios_mode;
    BYTE mode;

    if (*rom_end != 0xff || *rom_opt != 0x21) {
        *equip &= 0xffcf;          /* clear the video-type field */
        *equip |= 0x0010;          /* 40-column colour */
    }

    _asm {
        mov     ax, 4              /* 320x200 4-colour graphics */
        int     10h
    }

    /* Re-assert the mode control register with the monochrome bit set. The
     * BIOS keeps a shadow of 3D8h at 0040:0065; OR in bit 2 and write it
     * back to the hardware directly.  This is what makes the CGA palette
     * come out as the red/cyan/white set rather than green/magenta. */
    bios_mode = (BYTE far *) 0x00400065L;
    mode = (BYTE) (*bios_mode | 0x04);
    _asm {
        mov     dx, 3d8h
        mov     al, mode
        out     dx, al
    }
}

/* ------------------------------------------------- 4. title screen ------
 * Four tracks read straight into CGA video memory, so the picture appears
 * as it loads rather than after.  Sixteen reads of 8 sectors, alternating
 * head 0/1 and stepping the cylinder every second read, each advancing the
 * buffer by 0x1000 bytes.
 *
 * Starts at cylinder 0x26 (38) -- the title art lives near the end of the
 * disk.  DI counts the reads down from 4; BX walks through the framebuffer.
 */
static void load_title_screen(void)
{
    WORD offset   = 0;
    BYTE cylinder = 0x26;
    BYTE head     = 0;
    int  chunks   = 4;

    for (;;) {
        /* 8 sectors = 4096 bytes, starting at sector 5 of the track. */
        disk_read(CGA_SEG, offset, 8, cylinder, 5, head);

        offset += 0x1000;
        if (--chunks == 0)
            break;

        head ^= 1;                 /* inc dh; and dh,1 */
        if (head == 0)
            ++cylinder;            /* both heads done -- next cylinder */
    }
}

/* ---------------------------------------------------- 5. main load -----
 * The game image, 8K at a time.
 *
 * Each pass reads two lots of 8 sectors into the scratch buffer at 2D80 --
 * one at offset 0, one at 0x1000 -- then block-copies the whole 8K to the
 * running destination segment.  The staging step exists because the
 * destination overlaps memory the loader is still using.
 *
 * Track 4 is skipped.  Whatever is on it is not part of the image.
 */
static void load_game_image(void)
{
    VARS->dest_seg   = FIRST_DEST_SEG;
    VARS->unused     = 0;
    VARS->track      = 1;
    VARS->last_track = LAST_TRACK;

    for (;;) {
        BYTE far *src;
        BYTE far *dst;
        WORD i;

        /* Two reads of 8 sectors, head 0 then head 1. */
        disk_read(SCRATCH_SEG, 0x0000, 8, VARS->track, 1, 0);
        disk_read(SCRATCH_SEG, 0x1000, 8, VARS->track, 1, 1);

        /* Move the 8K to where it belongs. */
        src = (BYTE far *) (((unsigned long) SCRATCH_SEG << 16));
        dst = (BYTE far *) (((unsigned long) VARS->dest_seg << 16));
        for (i = 0; i < 0x2000; ++i)
            dst[i] = src[i];

        VARS->dest_seg += 0x200;   /* 8K in paragraphs */
        ++VARS->track;
        if (VARS->track == 4)      /* track 4 holds something else */
            ++VARS->track;

        if (VARS->track > VARS->last_track)
            break;
    }
}

/* ------------------------------------------------------ 6. relocate ----
 * Sixteen words at 0050:0000 each get 0x50 added.  That is a table of
 * segment values in the loaded image, being fixed up for the address the
 * image was actually loaded at.  Then control passes to 0050:0020.
 *
 * The far jump cannot be written in C -- a function pointer call would push
 * a return address, and there is nothing to return to.
 */
static void relocate_and_start(void)
{
    WORD far *table = (WORD far *) 0x00500000L;
    int i;

    for (i = 0; i < 16; ++i)
        table[i] += FIRST_DEST_SEG;

    _asm {
        db      0eah               /* ljmp 0050:0020 */
        dw      0020h
        dw      0050h
    }
}

/* ------------------------------------------------------------- entry ---
 * Entered by a far jump from the boot sector at 07C22, with the sector
 * already relocated to 0020:0000.  Never returns.
 */
void loader_main(void)
{
    steal_disk_table();
    set_graphics_mode();
    load_title_screen();
    load_game_image();
    relocate_and_start();
}
