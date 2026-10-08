/* Synthetic data only: exercise the actual adapter's boundaries and writes. */
#include <assert.h>
#include "../src/lammy_loading.c"
static unsigned char memory[0x200000], before[0x200000], archive[24576];
static unsigned allocation_calls, dma_calls;
int (*g_psx_bios_hle_hook)(CPUState *,uint32_t);
uint32_t psx_mod_read_word(uint32_t a) { return word(memory+(a&0x1FFFFFFFu)); }
uint8_t psx_mod_read_byte(uint32_t a) { return memory[a&0x1FFFFFFFu]; }
void psx_mod_write_byte(uint32_t a,uint8_t v) { memory[a&0x1FFFFFFFu]=v; }
void psx_mod_write_word(uint32_t a,uint32_t v) {
    for(unsigned i=0;i<4;i++) memory[(a&0x1FFFFFFFu)+i]=(uint8_t)(v>>(i*8));
}
void psx_mod_counter_add(const char *s,uint32_t n) { (void)s;(void)n; }
int psx_mod_dma_write_ram(uint32_t a,const void *p,uint32_t n,int lba) {
    (void)lba; assert(ram(a,n)); memcpy(memory+(a&0x1FFFFFFFu),p,n); ++dma_calls;return 1;
}
uint32_t psx_mod_call_guest(CPUState *cpu,uint32_t fn,uint32_t ra,uint32_t a,
                           uint32_t b,uint32_t c,uint32_t d) {
    (void)cpu;(void)ra;(void)b;(void)c;(void)d;
    if(fn==0x8001459Cu) {
        uint32_t out=psx_mod_read_word(0x8007A8FCu);
        psx_mod_write_word(0x8007A8FCu,out+a);++allocation_calls;return out;
    }
    assert(fn==0x800150BCu);
    psx_mod_write_word(CTX+0xC,psx_mod_read_word(CTX+0xC)+a);
    psx_mod_write_word(0x801A290Cu,psx_mod_read_word(0x801A290Cu)+a);return 0;
}
const uint8_t *psx_resident_find_lba(const PSXResidentPack *p,uint32_t lba,
                                    uint32_t n,uint32_t *file) {
    (void)p;if(lba<100 || lba>112 || n>sizeof archive-(lba-100)*2048) return NULL;
    if(file) *file=0;
    return archive+(lba-100)*2048;
}
uint32_t psx_resident_file_lba(const PSXResidentPack *p,uint32_t f) { (void)p;(void)f;return 100; }
int psx_resident_guest_ranges_match(const PSXResidentRange *r,uint32_t n,const char *h) { (void)r;(void)n;(void)h;return 1; }
const PSXResidentPack *psx_resident_prepare(const PSXResidentSpec *s) { (void)s;return NULL; }
int psx_mod_register_activation_plugin(const char *s,PSXModActivationCallback cb) { (void)s;(void)cb;return 1; }
int psx_mod_register_savestate_plugin(const char *s,PSXModActivationCallback cb) { (void)s;(void)cb;return 1; }
int psx_mod_register_vblank_plugin(const char *s,PSXModVBlankCallback cb) { (void)s;(void)cb;return 1; }
static void fixture(void) {
    memset(memory,0,sizeof memory);memset(archive,0,sizeof archive);
    archive[0]=1;archive[4]=1;archive[8]=4;
    for(unsigned i=8192;i<16384;i++) archive[i]=(uint8_t)(i*17+3);
    memset(archive+16384,255,4);
    psx_mod_write_word(CTX+0x3C,0x80011000u);
    psx_mod_write_word(0x80011030u,100);
    psx_mod_write_word(CTX+0xC,100);psx_mod_write_word(CTX+0x1C,2);
    psx_mod_write_word(0x801A2908u,0x80110000u);
    psx_mod_write_word(0x8007A8FCu,0x80080000u);
    psx_mod_write_word(0x8007A8F8u,0x80100000u);
    allocation_calls=dma_calls=0;
}
static void rejected(CPUState *cpu) {
    memcpy(before,memory,sizeof before);assert(!read_block(cpu));
    assert(!memcmp(before,memory,sizeof memory));assert(!allocation_calls && !dma_calls);
}
int main(void) {
    CPUState cpu={0};fixture();
    assert(archive_valid(NULL,0,archive,sizeof archive,1,NULL));
    assert(!archive_valid(NULL,0,archive,sizeof archive,0,NULL));
    archive[8]=255;archive[9]=255;
    assert(!archive_valid(NULL,0,archive,sizeof archive,1,NULL));fixture();
    assert(read_block(&cpu));assert(allocation_calls==1 && dma_calls==2);
    assert(!memcmp(memory+0x110000,archive,8192));
    assert(!memcmp(memory+0x80000,archive+8192,8192));
    assert(psx_mod_read_word(CTX+0xC)==108 && psx_mod_read_word(0x801A290Cu)==8);
    assert(psx_mod_read_word(CTX+0x1C)==0x0800000Au);
    assert(read_block(&cpu));assert(allocation_calls==1 && dma_calls==3);
    assert(psx_mod_read_word(CTX+0x1C)==0x20000002u);
    assert(psx_mod_read_word(CTX+0xC)==108);
    fixture();psx_mod_write_word(CTX+0x20,2);rejected(&cpu);
    fixture();psx_mod_write_word(CTX+0x1C,6);rejected(&cpu);
    fixture();psx_mod_write_word(CTX+0xC,99);rejected(&cpu);
    fixture();psx_mod_write_word(0x80011030u,101);rejected(&cpu);
    fixture();psx_mod_write_word(0x8007A8FCu,0x800FFFF0u);rejected(&cpu);
    fixture();psx_mod_write_word(0x801A2908u,0x801FFFFCu);rejected(&cpu);
    puts("PASS: archive integrity, exact payload writes, allocator/position effects, terminator and unchanged fallback guards");
}
