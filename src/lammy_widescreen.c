#include "mod_plugins.h"
#include <stdio.h>
#include <string.h>

static void activate_widescreen(void) {
    char aspect[16];
    if (!psx_mod_option_value("lammy.enhancement.widescreen", "widescreen",
                              "aspect", aspect, sizeof aspect))
        strcpy(aspect, "adaptive");
    if (!strcmp(aspect, "4:3")) {
        (void)psx_mod_set_fixed_display_aspect(4, 3);
    } else if (!strcmp(aspect, "21:9")) {
        (void)psx_mod_set_fixed_display_aspect(21, 9);
    } else {
        (void)psx_mod_set_fixed_display_aspect(16, 9);
        if (!strcmp(aspect, "adaptive"))
            (void)psx_mod_set_adaptive_display_aspect(0, 0);
    }
    printf("lammy widescreen: %s\n", aspect);
    fflush(stdout);
}

PSX_MOD_CONSTRUCTOR(lammy_register_widescreen) {
    (void)psx_mod_register_activation_plugin("lammy.widescreen", activate_widescreen);
}
