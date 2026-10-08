/* SCUS-94448 input judgement assistance. Guest music clocks stay untouched. */
#include "cpu_state.h"
#include "mod_plugins.h"
#include "psx_cycles.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PACKAGE "lammy.accessibility.timing"
#define FEATURE "judgement"
#define PLUGIN "lammy.timing"

static int offset_ms, early_ms, late_ms;
static FILE *trace_file;
static struct {
    int active, gate_active, player, route, original, adjusted;
    int tempo, offset, early, late, phase;
    uint32_t context;
    uint64_t cycle;
} pending;

static int option(const char *id, int fallback, int lo, int hi) {
    char value[32], *end;
    if (!psx_mod_option_value(PACKAGE, FEATURE, id, value, sizeof value))
        return fallback;
    long n = strtol(value, &end, 10);
    return end != value && !*end && n >= lo && n <= hi ? (int)n : fallback;
}

static int ticks(int ms, int tempo) {
    int64_t n = (int64_t)ms * tempo;
    return (int)(n < 0 ? -((-n + 30000) / 60000) : (n + 30000) / 60000);
}

static int extra_ticks(int ms, int tempo) {
    int n = ticks(ms, tempo);
    return n > 4 ? 4 : n;
}

static void restored(void) { memset(&pending, 0, sizeof pending); }

static void activate(void) {
    offset_ms = option("offset_ms", 0, -300, 300);
    early_ms = option("early_ms", 10, 0, 60);
    late_ms = option("late_ms", 10, 0, 60);
    restored();
    if (trace_file) fclose(trace_file);
    trace_file = NULL;
    const char *path = getenv("LAMMY_TIMING_TRACE");
    if (path && *path) {
        trace_file = fopen(path, "w");
        if (trace_file)
            fputs("cycle,player,route,input_tick,adjusted_tick,tempo,offset_ticks,extra_early,extra_late,phase,stock_accept,assist_accept\n", trace_file);
    }
    printf("lammy timing: offset=%d ms, extra early=%d ms, extra late=%d ms\n",
           offset_ms, early_ms, late_ms);
    fflush(stdout);
}

static void shift(CPUState *cpu, uint32_t address) {
    (void)address;
    restored();
    /* The retail loop distinguishes live pads (routes 0/1) from recorded
     * inputs (routes 2..4). s1 is a local musical timestamp, not the clock.
     * Check both player context and route before touching it. */
    uint32_t player = cpu->gpr[19], route = cpu->gpr[23];
    uint32_t context = cpu->gpr[16];
    if (player > 1 || route > 1 ||
        context != 0x80072688U + player * 0x60U ||
        (cpu->read_word(context + 0x30U) & 0xffU) != route)
        return;
    uint32_t packet = cpu->gpr[18];
    uint32_t song = cpu->read_word(0x8007277cU);
    if ((packet & 3U) || (packet & 0x1fffffffU) > 0x200000U - 0x1cU ||
        (song & 3U) || (song & 0x1fffffffU) > 0x200000U - 0x10U) {
        psx_mod_counter_add("lammy.timing.guard_reject", 1);
        return;
    }
    /* The retail converter at 0x8001a614 divides signed BPM*100 by 100,
     * then converts 75 Hz CD timestamps to 96 musical ticks per beat. */
    int bpm100 = (int16_t)(cpu->read_word(song + 4U) >> 16);
    if (bpm100 < 3000 || bpm100 > 30000) {
        psx_mod_counter_add("lammy.timing.guard_reject", 1);
        return;
    }
    int tempo = (bpm100 / 100) * 96;
    int original = (int32_t)cpu->gpr[17];
    int offset = ticks(offset_ms, tempo);
    int64_t adjusted = (int64_t)original - offset;
    if (adjusted < 0 || adjusted > INT32_MAX - 12) {
        psx_mod_counter_add("lammy.timing.guard_reject", 1);
        return;
    }
    pending.active = 1;
    pending.context = context;
    pending.player = (int)player;
    pending.route = (int)route;
    pending.original = original;
    pending.adjusted = (int)adjusted;
    pending.tempo = tempo;
    pending.offset = offset;
    pending.early = extra_ticks(early_ms, tempo);
    pending.late = extra_ticks(late_ms, tempo);
    pending.cycle = psx_cycle_count;
    cpu->gpr[17] = (uint32_t)adjusted;
    psx_mod_counter_add("lammy.timing.inputs", 1);
}

static void gate_entry(CPUState *cpu, uint32_t address) {
    (void)address;
    pending.gate_active = pending.active && cpu->gpr[31] == 0x80017338U &&
        cpu->gpr[20] == pending.context && cpu->gpr[5] == 1 && cpu->gpr[6] == 4 &&
        cpu->gpr[4] == (uint32_t)pending.adjusted;
    if (pending.gate_active)
        pending.phase = (pending.adjusted + 12) % 24 - 12;
}

static void gate_finish(CPUState *cpu, uint32_t address) {
    (void)address;
    if (!pending.gate_active) return;
    int stock = (int)cpu->gpr[7];
    int expected = pending.phase >= -8 && pending.phase <= 7;
    /* If the observed retail predicate differs, preserve it and report the
     * guard failure. This protects against an unexpected guest path. */
    if (stock != expected) {
        psx_mod_counter_add("lammy.timing.guard_reject", 1);
        restored();
        return;
    }
    int accepted = pending.phase >= -8 - pending.early &&
                   pending.phase <= 7 + pending.late;
    /* JR's delay slot moves a3 to v0. Only replace the timing predicate;
     * button matching, turn rules and scoring continue through retail code. */
    cpu->gpr[7] = (uint32_t)accepted;
    psx_mod_counter_add(accepted ? "lammy.timing.in_window" : "lammy.timing.outside_window", 1);
    if (accepted && !stock) psx_mod_counter_add("lammy.timing.extra_accept", 1);
    if (trace_file) {
        fprintf(trace_file, "%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            (unsigned long long)pending.cycle, pending.player, pending.route,
            pending.original, pending.adjusted, pending.tempo, pending.offset,
            pending.early, pending.late, pending.phase, stock, accepted);
        fflush(trace_file);
    }
    restored();
}

PSX_MOD_CONSTRUCTOR(lammy_register_timing) {
    (void)psx_mod_register_activation_plugin(PLUGIN, activate);
    (void)psx_mod_register_savestate_plugin(PLUGIN, restored);
    (void)psx_mod_register_instruction_plugin(PLUGIN, 0x8001b66cU, 0x00000000U, shift);
    (void)psx_mod_register_function_entry_plugin(PLUGIN, 0x8001b08cU, gate_entry);
    (void)psx_mod_register_instruction_plugin(PLUGIN, 0x8001b0dcU, 0x03e00008U, gate_finish);
}
