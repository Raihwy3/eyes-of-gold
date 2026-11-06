"""Apply the optional campaign loadout modifier at the two original intro passes."""
def replace_once(text,old,new):
    assert text.count(old)==1,old
    return text.replace(old,new)

def loadout(text):
    marker='void bondviewLoadSetupIntroSection(void)'
    text=replace_once(text,marker,'#include "native_subsistence.h"\n'+marker)
    guard='native_subsistence_enabled(getPlayerCount(),gamemode,(get_is_ramrom_flag() || get_recording_ramrom_flag()))'
    old='if (check_ramrom_flags() == intro_item->is_demo_playback)'
    text=replace_once(text,old,old[:-1]+' && (!'+guard+' || native_subsistence_item(intro_item->item_right,bossGetStageNum())))')
    old='if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->is_demo_playback)'
    text=replace_once(text,old,old[:-1]+' && (!'+guard+' || native_subsistence_ammo(((struct SetupIntroAmmo*)intro_record)->ammo_type,bossGetStageNum())))')
    text=replace_once(text,'    if (set_starting_weapon == 0)\n    {',
        '    if ('+guard+') {\n        starting_weapon[GUNRIGHT]=ITEM_FIST;starting_weapon[GUNLEFT]=ITEM_UNARMED;set_starting_weapon=1;\n    }\n\n    if (set_starting_weapon == 0)\n    {')
    return text

def frontend(text):
    prototypes='''static int native_cheat_active(int);
static u8 *native_cheat_label(int);
static u8 *native_cheat_value(int,u8 *);
static void native_cheat_toggle(int);
static void native_custom_cheats_sync(void);
'''
    text='#include "native_cheat_layout.h"\n'+prototypes+text
    text=replace_once(text,'u32 arrayUnlockedCheats[CHEAT_INVALID];','u32 arrayUnlockedCheats[CHEAT_MAX];')
    a=text.index('void init_menu15_cheat(void)\n{');b=text.index('void update_menu15_cheat(void)',a)
    body=text[a:b]
    body=replace_once(body,'    load_walletbond();','    arrayUnlockedCheats[totalunlockedcheats++]=0;\n    arrayUnlockedCheats[totalunlockedcheats++]=CHEAT_INVALID+1;\n    arrayUnlockedCheats[totalunlockedcheats++]=CHEAT_INVALID+2;\n    arrayUnlockedCheats[totalunlockedcheats++]=CHEAT_INVALID+3;\n    load_walletbond();')
    text=text[:a]+body+text[b:]
    text=replace_once(text,'    is_cheat_menu_available = FALSE;','    is_cheat_menu_available = TRUE;\n    native_custom_cheats_sync();')
    text=replace_once(text,'  appendsp = FALSE;','  appendsp = native_setting_subsistence() || native_setting_randomiser();')
    text=replace_once(text,'        g_CheatActivated[i] = 1 - g_CheatActivated[i];','        native_cheat_toggle(i);')
    a=text.index('void interface_menu15_cheat(void)\n{');b=text.index('Gfx * constructor_menu15_cheat(Gfx *DL)\n{',a)
    part=text[a:b]
    first=part.index('            if (totalunlockedcheats >= 0xD)')
    last=part.index('\n        }\n    }\n\n    if (joyGetButtonsPressedThisFrame',first)
    part=part[:first]+'            cheathighlighted=native_cheat_hit(totalunlockedcheats,(int)cursor_h_pos,(int)cursor_v_pos);'+part[last:]
    text=text[:a]+part+text[b:]
    a=text.index('Gfx * constructor_menu15_cheat(Gfx *DL)\n{');b=text.index('//NO CONTROLLER SCREEN',a)
    body=text[a:b].replace('cheatGetMenuTextPointer(', 'native_cheat_label(')
    import re
    body=re.sub(r'g_CheatActivated\[(arrayUnlockedCheats\[var_fp(?: \+ 12)?\])\]',r'native_cheat_active(\1)',body)
    body=body.replace('temp_v0 = langGet(strID);','temp_v0 = arrayUnlockedCheats[var_fp]==CHEAT_INVALID+2?(u8 *)"":native_cheat_value(arrayUnlockedCheats[var_fp],langGet(strID));',1)
    body=body.replace('temp_v0 = langGet(strID);','temp_v0 = arrayUnlockedCheats[var_fp + 12]==CHEAT_INVALID+2?(u8 *)"":native_cheat_value(arrayUnlockedCheats[var_fp + 12],langGet(strID));',1)
    body=body.replace('((totalunlockedcheats >= 0xD) ? 0xC : totalunlockedcheats)','((totalunlockedcheats > native_cheat_split(totalunlockedcheats)) ? native_cheat_split(totalunlockedcheats) : totalunlockedcheats)')
    body=body.replace('totalunlockedcheats >= 0xd','totalunlockedcheats > native_cheat_split(totalunlockedcheats)')
    body=body.replace('totalunlockedcheats - 0xC','totalunlockedcheats - native_cheat_split(totalunlockedcheats)')
    body=body.replace('sp88 = 0xB3;','sp88 = native_cheat_value_x(0,arrayUnlockedCheats[var_fp]==CHEAT_INVALID+2,sp7C);')
    body=body.replace('sp88 = 0x158;','sp88 = native_cheat_value_x(1,arrayUnlockedCheats[var_fp + 12]==CHEAT_INVALID+2,sp7C);')
    body=body.replace('var_fp + 12','var_fp + native_cheat_split(totalunlockedcheats)').replace('var_fp + 0xc','var_fp + native_cheat_split(totalunlockedcheats)')
    body=body.replace('(var_fp * 0x14) + 0x35','native_cheat_row_y(var_fp,totalunlockedcheats)')
    text=text[:a]+body+text[b:]
    return text+'''
static int native_cheat_active(int id) {
    return id==0?native_setting_subsistence():id==CHEAT_INVALID+1?native_setting_randomiser():id>=CHEAT_INVALID+2?0:g_CheatActivated[id];
}
static u8 *native_cheat_label(int id) {
    return id==0?(u8 *)native_subsistence_label():id==CHEAT_INVALID+1?(u8 *)native_randomiser_label():id==CHEAT_INVALID+2?(u8 *)native_randomiser_seed_label():id==CHEAT_INVALID+3?(u8 *)"Change seed":(u8 *)cheatGetMenuTextPointer(id);
}
static u8 *native_cheat_value(int id,u8 *value) {
    return id==CHEAT_INVALID+3?(u8 *)"":id==CHEAT_INVALID+2?(u8 *)native_randomiser_seed_text():value;
}
static int native_cheat_changes_loadout(int id) {
    switch(id) {
    case CHEAT_ALLGUNS: case CHEAT_INFINITE_AMMO:
    case CHEAT_SILVER_PP7: case CHEAT_GOLD_PP7:
    case CHEAT_2X_ROCKET_LAUNCHER: case CHEAT_2X_GRENADE_LAUNCHER:
    case CHEAT_2X_RCP90: case CHEAT_2X_THROWING_KNIFE:
    case CHEAT_2X_HUNTING_KNIFE: case CHEAT_2X_LASER:
    case CHEAT_MAGNUM: case CHEAT_LASER: case CHEAT_GOLDEN_GUN:return 1;
    default:return 0;
    }
}
static void native_custom_cheats_sync(void) {
    if(native_setting_subsistence() || native_setting_randomiser())
        for(int i=1;i<CHEAT_INVALID;i++)if(native_cheat_changes_loadout(i))g_CheatActivated[i]=0;
    update_menu15_cheat();
}
static void native_cheat_toggle(int id) {
    if(id==0)native_setting_set_subsistence(!native_setting_subsistence());
    else if(id==CHEAT_INVALID+1)native_setting_set_randomiser(!native_setting_randomiser());
    else if(id==CHEAT_INVALID+2) {
        unsigned old=native_setting_randomiser_seed();
        extern unsigned native_bottom_seed_entropy(void) __attribute__((weak));
        unsigned entropy=native_bottom_seed_entropy?native_bottom_seed_entropy():0;
        unsigned seed=(old*1664525u+1013904223u+entropy)%999999u+1u;
        if(seed==old)seed=old%999999u+1u;
        native_setting_set_randomiser_seed(seed);
    }
    else if(id==CHEAT_INVALID+3) {
        extern void native_bottom_seed_open(int) __attribute__((weak));
        if(native_bottom_seed_open)native_bottom_seed_open(native_setting_randomiser_seed());
    }
    else {
        g_CheatActivated[id]=!g_CheatActivated[id];
        if(g_CheatActivated[id] && native_cheat_changes_loadout(id)){native_setting_set_subsistence(0);native_setting_set_randomiser(0);}
    }
    native_custom_cheats_sync();
}
'''
