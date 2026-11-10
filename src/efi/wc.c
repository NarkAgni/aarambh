#include <efi.h>
#include <efilib.h>

#include "efi/wc.h"

#include "lib/memory.h"

#define MSR_PAT       0x277
#define PAT_TYPE_WC   0x01ULL
#define CPUID_PAT_BIT (1u << 16)

#define CR0_WP   (1ULL << 16)
#define CR4_PGE  (1ULL << 7)
#define CR4_LA57 (1ULL << 12)
#define CR4_CET  (1ULL << 23)

#define PTE_PRESENT   (1ULL << 0)
#define PTE_WRITE     (1ULL << 1)
#define PTE_USER      (1ULL << 2)
#define PTE_PWT       (1ULL << 3)
#define PTE_PCD       (1ULL << 4)
#define PTE_LARGE     (1ULL << 7)
#define PTE_PAT_SMALL (1ULL << 7)
#define PTE_PAT_LARGE (1ULL << 12)
#define PTE_NX        (1ULL << 63)
#define PTE_ADDRESS   0x000ffffffffff000ULL

#define PAGE_SIZE     4096ULL
#define TABLE_ENTRIES 512
#define TOP_LEVEL     4
#define SPARE_TABLES  8
#define MAX_CHANGES   (4096 + 64)

typedef struct {
    volatile UINT64 *entry;
    UINT64 value;
} Change;

/* Memory type changes only; split pages stay split, they map the same way. */
static Change *changes;
static int change_count;
static int change_overflow;

/* Tables that replaced a large page stay in use, so they are never handed out twice. */
static UINT64 *spare_tables;
static int spare_used;

static UINT64 range_start;
static UINT64 range_end;
static UINT64 original_pat;
static int pat_changed;
static int active;

static inline UINT64 read_msr(UINT32 index)
{
    UINT32 low;
    UINT32 high;

    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(index));
    return ((UINT64)high << 32) | low;
}

static inline void write_msr(UINT32 index, UINT64 value)
{
    __asm__ volatile("wrmsr" : : "c"(index), "a"((UINT32)value), "d"((UINT32)(value >> 32))
                     : "memory");
}

static inline UINT64 read_cr0(void)
{
    UINT64 value;

    __asm__ volatile("mov %%cr0, %0" : "=r"(value));
    return value;
}

static inline void write_cr0(UINT64 value)
{
    __asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

static inline UINT64 read_cr3(void)
{
    UINT64 value;

    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

static inline void write_cr3(UINT64 value)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

static inline UINT64 read_cr4(void)
{
    UINT64 value;

    __asm__ volatile("mov %%cr4, %0" : "=r"(value));
    return value;
}

static inline void write_cr4(UINT64 value)
{
    __asm__ volatile("mov %0, %%cr4" : : "r"(value) : "memory");
}

static inline void flush_caches(void)
{
    __asm__ volatile("sfence; wbinvd" : : : "memory");
}

/* Toggling CR4.PGE drops every TLB entry, global ones included. */
static void flush_tlb(void)
{
    UINT64 cr4 = read_cr4();

    if (cr4 & CR4_PGE) {
        write_cr4(cr4 & ~CR4_PGE);
        write_cr4(cr4);
    } else {
        write_cr3(read_cr3());
    }
}

static int has_pat(void)
{
    UINT32 a;
    UINT32 b;
    UINT32 c;
    UINT32 d;

    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (d & CPUID_PAT_BIT) != 0;
}

/* Level 1 maps 4 KiB, 2 maps 2 MiB, 3 maps 1 GiB, 4 is the top table. */
static int level_shift(int level)
{
    return 12 + 9 * (level - 1);
}

static int is_leaf(UINT64 entry, int level)
{
    return level == 1 || (level <= 3 && (entry & PTE_LARGE));
}

static UINT64 pat_bit(int level)
{
    if (level == 1) {
        return PTE_PAT_SMALL;
    }
    return PTE_PAT_LARGE;
}

static UINT64 *table_of(UINT64 entry)
{
    return (UINT64 *)(UINTN)(entry & PTE_ADDRESS);
}

static UINT64 leaf_address(UINT64 entry, int level)
{
    return entry & PTE_ADDRESS & ~((1ULL << level_shift(level)) - 1);
}

static int cache_index(UINT64 entry)
{
    return ((entry & PTE_PCD) ? 2 : 0) | ((entry & PTE_PWT) ? 1 : 0);
}

static int leaf_index(UINT64 entry, int level)
{
    return ((entry & pat_bit(level)) ? 4 : 0) | cache_index(entry);
}

static UINT64 with_index(UINT64 entry, int level, int index)
{
    entry &= ~(PTE_PWT | PTE_PCD | pat_bit(level));

    if (index & 1) {
        entry |= PTE_PWT;
    }
    if (index & 2) {
        entry |= PTE_PCD;
    }
    if (index & 4) {
        entry |= pat_bit(level);
    }

    return entry;
}

/* Every PAT index some mapping (or a page table fetch) already depends on. */
static void scan_table(const UINT64 *table, int level, UINT8 *used)
{
    int i;

    for (i = 0; i < TABLE_ENTRIES; i++) {
        UINT64 entry = table[i];

        if ((entry & PTE_PRESENT) == 0) {
            continue;
        }

        if (is_leaf(entry, level)) {
            used[leaf_index(entry, level)] = 1;
        } else {
            used[cache_index(entry)] = 1;
            scan_table(table_of(entry), level - 1, used);
        }
    }
}

/* A PAT slot that is already WC, else one no mapping uses; -1 if neither. */
static int choose_index(UINT64 pat, const UINT8 *used, int *reprogram)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (((pat >> (i * 8)) & 0xff) == PAT_TYPE_WC) {
            *reprogram = 0;
            return i;
        }
    }

    for (i = 7; i > 0; i--) {
        if (!used[i]) {
            *reprogram = 1;
            return i;
        }
    }

    return -1;
}

static int set_type(volatile UINT64 *entry, int level, int index)
{
    if (change_count >= MAX_CHANGES) {
        change_overflow = 1;
        return 0;
    }

    changes[change_count].entry = entry;
    changes[change_count].value = *entry;
    change_count++;
    *entry = with_index(*entry, level, index);
    return 1;
}

static void undo_types(void)
{
    while (change_count > 0) {
        change_count--;
        *changes[change_count].entry = changes[change_count].value;
    }
}

/* Replace a large page with a table of smaller pages that map the same memory the same way. */
static int split_entry(volatile UINT64 *entry, int level)
{
    UINT64 old = *entry;
    UINT64 base = leaf_address(old, level);
    UINT64 child_size = 1ULL << level_shift(level - 1);
    UINT64 flags = old & ~PTE_ADDRESS & ~(PTE_PWT | PTE_PCD | PTE_LARGE);
    int index = leaf_index(old, level);
    UINT64 *table;
    int i;

    if (spare_used >= SPARE_TABLES) {
        return 0;
    }

    table = spare_tables + spare_used * TABLE_ENTRIES;
    spare_used++;

    for (i = 0; i < TABLE_ENTRIES; i++) {
        UINT64 child = (base + i * child_size) | flags;

        if (level - 1 > 1) {
            child |= PTE_LARGE;
        }
        table[i] = with_index(child, level - 1, index);
    }

    *entry = (UINT64)(UINTN)table | (old & (PTE_PRESENT | PTE_WRITE | PTE_USER | PTE_NX));
    return 1;
}

static int apply_range(UINT64 *table, int level, UINT64 start, UINT64 end, int index)
{
    UINT64 size = 1ULL << level_shift(level);
    UINT64 address = start;

    while (address < end) {
        volatile UINT64 *entry = &table[(address >> level_shift(level)) & (TABLE_ENTRIES - 1)];
        UINT64 entry_start = address & ~(size - 1);
        UINT64 entry_end = entry_start + size;
        UINT64 chunk_end = entry_end < end ? entry_end : end;

        if ((*entry & PTE_PRESENT) == 0) {
            return 0;
        }

        if (is_leaf(*entry, level) && address == entry_start && chunk_end == entry_end) {
            if (!set_type(entry, level, index)) {
                return 0;
            }
        } else {
            if (is_leaf(*entry, level) && !split_entry(entry, level)) {
                return 0;
            }
            if (!apply_range(table_of(*entry), level - 1, address, chunk_end, index)) {
                return 0;
            }
        }

        address = chunk_end;
    }

    return 1;
}

static int allocate_buffers(void)
{
    EFI_PHYSICAL_ADDRESS address;

    if (changes == NULL) {
        changes = mem_alloc(MAX_CHANGES * sizeof(Change));
    }

    if (spare_tables == NULL
        && BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, SPARE_TABLES, &address)
               == EFI_SUCCESS) {
        spare_tables = (UINT64 *)(UINTN)address;
    }

    return changes != NULL && spare_tables != NULL;
}

static void restore_pat(void)
{
    if (pat_changed) {
        write_msr(MSR_PAT, original_pat);
        pat_changed = 0;
    }
}

/* Runs with interrupts off and page table write protection lifted. */
static int apply_locked(void)
{
    UINT8 used[8] = { 0 };
    UINT64 cr3 = read_cr3();
    UINT64 *top = table_of(cr3);
    int reprogram = 0;
    int index;

    used[cache_index(cr3)] = 1;
    scan_table(top, TOP_LEVEL, used);

    original_pat = read_msr(MSR_PAT);
    index = choose_index(original_pat, used, &reprogram);
    if (index < 0) {
        return 0;
    }

    if (reprogram) {
        flush_caches();
        write_msr(MSR_PAT, (original_pat & ~(0xffULL << (index * 8)))
                               | (PAT_TYPE_WC << (index * 8)));
        pat_changed = 1;
    }

    change_count = 0;
    change_overflow = 0;

    if (!apply_range(top, TOP_LEVEL, range_start, range_end, index)) {
        undo_types();
        flush_tlb();
        flush_caches();
        restore_pat();
        flush_caches();
        return 0;
    }

    flush_tlb();
    flush_caches();
    return 1;
}

int wc_enable(UINT64 base, UINT64 size)
{
    EFI_TPL tpl;
    UINT64 cr0;
    UINT64 cr4;
    int applied;

    if (active) {
        return 1;
    }

    /* Clearing CR0.WP faults while CET is on; 5-level tables are not walked. */
    cr4 = read_cr4();
    if (!has_pat() || (cr4 & (CR4_CET | CR4_LA57)) || base == 0 || size == 0) {
        return 0;
    }

    if (!allocate_buffers()) {
        return 0;
    }

    range_start = base & ~(PAGE_SIZE - 1);
    range_end = (base + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    tpl = BS->RaiseTPL(TPL_HIGH_LEVEL);
    cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);

    applied = apply_locked();

    write_cr0(cr0);
    BS->RestoreTPL(tpl);

    active = applied;
    return applied;
}

void wc_disable(void)
{
    EFI_TPL tpl;
    UINT64 cr0;

    if (!active) {
        return;
    }

    tpl = BS->RaiseTPL(TPL_HIGH_LEVEL);
    cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);

    flush_caches();
    undo_types();
    flush_tlb();
    flush_caches();
    restore_pat();
    flush_caches();

    write_cr0(cr0);
    BS->RestoreTPL(tpl);

    active = 0;
}
