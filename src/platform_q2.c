/* Q2 MIPS startup and framebuffer bridge, adapted from the GPL-3 Q2 Dino Runner port. */
#include "app.h"

long sc(long n, long a, long b, long c, long d, long e, long f);
__asm__(".text\n.set noreorder\n.globl sc\nsc:\n"
        "move $2, $4\nmove $4, $5\nmove $5, $6\nmove $6, $7\n"
        "lw $7, 16($29)\nlw $8, 20($29)\nlw $9, 24($29)\n"
        "addiu $29, $29, -32\nsw $8, 16($29)\nsw $9, 20($29)\n"
        "syscall\naddiu $29, $29, 32\n"
        "beqz $7, 1f\nnop\nsubu $2, $0, $2\n1: jr $31\nnop\n"
        ".globl __start\n__start:\n"
        "lw $4, 0($29)\naddiu $5, $29, 4\nli $8, -8\nand $29, $29, $8\naddiu $29, $29, -32\n"
        "jal q2_main\nnop\nmove $4, $2\nli $2, 4246\nsyscall\n1: b 1b\nnop\n.set reorder\n");

#define SYS_fork 4002
#define SYS_execve 4011
#define SYS_wait4 4114
#define SYS_read 4003
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_unlink 4010
#define SYS_rename 4038
#define SYS_ioctl 4054
#define SYS_mmap 4090
#define SYS_munmap 4091
#define SYS_fsync 4118
#define SYS_bind 4169
#define SYS_recv 4175
#define SYS_socket 4183
#define SYS_poll 4188
#define SYS_clock_gettime 4263

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOPAN_DISPLAY 0x4606
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x100
#define O_TRUNC 0x200
#define O_CLOEXEC 0x80000
#define MSG_DONTWAIT 0x40

#define INPUT_SOCKET "/tmp/q2-zuma.input.sock"
#define SCORE_FILE "/mnt/mmc/.zuma/best"
#define SCORE_TEMP "/mnt/mmc/.zuma/best.new"

enum { FRAME_MS = 17 };
static struct app app;

void *memset(void *dest, int value, __SIZE_TYPE__ length)
{
    unsigned char *p = dest;
    while (length--)
        *p++ = (unsigned char)value;
    return dest;
}

void *memcpy(void *dest, const void *src, __SIZE_TYPE__ length)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (length--)
        *d++ = *s++;
    return dest;
}

static int bind_input(void)
{
    struct { unsigned short family; char path[108]; } address = {1, INPUT_SOCKET};
    int fd = (int)sc(SYS_socket, 1, 1, 0, 0, 0, 0); /* AF_UNIX, MIPS SOCK_DGRAM */
    if (fd < 0)
        return -1;
    sc(SYS_unlink, (long)INPUT_SOCKET, 0, 0, 0, 0, 0);
    if (!sc(SYS_bind, fd, (long)&address, sizeof address, 0, 0, 0))
        return fd;
    sc(SYS_close, fd, 0, 0, 0, 0, 0);
    return -1;
}

static unsigned now_ms(void)
{
    long time[2] = {0};
    sc(SYS_clock_gettime, 1, (long)time, 0, 0, 0, 0); /* CLOCK_MONOTONIC */
    return (unsigned)time[0] * 1000 + (unsigned)time[1] / 1000000;
}

static u32 load_score(void)
{
    u32 best = 0;
    int fd = (int)sc(SYS_open, (long)SCORE_FILE, O_CLOEXEC, 0, 0, 0, 0);
    if (fd < 0)
        return 0;
    long count = sc(SYS_read, fd, (long)&best, sizeof best, 0, 0, 0);
    sc(SYS_close, fd, 0, 0, 0, 0, 0);
    return count == sizeof best ? best : 0;
}

static void save_score(u32 best)
{
    int fd = (int)sc(SYS_open, (long)SCORE_TEMP, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                     0644, 0, 0, 0);
    if (fd < 0)
        return;
    int ok = sc(SYS_write, fd, (long)&best, sizeof best, 0, 0, 0) == sizeof best &&
             sc(SYS_fsync, fd, 0, 0, 0, 0, 0) == 0;
    if (sc(SYS_close, fd, 0, 0, 0, 0, 0) == 0 && ok &&
        sc(SYS_rename, (long)SCORE_TEMP, (long)SCORE_FILE, 0, 0, 0, 0) == 0)
        return;
    sc(SYS_unlink, (long)SCORE_TEMP, 0, 0, 0, 0, 0);
}

/* Small optional Q2 MCU click effect. A short-lived child runs the firmware's
 * buzzer command; failures are silent and must never prevent the game from running.
 * Prefer calling cmd_mcu directly rather than through a shell on every pad.
 */
static int sound_child = -1;
static void sound_poll(void)
{
    if (sound_child > 0) {
        long status = 0;
        long done = sc(SYS_wait4, sound_child, (long)&status, 1, 0, 0, 0);
        if (done == sound_child || done < 0)
            sound_child = -1;
    }
}
static void sound_play(int event)
{
    (void)event;
    sound_poll();
    if (sound_child > 0)
        return;
    long child = sc(SYS_fork, 0, 0, 0, 0, 0, 0);
    if (child == 0) {
        char *const argv[] = {"cmd_mcu", "write_str", "buzzer", 0};
        char *const env[] = {"PATH=/usr/bin:/bin:/usr/sbin:/sbin", 0};
        sc(SYS_execve, (long)"/usr/bin/cmd_mcu", (long)argv, (long)env, 0, 0, 0);
        sc(SYS_execve, (long)"/bin/cmd_mcu", (long)argv, (long)env, 0, 0, 0);
        /* Firmware variants may keep the executable elsewhere on PATH. */
        char *const fallback[] = {"/bin/sh", "-c", "cmd_mcu write_str buzzer", 0};
        sc(SYS_execve, (long)fallback[0], (long)fallback, (long)env, 0, 0, 0);
        sc(4246, 127, 0, 0, 0, 0, 0);
        for (;;) {}
    }
    if (child > 0)
        sound_child = (int)child;
}

/* The Q2 firmware stores display preferences in /mnt/data/config.ini.
 * [IPOD] LIGHT=0 is dark, LIGHT=1 is light, LIGHT=2 is automatic.
 * Automatic mode currently falls back to dark because the game's tiny
 * freestanding runtime does not have the firmware's localtime helper.
 * This runs once when the game launches.
 */
static int theme_from_config(void){
    int fd=(int)sc(SYS_open,(long)"/mnt/data/config.ini",O_CLOEXEC,0,0,0,0);
    if(fd<0)return 1;
    char line[96],chunk[256];
    int len=0,ipod=0,light=0;
    long got;
    while((got=sc(SYS_read,fd,(long)chunk,sizeof chunk,0,0,0))>0){
        for(int j=0;j<got;++j){
            char c=chunk[j];
            if(c=='\r')continue;
            if(c!='\n'&&len<(int)sizeof line-1){line[len++]=c;continue;}
            line[len]=0;len=0;
            char *p=line;while(*p==' '||*p=='\t')++p;
            if(*p=='['){
                ipod=p[1]=='I'&&p[2]=='P'&&p[3]=='O'&&p[4]=='D'&&p[5]==']';
            }else if(ipod&&p[0]=='L'&&p[1]=='I'&&p[2]=='G'&&p[3]=='H'&&p[4]=='T'){
                p+=5;while(*p==' '||*p=='\t')++p;
                if(*p=='='){
                    ++p;while(*p==' '||*p=='\t')++p;
                    if(*p>='0'&&*p<='2')light=*p-'0';
                }
            }
        }
    }
    sc(SYS_close,fd,0,0,0,0,0);
    return light!=1;
}

static void copy_page(u32 *dst, const u32 *src, unsigned stride)
{
    for (int x = 0; x < WIDTH; ++x)
        for (int y = 0; y < HEIGHT; ++y)
            dst[x * stride + y] = src[x * stride + y];
}

int q2_main(void)
{
    unsigned var[40], fix[17];
    int fb = (int)sc(SYS_open, (long)"/dev/fb0", O_RDWR | O_CLOEXEC, 0, 0, 0, 0);
    if (fb < 0 || sc(SYS_ioctl, fb, FBIOGET_VSCREENINFO, (long)var, 0, 0, 0) ||
        sc(SYS_ioctl, fb, FBIOGET_FSCREENINFO, (long)fix, 0, 0, 0))
        return 1;
    unsigned stride = fix[11] / sizeof(u32), front = var[5];
    if (var[0] != HEIGHT || var[1] != WIDTH || var[4] || var[6] != 32 ||
        var[8] != 16 || fix[11] % sizeof(u32) || stride < HEIGHT)
        return 1;
    unsigned rows = fix[5] / fix[11];
    if (rows > var[3])
        rows = var[3];
    if (rows < WIDTH || front > rows - WIDTH)
        return 1;
    long mapped = sc(SYS_mmap, 0, (long)fix[5], 3, 1, fb, 0); /* RW, MAP_SHARED */
    if (mapped < 0 && mapped >= -4095)
        return 1;
    u32 *memory = (u32 *)mapped;
    int input = bind_input();
    if (input < 0)
        return 1;

    if (!app_init(&app, load_score())) {
        sc(SYS_close, input, 0, 0, 0, 0, 0);
        sc(SYS_unlink, (long)INPUT_SOCKET, 0, 0, 0, 0, 0);
        sc(SYS_munmap, mapped, (long)fix[5], 0, 0, 0, 0);
        sc(SYS_close, fb, 0, 0, 0, 0, 0);
        return 1;
    }
    app.dark_mode=theme_from_config();
    unsigned visible = front;
    int can_pan = rows >= 2 * WIDTH && (front == 0 || front == WIDTH);
    struct { int fd; short events, revents; } poll_input = {input, 1, 0};
    unsigned previous = now_ms(), deadline = previous;
    int redraw = 1;
    while (!app.quit) {
        unsigned now = now_ms();
        int remaining = (int)(deadline - now);
        int animated = app_animated(&app);
        if ((animated && remaining <= 0) || (!animated && redraw)) {
            unsigned elapsed = now - previous;
            if (elapsed > 2 * FRAME_MS)
                elapsed = 2 * FRAME_MS;
            previous = now;
            deadline = now + FRAME_MS;
            unsigned page = can_pan ? (visible ? 0 : WIDTH) : visible;
            app_frame(&app, memory + page * stride, stride, elapsed);
            if (can_pan) {
                var[5] = page;
                if (sc(SYS_ioctl, fb, FBIOPAN_DISPLAY, (long)var, 0, 0, 0) == 0)
                    visible = page;
                else {
                    can_pan = 0;
                    copy_page(memory + visible * stride, memory + page * stride, stride);
                }
            }
            if (app.sound_pending) {
                sound_play(app.sound_event);
                app.sound_pending = 0;
            }
            sound_poll();
            if (app.save_pending) {
                save_score((u32)app.best);
                app.save_pending = 0;
            }
            redraw = 0;
            remaining = FRAME_MS;
            animated = app_animated(&app);
        }

        long ready = sc(SYS_poll, (long)&poll_input, 1, animated ? remaining : -1, 0, 0, 0);
        if (ready == 0 || ready == -4)
            continue;
        if (ready < 0 || !(poll_input.revents & 1))
            break;
        char message[64];
        long bytes = sc(SYS_recv, input, (long)message, sizeof message - 1, MSG_DONTWAIT, 0, 0);
        if (bytes > 0) {
            message[bytes] = '\0';
            if (app_input(&app, message))
                redraw = 1;
            /* Button sounds should not wait for a potentially expensive redraw. */
            if (app.sound_pending) {
                sound_play(app.sound_event);
                app.sound_pending = 0;
            }
            if (!animated && app_animated(&app))
                deadline = previous = now_ms();
        }
    }

    if (visible != front) {
        var[5] = front;
        sc(SYS_ioctl, fb, FBIOPAN_DISPLAY, (long)var, 0, 0, 0);
    }
    sc(SYS_close, input, 0, 0, 0, 0, 0);
    sc(SYS_unlink, (long)INPUT_SOCKET, 0, 0, 0, 0, 0);
    sc(SYS_munmap, mapped, (long)fix[5], 0, 0, 0, 0);
    sc(SYS_close, fb, 0, 0, 0, 0, 0);
    return app.quit ? 0 : 1;
}
