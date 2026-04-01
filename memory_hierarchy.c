/*
 * memory_hierarchy.c
 *
 * Measures and compares access latencies across the memory hierarchy:
 *   CPU registers -> L1 cache -> L2 cache -> last-level cache (LLC) ->
 *   main memory (DRAM) -> file system -> network (TCP loopback).
 *
 * Before running the labeled benchmark, the program sweeps array sizes
 * from 4 KB to 256 MB and looks for latency jumps to automatically
 * detect the cache level boundaries on the current machine.
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
 * Warmup strategy: a sequential scan (not pointer-chasing) is
 * used to warm up the data.  The hardware prefetcher handles
 * sequential access extremely efficiently, so even a 256 MB
 * array can be faulted in and loaded in a few milliseconds.
 * A pointer-chase warmup would require millions of iterations
 * to touch the same fraction of a large array.
 * ============================================================ */

/* Shared sink used to prevent the compiler from eliminating
 * warmup loops and timed loops as dead code. */
static volatile uint32_t v_sink;

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
    uint32_t sum = 0, idx = 0;
    long long t0, t1;
    size_t i;
    int j;

    arr = (uint32_t *)malloc(array_bytes);
    if (arr == NULL) {
        perror("malloc");
        return -1.0;
    }

    init_random_indices(arr, n);

    /* Sequential warmup: scan the entire array once so all pages
     * are faulted in and the data is resident in the appropriate
     * cache level.  The hardware prefetcher handles sequential
     * access with near-zero overhead, making this O(n) in time
     * regardless of array size -- far more efficient than trying
     * to warm up via the same pointer-chase used for timing. */
    for (i = 0; i < n; i++) sum += arr[i];
    v_sink = sum;   /* prevent dead-code elimination of the warmup */

    /* Timed pointer-chase pass: each iteration's address is
     * unknown until the previous load completes, defeating
     * hardware prefetching and exposing raw load latency. */
    t0 = get_time_ns();
    for (j = 0; j < accesses; j++) idx = arr[idx];
    t1 = get_time_ns();

    v_sink = idx;   /* prevent dead-code elimination of the timed loop */
    free(arr);

    return (double)(t1 - t0) / accesses;
}

/* ============================================================
 * Cache boundary detection via latency sweep
 *
 * We run bench_cache at power-of-2 array sizes from 4 KB to
 * 256 MB.  When the latency between consecutive sizes jumps by
 * more than BOUNDARY_RATIO, we have crossed a cache boundary.
 * The size just before the jump is the capacity of that level.
 *
 * This approach requires no OS-specific API and works on any
 * machine -- the program discovers the hardware topology at
 * runtime rather than assuming fixed sizes.
 *
 * Limitation: the sweep uses powers of 2, so a cache whose
 * size falls between two sweep points (e.g., a 44 MB LLC
 * between the 32 MB and 64 MB sweep sizes) cannot be cleanly
 * isolated.  Its boundary will appear at the nearest power of
 * 2 below its actual capacity.
 * ============================================================ */

/* Seventeen sizes from 4 KB to 256 MB, each double the last. */
#define NUM_SWEEP  17
#define BOUNDARY_RATIO  2.0   /* relative latency jump that signals a new level */

static const size_t SWEEP_SIZES[NUM_SWEEP] = {
    4UL*1024,           /*   4 KB */
    8UL*1024,           /*   8 KB */
    16UL*1024,          /*  16 KB */
    32UL*1024,          /*  32 KB */
    64UL*1024,          /*  64 KB */
    128UL*1024,         /* 128 KB */
    256UL*1024,         /* 256 KB */
    512UL*1024,         /* 512 KB */
    1024UL*1024,        /*   1 MB */
    2UL*1024*1024,      /*   2 MB */
    4UL*1024*1024,      /*   4 MB */
    8UL*1024*1024,      /*   8 MB */
    16UL*1024*1024,     /*  16 MB */
    32UL*1024*1024,     /*  32 MB */
    64UL*1024*1024,     /*  64 MB */
    128UL*1024*1024,    /* 128 MB */
    256UL*1024*1024     /* 256 MB */
};

/* Scale the iteration count so each sweep point takes roughly
 * the same wall-clock time, regardless of which cache level
 * the array falls in. */
static int sweep_iters(size_t bytes) {
    if (bytes <=   32UL*1024)         return 4000000;
    if (bytes <=  512UL*1024)         return 2000000;
    if (bytes <=    4UL*1024*1024)    return  500000;
    if (bytes <=   64UL*1024*1024)    return  200000;
    return 100000;
}

/* Minimum buffer size required by format_size() -- the longest
 * possible output is e.g. "9999 MB\0" = 8 bytes; 16 gives margin. */
#define SIZE_BUF_BYTES 16

/* Write a human-readable size string (e.g., "   4 KB", " 256 MB")
 * into buf.  buf must point to at least SIZE_BUF_BYTES bytes.
 *
 * We cast to unsigned int (%u) rather than using size_t (%zu) to
 * give the compiler a provably bounded value.  All sweep sizes fit
 * comfortably in unsigned int (max value: 256 for 256 MB).        */
static void format_size(size_t bytes, char *buf) {
    if (bytes >= 1024UL*1024)
        sprintf(buf, "%4u MB", (unsigned int)(bytes / (1024UL*1024)));
    else
        sprintf(buf, "%4u KB", (unsigned int)(bytes / 1024));
}

/* Run the sweep, print one row per size (with transition annotations),
 * fill boundaries[0..3] with the detected cache capacities, and
 * return the number of boundaries found.
 * boundaries[k] is the last sweep size that fits within level k. */
static int run_sweep(size_t *boundaries) {
    double prev = 0.0, lat = 0.0;
    int i, nb = 0;
    char sz_buf[16];
    static const char *TRANS[] = {"L1->L2", "L2->LLC", "LLC->DRAM"};

    printf("  %8s  %12s\n", "Array", "Latency");
    printf("  ------------------------\n");

    for (i = 0; i < NUM_SWEEP; i++) {
        format_size(SWEEP_SIZES[i], sz_buf);
        printf("  %8s  ", sz_buf);
        fflush(stdout);

        lat = bench_cache(SWEEP_SIZES[i], sweep_iters(SWEEP_SIZES[i]));
        if (lat < 0.0) {
            printf("(malloc failed -- stopping sweep)\n");
            break;
        }

        /* Check whether the previous size was a level boundary.
         * We annotate this row because it is the first row that
         * belongs to the new (slower) level. */
        if (prev > 0.0 && lat / prev > BOUNDARY_RATIO && nb < 4) {
            const char *label = (nb < 3) ? TRANS[nb] : "new level";
            printf("%9.2f ns  *** %s\n", lat, label);
            boundaries[nb++] = SWEEP_SIZES[i-1];
        } else {
            printf("%9.2f ns\n", lat);
        }
        prev = lat;
    }
    return nb;
}

/* Given the detected boundary sizes, pick one representative
 * array size for each labeled level:
 *
 *  L1:   boundaries[0]     -- the largest array that fits in L1
 *  L2:   boundaries[0]*2   -- one step past L1, within L2
 *         (capped at boundaries[1] if detected)
 *  LLC:  boundaries[1]*2   -- one step past L2, within LLC
 *         (capped at boundaries[2] if detected)
 *  RAM:  boundaries[nb-1]*4 -- well beyond all cache
 *         (capped at 256 MB; floored at 64 MB)
 *
 * Fallback sizes are used when fewer than the expected number
 * of boundaries were detected. */
static void derive_bench_sizes(const size_t *b, int nb,
                                size_t *l1, size_t *l2,
                                size_t *llc, size_t *ram) {
    *l1  = (nb >= 1) ? b[0]          : 32UL*1024;
    *l2  = (nb >= 1) ? b[0] * 2      : 512UL*1024;
    *llc = (nb >= 2) ? b[1] * 2      : 16UL*1024*1024;
    *ram = (nb >= 1) ? b[nb-1] * 4   : 128UL*1024*1024;

    /* Cap l2 so it stays within L2 when an L2 boundary is known. */
    if (nb >= 2 && *l2 >= b[1]) *l2 = b[1];

    /* Cap llc so it stays within LLC when an LLC boundary is known. */
    if (nb >= 3 && *llc >= b[2]) *llc = b[2];

    /* Clamp ram to a reasonable range. */
    if (*ram > 256UL*1024*1024) *ram = 256UL*1024*1024;
    if (*ram <  64UL*1024*1024) *ram =  64UL*1024*1024;
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
 * Main: sweep to detect cache topology, then run the full
 *       labeled benchmark using the detected sizes
 * ============================================================ */

int main(void) {
    size_t boundaries[4];
    size_t sz_l1, sz_l2, sz_llc, sz_ram;
    double reg_ns, l1_ns, l2_ns, llc_ns, ram_ns, file_us, net_us;
    char sz_buf[16];
    int nb;

    printf("\n  Memory Hierarchy Latency Demo  --  COMP 251\n");

    /* ---- Step 1: sweep to find cache level boundaries ---- */
    printf("\n  Sweeping array sizes to detect cache levels on this machine...\n");
    printf("  (*** marks the first measurement in each new cache level)\n\n");

    nb = run_sweep(boundaries);

    /* Print a plain-English summary of what was found. */
    printf("\n  Detected %d cache level boundary/boundaries:", nb);
    if (nb == 0) {
        printf(" none (using fallback sizes)\n");
    } else {
        int k;
        for (k = 0; k < nb; k++) {
            format_size(boundaries[k], sz_buf);
            printf("  L%d ~%s", k+1, sz_buf);
        }
        printf("\n");
    }

    /* ---- Step 2: derive benchmark sizes from boundaries ---- */
    derive_bench_sizes(boundaries, nb, &sz_l1, &sz_l2, &sz_llc, &sz_ram);

    /* ---- Step 3: run the labeled benchmark ---- */
    printf("\n  ==================================================================\n");
    printf("  %-28s  %13s  %s\n", "Level", "Latency", "Notes");
    printf("  ------------------------------------------------------------------\n");

    printf("  %-28s  ", "CPU Register (ALU op)");
    fflush(stdout);
    reg_ns = bench_register();
    printf("%9.2f ns/acc  local variable arithmetic\n", reg_ns);

    {
        /* Build each label with snprintf so the %-28s padding
         * is correct regardless of how wide the size string is. */
        char label[40];

        format_size(sz_l1, sz_buf);
        snprintf(label, sizeof(label), "L1 Cache    (%s)", sz_buf);
        printf("  %-28s  ", label); fflush(stdout);
        l1_ns = bench_cache(sz_l1, 5000000);
        printf("%9.2f ns/acc  pointer chase, fits in L1\n", l1_ns);

        format_size(sz_l2, sz_buf);
        snprintf(label, sizeof(label), "L2 Cache    (%s)", sz_buf);
        printf("  %-28s  ", label); fflush(stdout);
        l2_ns = bench_cache(sz_l2, 2000000);
        printf("%9.2f ns/acc  pointer chase, fits in L2\n", l2_ns);

        format_size(sz_llc, sz_buf);
        snprintf(label, sizeof(label), "LLC         (%s)", sz_buf);
        printf("  %-28s  ", label); fflush(stdout);
        llc_ns = bench_cache(sz_llc, 500000);
        printf("%9.2f ns/acc  pointer chase, fits in LLC\n", llc_ns);

        format_size(sz_ram, sz_buf);
        snprintf(label, sizeof(label), "Main Memory (%s)", sz_buf);
        printf("  %-28s  ", label); fflush(stdout);
        ram_ns = bench_cache(sz_ram, 200000);
        printf("%9.2f ns/acc  pointer chase, random DRAM access\n", ram_ns);
    }

    printf("  %-28s  ", "File System    (1 MB)");
    fflush(stdout);
    file_us = bench_file();
    printf("%9.2f us/page  write + fsync + read, per 4 KB page\n", file_us);

    printf("  %-28s  ", "Network (TCP loopback)");
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
