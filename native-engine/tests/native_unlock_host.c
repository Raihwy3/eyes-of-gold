#include <ultra64.h>
#include "bondconstants.h"
#include "bondtypes.h"
#include "native_settings.h"
#include "native_test_assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
s32 portAllUnlocked;
#include "native-unlock-queries.h"
int main(void) {
    unsetenv("GE_DEBUG_UNLOCKALL");
    native_settings_init(NULL);
    save_data earned={0},before;
    before=earned;
    for(int repeat=0;repeat<3;repeat++) {
        native_setting_set_unlock_all(1);
        for(int folder=0;folder<4;folder++)for(int stage=SP_LEVEL_DAM;stage<SP_LEVEL_MAX;stage++)
            for(int difficulty=DIFFICULTY_AGENT;difficulty<DIFFICULTY_MAX;difficulty++)
                assert(fileIsStageUnlockedAtDifficulty(folder,stage,difficulty)==STAGESTATUS_COMPLETED);
        for(unsigned i=0;i<sizeof retail_cheats/sizeof *retail_cheats;i++)assert(frontCheckIfCheatIsUnlocked(retail_cheats[i]));
        assert(!frontCheckIfCheatIsUnlocked(CHEAT_MAXAMMO));
        assert(!fileGetIsCheatUnlocked(&earned,1) && fileGetIsCheatUnlocked(&earned,2));
        assert(fileIs007ModeUnlocked(0));
        assert(fileIsStageUnlockedAtDifficulty(0,SP_LEVEL_MAX,0)==STAGESTATUS_LOCKED);
        native_setting_set_unlock_all(0);
        assert(!fileIs007ModeUnlocked(0));
        assert(!frontCheckIfCheatIsUnlocked(CHEAT_INVINCIBILITY) && frontCheckIfCheatIsUnlocked(CHEAT_DK_MODE));
        assert(fileIsStageUnlockedAtDifficulty(0,SP_LEVEL_DAM,0)==STAGESTATUS_LOCKED);
        assert(!fileGetIsCheatUnlocked(&earned,1) && fileGetIsCheatUnlocked(&earned,2));
        assert(!memcmp(&earned,&before,sizeof earned));
    }
    puts("PASS original unlock query preambles: every stage/difficulty, cheat availability and 007 mode; live on/off, bounds and unchanged earned save bytes");
}
