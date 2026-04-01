/*
 * memory_hierarchy.c
 *
 * Measures and compares access latencies across the memory hierarchy:
 *   CPU registers -> L1 cache -> L2 cache -> last-level cache (LLC) ->
 *   main memory (DRAM) -> file system -> network (TCP loopback).
 *
 * Compile:  make
 * Run:      ./memory_hierarchy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ============================================================
 * Timing utility: monotonic nanosecond clock
 * ============================================================ */

static long long get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

/* ============================================================
 * Benchmark 1 -- CPU registers
 *
 * Local integer variables are held in CPU registers by the
 * compiler.  A chain of four additions creates a loop-carried
 * dependency that prevents the compiler from collapsing the
 * loop into a constant.  A volatile write at the end prevents
 * the loop from being eliminated as dead code.
 *
 * What we measure: the latency of a single integer ALU op
 * when all operands already live in registers (no memory access).
 * ============================================================ */

#define REG_ITERS  100000000   /* 100 million iterations */

static double bench_register(void) {
    uint64_t a = 1, b = 2, c = 3, d = 5;
    volatile uint64_t sink;
    long long t0, t1;
    int i;

    t0 = get_time_ns();
    for (i = 0; i < REG_ITERS; i++) {
        a += b;
        b += c;
        c += d;
        d += a;
    }
    t1 = get_time_ns();

    /* Force the compiler to produce the final values. */
    sink = a + b + c + d;
    (void)sink;

    return (double)(t1 - t0) / REG_ITERS;
}

/* ============================================================
 * Benchmarks 2-5 -- Cache levels and main memory (DRAM)
 *
 * Pointer-chasing technique: each array element stores a random
 * next index.  The hot loop is:
 *
 *     idx = arr[idx];
 *
 * Each load's address depends on the previous load's result,
 * so the CPU cannot prefetch ahead.  This exposes the raw
 * load-use latency of whichever memory level holds the data.
 *
 * Array sizes map to cache levels:
 *    32 KB  -> fits entirely in L1 data cache
 *   512 KB  -> fits in L2, too large for L1
 *    16 MB  -> fits in last-level cache (LLC), too large for L2
 *   128 MB  -> exceeds all cache levels; accesses hit DRAM
 *
 * A warmup pass runs before timing to ensure the data is
 * resident at the intended cache level (or to fault in pages
 * for the DRAM case).
 * ============================================================ */

/* Fill arr[0..n-1] with pseudo-random indices in [0, n-1].
 * We use an inline LCG instead of rand() because it is much
 * faster for initializing large arrays (no syscall overhead). */
static void init_random_indices(uint32_t *arr, size_t n) {
    uint32_t x = 2463534242u;   /* arbitrary non-zero seed */
    size_t i;
    for (i = 0; i < n; i++) {
        x = x * 1664525u + 1013904223u;
        arr[i] = (uint32_t)(x % (uint32_t)n);
    }
}

static double bench_cache(size_t array_bytes, int accesses) {
    size_t n = array_bytes / sizeof(uint32_t);
    uint32_t *arr;
    uint32_t idx = 0;
    volatile uint32_t sink;
    long long t0, t1;
    int i;

    arr = (uint32_t *)malloc(array_bytes);
    if (arr == NULL) {
        perror("malloc");
        return -1.0;
    }

    init_random_indices(arr, n);

    /* Warmup: bring the data into the target cache level
     * (or fault in all virtual pages for the DRAM test). */
    for (i = 0; i < accesses; i++) {
        idx = arr[idx];
    }

    /* Timed pointer-chase pass. */
    t0 = get_time_ns();
    for (i = 0; i < accesses; i++) {
        idx = arr[idx];
    }
    t1 = get_time_ns();

    /* Sink prevents the compiler from eliminating the loop. */
    sink = idx;
    (void)sink;
    free(arr);

    return (double)(t1 - t0) / accesses;
}

/* ============================================================
 * Benchmark 6 -- File system
 *
 * Writes a 1 MB buffer to a temporary file (with fsync to flush
 * through the OS page cache onto storage), then re-reads it
 * with the OS page cache disabled so the read actually hits
 * the storage device.
 *
 * Latency is reported in microseconds per 4 KB page -- one
 * page-sized I/O is the natural unit at this level, analogous
 * to one cache-line fetch in the cache benchmarks above.
 * ============================================================ */

#define FILE_BYTES  (1 * 1024 * 1024)   /* 1 MB transfer */
#define PAGE_BYTES  4096                /* standard OS page size */
#define TMP_FILE    "/tmp/mem_hier_bench.bin"

static double bench_file(void) {
    char *buf;
    long long t0, t1;
    int fd;

    buf = (char *)malloc(FILE_BYTES);
    if (buf == NULL) { perror("malloc"); return -1.0; }
    memset(buf, 0xAB, FILE_BYTES);

    /* Write phase: flush all the way to storage with fsync. */
    fd = open(TMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) { perror("open (write)"); free(buf); return -1.0; }
    if (write(fd, buf, FILE_BYTES) != FILE_BYTES) {
        perror("write"); close(fd); free(buf); return -1.0;
    }
    fsync(fd);
    close(fd);

    /* Read phase: bypass the OS page cache so we measure storage,
     * not the kernel buffer cache handing back the just-written data. */
    fd = open(TMP_FILE, O_RDONLY);
    if (fd < 0) { perror("open (read)"); free(buf); return -1.0; }

#ifdef F_NOCACHE
    /* macOS: tell the kernel not to cache I/O through this fd. */
    fcntl(fd, F_NOCACHE, 1);
#endif

    t0 = get_time_ns();
    if (read(fd, buf, FILE_BYTES) < 0) perror("read");
    t1 = get_time_ns();

    close(fd);
    unlink(TMP_FILE);
    free(buf);

    /* Return microseconds per 4 KB page. */
    {
        int num_pages = FILE_BYTES / PAGE_BYTES;   /* = 256 pages in 1 MB */
        return (double)(t1 - t0) / (double)num_pages / 1000.0;
    }
}

/* ============================================================
 * Benchmark 7 -- Network (TCP loopback)
 *
 * Fork a child process that acts as a TCP echo server on the
 * loopback interface (127.0.0.1).  The parent connects as the
 * client and sends NET_ROUNDS small messages, waiting for each
 * echo before sending the next -- this serializes the messages
 * so we measure true round-trip latency, not throughput.
 *
 * Even on loopback (no physical wire or network card), both
 * endpoints traverse the full kernel TCP/IP stack, go through
 * system calls, and incur context switches between processes.
 * This is representative of the minimum network overhead.
 * ============================================================ */

#define LOOPBACK_PORT  59876
#define NET_ROUNDS     100
#define NET_MSG_BYTES  64

static double bench_network(void) {
    struct sockaddr_in addr;
    int srv, yes = 1;
    pid_t pid;

    /* Create and bind the listen socket before forking so both
     * parent (client) and child (server) inherit valid fds. */
    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return -1.0; }

    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(LOOPBACK_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(srv); return -1.0;
    }
    listen(srv, 1);

    pid = fork();
    if (pid < 0) { perror("fork"); close(srv); return -1.0; }

    if (pid == 0) {
        /* ---- Child: TCP echo server ---- */
        char buf[NET_MSG_BYTES];
        int conn, i;

        conn = accept(srv, NULL, NULL);
        close(srv);
        if (conn < 0) _exit(1);

        for (i = 0; i < NET_ROUNDS; i++) {
            ssize_t n = recv(conn, buf, NET_MSG_BYTES, MSG_WAITALL);
            if (n <= 0) break;
            send(conn, buf, (size_t)n, 0);
        }
        close(conn);
        _exit(0);
    }

    /* ---- Parent: TCP client ---- */
    {
        char buf[NET_MSG_BYTES];
        long long t0, t1;
        int cli, i;

        close(srv);       /* parent does not need the listen socket */
        usleep(5000);     /* allow child to reach accept() */

        cli = socket(AF_INET, SOCK_STREAM, 0);
        if (cli < 0 || connect(cli, (struct sockaddr *)&addr,
                               sizeof(addr)) < 0) {
            perror("connect"); close(cli);
            waitpid(pid, NULL, 0);
            return -1.0;
        }

        memset(buf, 0x55, NET_MSG_BYTES);

        /* Send each message and wait for its echo before the next. */
        t0 = get_time_ns();
        for (i = 0; i < NET_ROUNDS; i++) {
            send(cli, buf, NET_MSG_BYTES, 0);
            recv(cli, buf, NET_MSG_BYTES, MSG_WAITALL);
        }
        t1 = get_time_ns();

        close(cli);
        waitpid(pid, NULL, 0);

        /* Return microseconds per round trip. */
        return (double)(t1 - t0) / NET_ROUNDS / 1000.0;
    }
}

/* ============================================================
 * Main: run all benchmarks and print the results table
 * ============================================================ */

int main(void) {
    double reg_ns, l1_ns, l2_ns, llc_ns, ram_ns, file_us, net_us;

    printf("\n  Memory Hierarchy Latency Demo  --  COMP 251\n");
    printf("  ==================================================================\n");
    printf("  %-26s  %13s  %s\n", "Level", "Latency", "Notes");
    printf("  ------------------------------------------------------------------\n");

    /* Print each label before its benchmark so the user gets live
     * feedback as measurements run.  The value fills in after. */

    printf("  %-26s  ", "CPU Register (ALU op)");
    fflush(stdout);
    reg_ns = bench_register();
    printf("%9.2f ns/acc  local variable arithmetic\n", reg_ns);

    printf("  %-26s  ", "L1 Cache    (~32 KB)");
    fflush(stdout);
    l1_ns = bench_cache(32 * 1024, 5000000);
    printf("%9.2f ns/acc  pointer chase, array fits in L1\n", l1_ns);

    printf("  %-26s  ", "L2 Cache   (~512 KB)");
    fflush(stdout);
    l2_ns = bench_cache(512 * 1024, 2000000);
    printf("%9.2f ns/acc  pointer chase, array fits in L2\n", l2_ns);

    printf("  %-26s  ", "Last-Level Cache (~16 MB)");
    fflush(stdout);
    llc_ns = bench_cache(16 * 1024 * 1024, 500000);
    printf("%9.2f ns/acc  pointer chase, array fits in LLC\n", llc_ns);

    printf("  %-26s  ", "Main Memory  (~128 MB)");
    fflush(stdout);
    ram_ns = bench_cache(128 * 1024 * 1024, 200000);
    printf("%9.2f ns/acc  pointer chase, random DRAM access\n", ram_ns);

    printf("  %-26s  ", "File System    (1 MB)");
    fflush(stdout);
    file_us = bench_file();
    printf("%9.2f us/page  write + fsync + read, per 4 KB page\n", file_us);

    printf("  %-26s  ", "Network (TCP loopback)");
    fflush(stdout);
    net_us = bench_network();
    printf("%9.2f us/RTT   %d round trips, 64-byte messages\n",
           net_us, NET_ROUNDS);

    printf("  ==================================================================\n");

    /* Show how much slower each level is relative to a register op.
     * These ratios are the main teaching point of the demo. */
    if (reg_ns > 0.0) {
        printf("\n  Relative slowdown vs CPU register arithmetic:\n");
        if (l1_ns  > 0.0) printf("    L1 cache:        %7.0fx\n",  l1_ns  / reg_ns);
        if (l2_ns  > 0.0) printf("    L2 cache:        %7.0fx\n",  l2_ns  / reg_ns);
        if (llc_ns > 0.0) printf("    LLC:             %7.0fx\n",  llc_ns / reg_ns);
        if (ram_ns > 0.0) printf("    Main memory:     %7.0fx\n",  ram_ns / reg_ns);
        if (file_us > 0.0)
            printf("    File system:  %10.0fx\n", (file_us * 1000.0) / reg_ns);
        if (net_us > 0.0)
            printf("    Network:      %10.0fx\n", (net_us  * 1000.0) / reg_ns);
    }
    printf("\n");

    return 0;
}
