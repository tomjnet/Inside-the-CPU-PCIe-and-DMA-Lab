/* DMA in 8 Minutes: Devices Move Data Themselves - slide 7: driver side: harvest and refill (C version of 07_harvest.cpp) */
/* Build: make 07_harvest_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The whole NIC receive queue as a model: driver (main), device (device_rx), host RAM (an array).
   Change RING to 4 or 16 and watch the drops of the large burst appear or vanish (the last check expects 8). */
#define RING 8u
#define BUF_SIZE 2048u
#define DMA_BASE 0x100000000ull
#define DONE 1u
#define MAX_PKT 1514u

typedef struct {
    uint64_t dma_addr;
    uint16_t buf_len;
    uint16_t pkt_len;
    uint32_t status;
} RxDesc;

/* A device register that counts its writes: every write is one MMIO store, one PCIe crossing.
   C has no operator overloading, so the store goes through doorbell_write instead of "=". */
typedef struct {
    uint32_t value;
    int writes;
} Doorbell;
typedef struct { Doorbell rx_tail; } Mmio;

static void doorbell_write(Doorbell *d, uint32_t v) { d->value = v; ++d->writes; }

static uint8_t ram[RING * BUF_SIZE];
static RxDesc ring[RING];
static Mmio mmio;
static uint32_t head = 0;            /* device side index */

/* fixed-seed splitmix64 in place of std::mt19937 (the packets differ from the C++ run, the counts do not) */
static uint64_t rng_state = 42;
static uint32_t rng_next(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
}

static uint64_t dma_map(const uint8_t *p) {
    return DMA_BASE + (uint64_t)(p - ram);
}
static const uint8_t *ram_at(uint64_t dma_addr) {
    return ram + (size_t)(dma_addr - DMA_BASE);
}

static void dma_write(uint64_t dma_addr, const uint8_t *data, size_t len) {
    memcpy(ram + (size_t)(dma_addr - DMA_BASE), data, len);
}

static int device_rx(const uint8_t *pkt, size_t len) {     /* slide 6 */
    if (head == mmio.rx_tail.value) return 0;
    RxDesc *d = &ring[head];
    dma_write(d->dma_addr, pkt, len);
    d->pkt_len = (uint16_t)len;
    d->status = DONE;
    head = (head + 1) % RING;
    return 1;
}

static uint64_t checksum(const uint8_t *p, size_t n) {
    uint64_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum = sum * 31 + p[i];
    return sum;
}

static uint64_t delivered_sum = 0;
static int delivered = 0;
static int inside_ram = 1;
static void deliver(const uint8_t *data, uint16_t len) {   /* the network stack: reads in place */
    if (data < ram || data + len > ram + sizeof ram) inside_ram = 0;
    delivered_sum += checksum(data, len);
    ++delivered;
}

int main(void) {
    for (uint32_t i = 0; i < RING; ++i) {                   /* slide 5: post every buffer */
        RxDesc d = { dma_map(ram + (size_t)i * BUF_SIZE), (uint16_t)BUF_SIZE, 0, 0 };
        ring[i] = d;
    }
    uint32_t tail = RING - 1;
    uint32_t next = 0;                                      /* driver side index */
    doorbell_write(&mmio.rx_tail, tail);

    static uint8_t pkt[MAX_PKT];
    uint64_t accepted_sum = 0;
    int sent = 0;
    int dropped = 0;
    const int bursts[] = { 3, 5, 1, 10, 7, 2 };
    printf("burst  accepted  dropped  harvested  head  tail\n");
    for (size_t b = 0; b < sizeof bursts / sizeof bursts[0]; ++b) {
        int burst = bursts[b];
        int ok = 0;
        for (int p = 0; p < burst; ++p) {                   /* the wire: packets arrive back to back */
            size_t len = 60 + rng_next() % 1455;
            for (size_t i = 0; i < len; ++i) pkt[i] = (uint8_t)(rng_next() & 0xFFu);
            ++sent;
            if (device_rx(pkt, len)) {
                ++ok;
                accepted_sum += checksum(pkt, len);
            } else {
                ++dropped;
            }
        }
        int before = delivered;

        /* Driver: harvest every DONE slot. Zero copies of the payload. */
        while (ring[next].status & DONE) {
            deliver(ram_at(ring[next].dma_addr), ring[next].pkt_len);
            ring[next].status = 0;           /* same buffer, back to the device */
            tail = next;
            next = (next + 1) % RING;
        }
        doorbell_write(&mmio.rx_tail, tail); /* one doorbell for the batch */

        printf("  %d%s%d         %d        %d          %u     %u\n", burst, burst < 10 ? "      " : "     ",
               ok, burst - ok, delivered - before, head, tail);
    }

    printf("sent %d, delivered %d, dropped %d (a burst larger than the %u usable slots overflows this ring)\n",
           sent, delivered, dropped, RING - 1);
    printf("doorbells (PCIe crossings by the CPU): %d for %d packets; payload bytes copied by the CPU: 0\n",
           mmio.rx_tail.writes, delivered);
    printf("every delivered pointer was inside the pinned buffers: %s\n", inside_ram ? "yes" : "NO");
    int ok = inside_ram && delivered + dropped == sent && delivered_sum == accepted_sum;
    if (RING == 8 && dropped != 3) ok = 0;                  /* 10 packets into 7 usable slots */
    fputs(ok ? "checksums of accepted and delivered packets match\n"
             : "MISMATCH: a packet was lost or damaged\n", stdout);
    return ok ? 0 : 1;
}
