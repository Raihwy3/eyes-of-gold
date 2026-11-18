"""Exercise the actual original save-query overrides without modifying saves."""
from pathlib import Path
import re
import build,build_probe

def main():
    source=build.source_path(Path('src/game/file2.c')).read_text()
    # Keep the actual query preambles, stopping at their unchanged save reads.
    functions=[]
    for signature,stop,fallback in [
        ('bool fileGetIsCheatUnlocked(save_data *save, s32 cheat)','    if (cheat >= 0','return save ? (cheat==2) : FALSE;'),
        ('STAGESTATUS fileIsStageUnlockedAtDifficulty(s32 foldernum, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty)','    if (( fileIsFolderValid','return STAGESTATUS_LOCKED;'),
        ('s32 fileIs007ModeUnlocked(u32 folder)','    LEVEL_SOLO_SEQUENCE','return FALSE;')]:
        start=source.index(signature);end=source.index(stop,start)
        functions.append(source[start:end]+'    '+fallback+'\n}\n')
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    front=build.source_path(Path('src/game/front.c')).read_text()
    start=front.index('s32 frontCheckIfCheatIsUnlocked(s32 cheat)');end=front.index('\n#ifdef PORT',start)
    ui=front[start:end]
    ids=re.findall(r'case (CHEAT_\w+):\s*return native_setting_unlock_all',ui)
    assert len(ids)==23,ids
    functions += ['s32 selected_folder_num; static save_data ui_save;',
        'save_data *fileGetSaveForFoldernum(s32 folder){(void)folder;return &ui_save;}',
        's32 fileIsCradleCompletedForFolder(s32 folder){(void)folder;return 0;}',
        's32 fileIsAztecCompletedOnSecretOr00ForFolder(s32 folder){(void)folder;return 0;}',
        's32 fileIsEgyptCompletedOn00ForFolder(s32 folder){(void)folder;return 0;}',
        ui,'static const int retail_cheats[]={'+','.join(ids)+'};']
    (out/'native-unlock-queries.h').write_text('\n'.join(functions))
    binary=out/'unlock-all-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-Wno-unused-variable','-Wno-unused-parameter','-I'+str(out),'-no-pie',build.TARGET/'tests/native_unlock_host.c',build.TARGET/'src/native_settings.c','-o',binary])
    print(build_probe.run([binary]).stdout)
if __name__=='__main__':main()
