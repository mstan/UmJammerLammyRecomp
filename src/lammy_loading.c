/* SCUS-94448 INT archive loader. Replace blocking reads, not resource setup.
 * Movies/XA and all guest clocks remain on the original path. */
#include "mod_plugins.h"
#include "mod_resident.h"
#include "bios_hle.h"
#include "cpu_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lammy_resident_catalog.inc"
#define CTX 0x801A2918u
static const PSXResidentPack *pack;
static int trace, retail, contract, installed;
static unsigned frame, blocks, load_start;
static int (*previous_hook)(CPUState *, uint32_t);
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static int ram(uint32_t a, uint32_t n) {
    return a>=0x80010000u && a<=0x80200000u && !(a&3u) && n<=0x80200000u-a;
}
static int archive_valid(PSXResidentSink *sink, uint32_t file, const uint8_t *p,
                         uint32_t n, int stock, void *user) {
    (void)sink; (void)file; (void)user;
    if (!stock) return 0;
    for (uint32_t at=0; at<=n && n-at>=8192;) {
        uint32_t type=word(p+at), count=word(p+at+4), sectors=word(p+at+8);
        if (type==0xFFFFFFFFu) return at+8192==n;
        /* Four header sectors, <=408 resource descriptors, then raw payload.
         * TIM/VAB/model blocks are already unpacked in the retail archive. */
        if (type<1 || type>3 || !count || count>408 || !sectors || sectors>0x3FF ||
            sectors>(n-at-8192)/2048) return 0;
        at+=8192+sectors*2048;
    }
    return 0;
}
static int code_contract(void) {
    static const PSXResidentRange ranges[]={
        {0x8001459Cu,0x80014664u}, {0x800150BCu,0x80015580u},
        {0x80015790u,0x8001580Cu}, {0x80015C18u,0x80015CC0u},
        {0x8001607Cu,0x80016150u}
    };
    return psx_resident_guest_ranges_match(ranges,5,
        "64491b9cd3e96c6f456476cca5b5fcdd6431179e8c18ce6dcdb97c4c99462bee");
}
static void name_at(uint32_t descriptor,char out[80]) {
    uint32_t a=psx_mod_read_word(descriptor);
    unsigned i=0;
    for (; i<79 && a+i>=0x80010000u && a+i<0x80200000u; ++i) {
        out[i]=(char)psx_mod_read_byte(a+i);
        if (!out[i]) return;
    }
    out[i]=0;
}
static int read_block(CPUState *cpu) {
    uint32_t flags=psx_mod_read_word(CTX+0x1C), descriptor=psx_mod_read_word(CTX+0x3C);
    uint32_t offset=psx_mod_read_word(0x801A290Cu), lba=psx_mod_read_word(CTX+0xC);
    uint32_t header=psx_mod_read_word(0x801A2908u), index=0;
    if (!ram(descriptor,0x38) || !ram(header,8192) || psx_mod_read_word(CTX+0x20)!=0 ||
        (flags&0x40000000u) || (flags&4u) || !(flags&3u) || offset>0xFFFFFu)
        return 0;
    const uint8_t *p=psx_resident_find_lba(pack,lba,8192,&index);
    if (!p || lba!=psx_resident_file_lba(pack,index)+offset ||
        psx_mod_read_word(descriptor+0x30)!=psx_resident_file_lba(pack,index)) return 0;
    uint32_t type=word(p), sectors=word(p+8), bytes=sectors*2048;
    const uint8_t *payload=type==0xFFFFFFFFu ? NULL :
        psx_resident_find_lba(pack,lba+4,bytes,NULL);
    /* Check allocation before any mutation, so unsupported requests can run
     * the original routine. Keep its actual allocator/bookkeeping effects. */
    if (type!=0xFFFFFFFFu) {
        uint32_t top=psx_mod_read_word(0x8007A8F8u), bottom=psx_mod_read_word(0x8007A8FCu);
        if (type<1 || type>3 || !payload || !bytes || !ram(bottom,bytes) ||
            bottom+bytes>=top || psx_mod_read_word(0x80058890u)>=0x600) return 0;
    }
    psx_mod_write_word(0x801A2910u,0);
    psx_mod_write_word(CTX+0x1C,flags&0xF7FFFFF7u);
    psx_mod_write_byte(CTX+0x24,0x80);
    if (!psx_mod_dma_write_ram(header,p,8192,(int)lba)) return 0;
    if (type==0xFFFFFFFFu) {
        /* Normal CD callback does not advance the terminator's sector. */
        psx_mod_write_word(CTX+0x1C,(flags&0xF7FFFFF7u)|0x20000000u);
        if (trace) { printf("lammy loading: done frame=%u frames=%u blocks=%u\n",frame,frame-load_start,blocks); fflush(stdout); }
    } else {
        uint32_t dest=psx_mod_call_guest(cpu,0x8001459Cu,0x80015230u,bytes,0,0,0);
        psx_mod_write_word(0x801A296Cu,dest);
        psx_mod_call_guest(cpu,0x800150BCu,0x8001525Cu,4,0,0,0);
        psx_mod_dma_write_ram(dest,payload,bytes,(int)lba+4);
        psx_mod_call_guest(cpu,0x800150BCu,0x8001515Cu,sectors,0,0,0);
        psx_mod_write_word(CTX+0x1C,(flags&0xF7FFFFF7u)|0x08000008u);
        ++blocks;
        psx_mod_counter_add("lammy.loading.blocks",1);
        psx_mod_counter_add("lammy.loading.bytes",8192+bytes);
    }
    return 1;
}
static int dispatch(CPUState *cpu,uint32_t pc) {
    if (pc==0x1607Cu && ram(cpu->gpr[4],0x38)) {
        char name[80]; name_at(cpu->gpr[4],name); load_start=frame; blocks=0;
        if (trace) { printf("lammy loading: begin frame=%u name=%s\n",frame,name); fflush(stdout); }
    }
    if (pc==0x152D8u) {
        if (psx_mod_read_word(0x801A290Cu)==0) { load_start=frame; blocks=0; }
        if (!contract) {
            contract=code_contract()?1:-1;
            psx_mod_counter_add(contract==1?"lammy.loading.code_verified":"lammy.loading.code_mismatch",1);
        }
        if (trace) { printf("lammy loading: read frame=%u lba=%u offset=%u flags=%08X mode=%s\n",frame,
            psx_mod_read_word(CTX+0xC),psx_mod_read_word(0x801A290Cu),
            psx_mod_read_word(CTX+0x1C),retail?"retail":"resident"); fflush(stdout); }
        if (!retail && pack && contract==1 && read_block(cpu)) return 1;
        psx_mod_counter_add("lammy.loading.fallback",1);
    }
    return previous_hook ? previous_hook(cpu,pc) : 0;
}
static void bind(void) {
    if (installed && g_psx_bios_hle_hook!=dispatch) {
        previous_hook=g_psx_bios_hle_hook;
        g_psx_bios_hle_hook=dispatch;
    }
}
static void tick(void) { ++frame; bind(); }
static void restore(void) { contract=0; bind(); }
static void activate(void) {
    const char *s=getenv("LAMMY_LOADING_TRACE"); trace=s && !strcmp(s,"1");
    s=getenv("LAMMY_LOADING_RETAIL"); retail=s && !strcmp(s,"1");
    const PSXResidentSpec spec={sizeof spec,"UmJammerLammyRecomp","int-v1",files,
        sizeof files/sizeof files[0],PSX_RESIDENT_REQUIRE_STOCK,4*1024*1024,
        archive_valid,NULL,3};
    pack=psx_resident_prepare(&spec); contract=0; installed=1;
    printf("lammy loading: %s\n",pack && !retail?"resident archives ready":"original disc loader"); fflush(stdout);
}
PSX_MOD_CONSTRUCTOR(lammy_register_loading) {
    psx_mod_register_activation_plugin("lammy.loading",activate);
    psx_mod_register_vblank_plugin("lammy.loading",tick);
    psx_mod_register_savestate_plugin("lammy.loading",restore);
}
