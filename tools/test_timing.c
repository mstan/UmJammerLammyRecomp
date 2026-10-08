#include <assert.h>
#include <stdio.h>
#include "../src/lammy_timing.c"

uint64_t psx_cycle_count;
int psx_mod_option_value(const char *p, const char *f, const char *o, char *out, uint32_t size) {
    (void)p; (void)f; (void)o; (void)out; (void)size; return 0;
}
void psx_mod_counter_add(const char *name, uint32_t delta) { (void)name; (void)delta; }
int psx_mod_register_activation_plugin(const char *id, PSXModActivationCallback cb) { (void)id; (void)cb; return 1; }
int psx_mod_register_savestate_plugin(const char *id, PSXModActivationCallback cb) { (void)id; (void)cb; return 1; }
int psx_mod_register_function_entry_plugin(const char *id, uint32_t a, PSXModFunctionEntryCallback cb) { (void)id; (void)a; (void)cb; return 1; }
int psx_mod_register_instruction_plugin(const char *id, uint32_t a, uint32_t w, PSXModFunctionEntryCallback cb) { (void)id; (void)a; (void)w; (void)cb; return 1; }

static uint32_t route, bpm100 = 12000;
static uint32_t read_test(uint32_t address) {
    if (address == 0x800726b8U) return route;
    if (address == 0x8007277cU) return 0x80080000U;
    if (address == 0x80080004U) return bpm100 << 16;
    return 0;
}
static CPUState input(int tick) {
    CPUState cpu = {0}; cpu.read_word = read_test;
    cpu.gpr[16] = 0x80072688U; cpu.gpr[17] = (uint32_t)tick;
    cpu.gpr[18] = 0x80078e28U; cpu.gpr[23] = route;
    shift(&cpu, 0); return cpu;
}
static int judge(int tick) {
    CPUState cpu = input(tick); assert(pending.active);
    int adjusted = (int)cpu.gpr[17];
    cpu.gpr[4] = (uint32_t)adjusted; cpu.gpr[5] = 1; cpu.gpr[6] = 4;
    cpu.gpr[20] = 0x80072688U; cpu.gpr[31] = 0x80017338U;
    gate_entry(&cpu, 0); assert(pending.gate_active);
    int retail_bucket = ((adjusted + 12) / 4) % 6;
    cpu.gpr[7] = retail_bucket >= 1 && retail_bucket <= 4;
    gate_finish(&cpu, 0); assert(!pending.active);
    return (int)cpu.gpr[7];
}
int main(void) {
    for (int i = 0; i < 8192; ++i) {
        int bucket = ((i + 12) / 4) % 6;
        assert(judge(i) == (bucket >= 1 && bucket <= 4));
    }
    assert(!judge(15)); assert(judge(16)); assert(judge(31)); assert(!judge(32));
    early_ms = 10; assert(judge(15)); assert(!judge(32));
    early_ms = 0; late_ms = 10; assert(!judge(15)); assert(judge(32));
    early_ms = late_ms = 60;
    for (int i = 0; i < 96; ++i) assert(judge(i));
    assert(extra_ticks(60, 11520) == 4);
    assert(ticks(100, 11520) == 19 && ticks(-100, 11520) == -19);
    early_ms = late_ms = 0;
    offset_ms = 100; assert(judge(24 + 19));
    offset_ms = -100; assert(judge(24 - 19));
    offset_ms = 300; CPUState cpu = input(0); assert(!pending.active && !cpu.gpr[17]);
    offset_ms = 0; route = 2; cpu = input(200); assert(!pending.active && cpu.gpr[17] == 200);
    route = 0; bpm100 = 0; cpu = input(200); assert(!pending.active && cpu.gpr[17] == 200);
    bpm100 = 12000; cpu = input(200); assert(pending.active); restored(); assert(!pending.active);
    cpu = input(200); cpu.gpr[31] = 0x800174ecU; gate_entry(&cpu, 0); assert(!pending.gate_active);
    puts("PASS: 8192 stock cases, one-sided widening, maximum range, signed offsets and replay/tempo guards");
}
