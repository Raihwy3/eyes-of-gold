"""Use the original local two-player match with only physical pad 0 present."""
import effect_lod

def _replace_once(text, old, new):
    assert old in text, f"Expected snippet not found in front.c: {old!r}"
    return text.replace(old, new, 1)

def front(text):
    # Only the frontend sees two available slots. Real controller input remains
    # one connected pad, and the original second player's input stays neutral.
    text=text.replace('joyGetControllerCount()','native_preview_controller_count()')
    text='#include "native_mp_bot.h"\nstatic int native_preview_controller_count(void){return 2;}\nstatic int highlight_bots;\n'+text
    signature='s32 check_if_mp_stage_unlocked(s32 stage)'
    # All original selectable arenas are available for this local preview.
    text=effect_lod.prefix(text,signature,'    if(stage<MP_STAGE_RANDOM || stage>=MP_STAGE_SELECTED_MAX)return FALSE;\n    return TRUE;')
    text='static void native_preview_ready_idle(void);\n'+text
    for signature in ('void interface_menu0F_mpcharsel(void)','void interface_menu10_mphandicap(void)','void interface_menu11_mpcontrols(void)'):
        text=effect_lod.prefix(text,signature,'    native_preview_ready_idle();')
    text+='\nstatic void native_preview_ready_idle(void){player_has_selected_char[PLAYER_2]=TRUE;}\n'

    # Compact MP Options rows slightly (18px step from 0x75..0x105) so row 9
    # ("Bots": Off/On at 0x117) fits cleanly inside the original folder area.
    text=_replace_once(text,'    highlight_aimadjustment = FALSE;\n','    highlight_aimadjustment = FALSE;\n    highlight_bots = FALSE;\n')
    text=_replace_once(text,'    s32 aimadjust_selected = 0;\n','    s32 aimadjust_selected = 0;\n    s32 bots_selected = 0;\n')
    text=_replace_once(text,'        highlight_aimadjustment = 0;\n','        highlight_aimadjustment = 0;\n        highlight_bots = 0;\n')
    text=_replace_once(text,
"""            s32 temp_f6 = cursor_v_pos;
            if ((temp_f6 >= 0x119) && (unlock_aim_sight))
            {
                highlight_aimadjustment = 1;
            }
            else if ((temp_f6 >= 0x105) && (unlock_control_style))
            {
                highlight_controlstyle = 1;
            }
            else if ((temp_f6 >= 0xF1) && (unlock_handicap))
            {
                highlight_health = 1;
            }
            else if ((temp_f6 >= 0xDD) && (unlock_chars))
            {
                highlight_character = 1;
            }
            else if ((temp_f6 >= 0xC9) && (unlock_weapon_select))
            {
                highlight_weaponselect = 1;
            }
            else if ((temp_f6 >= 0xB5) && (unlock_game_length))
            {
                highlight_gamelength = 1;
            }
            else if ((temp_f6 >= 0xA1) && (unlock_stage_select))
            {
                highlight_gameselect = 1;
            }
            else if (temp_f6 >= 0x8D)
            {
                highlight_scenario = 1;
            }
            else
            {
                highlight_players = 1;
            }""",
"""            s32 temp_f6 = cursor_v_pos;
            if (temp_f6 >= 0x117)
            {
                highlight_bots = 1;
            }
            else if ((temp_f6 >= 0x105) && (unlock_aim_sight))
            {
                highlight_aimadjustment = 1;
            }
            else if ((temp_f6 >= 0xF3) && (unlock_control_style))
            {
                highlight_controlstyle = 1;
            }
            else if ((temp_f6 >= 0xE1) && (unlock_handicap))
            {
                highlight_health = 1;
            }
            else if ((temp_f6 >= 0xCF) && (unlock_chars))
            {
                highlight_character = 1;
            }
            else if ((temp_f6 >= 0xBD) && (unlock_weapon_select))
            {
                highlight_weaponselect = 1;
            }
            else if ((temp_f6 >= 0xAB) && (unlock_game_length))
            {
                highlight_gamelength = 1;
            }
            else if ((temp_f6 >= 0x99) && (unlock_stage_select))
            {
                highlight_gameselect = 1;
            }
            else if (temp_f6 >= 0x87)
            {
                highlight_scenario = 1;
            }
            else
            {
                highlight_players = 1;
            }""")
    text=_replace_once(text,
"""        else if (highlight_aimadjustment)
        {
            aimadjust_selected = 1;
        }""",
"""        else if (highlight_aimadjustment)
        {
            aimadjust_selected = 1;
        }
        else if (highlight_bots)
        {
            bots_selected = 1;
        }""")
    text=_replace_once(text,
"""    if (aimadjust_selected)
    {
        advance_aim_settings_selection();
    }""",
"""    if (aimadjust_selected)
    {
        advance_aim_settings_selection();
        return;
    }
    if (bots_selected)
    {
        native_mp_bot_toggle();
    }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0x79;
  if (highlight_players) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x78,iStack28 + 0x3c,0x87,0x32);
  }""",
"""  x = 0x39;
  y = 0x75;
  if (highlight_players) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x74,iStack28 + 0x3c,0x83,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0x8d;
  if (highlight_scenario) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x8c,iStack28 + 0x3c,0x9b,0x32);
  }""",
"""  x = 0x39;
  y = 0x87;
  if (highlight_scenario) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x86,iStack28 + 0x3c,0x95,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0xa1;
  if (highlight_gameselect) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xa0,iStack28 + 0x3c,0xaf,0x32);
  }""",
"""  x = 0x39;
  y = 0x99;
  if (highlight_gameselect) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x98,iStack28 + 0x3c,0xa7,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0xb5;
  if (highlight_gamelength) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xb4,iStack28 + 0x3c,0xc3,0x32);
  }""",
"""  x = 0x39;
  y = 0xab;
  if (highlight_gamelength) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xaa,iStack28 + 0x3c,0xb9,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0xc9;
  if (highlight_weaponselect) {
    DL = microcode_constructor_related_to_menus(DL,0x37,200,iStack28 + 0x3c,0xd7,0x32);
  }""",
"""  x = 0x39;
  y = 0xbd;
  if (highlight_weaponselect) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xbc,iStack28 + 0x3c,0xcb,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0xdd;
  if (highlight_character) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xdc,iStack28 + 0x3c,0xeb,0x32);
  }""",
"""  x = 0x39;
  y = 0xcf;
  if (highlight_character) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xce,iStack28 + 0x3c,0xdd,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0xf1;
  if (highlight_health) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xf0,iStack28 + 0x3c,0xff,0x32);
  }""",
"""  x = 0x39;
  y = 0xe1;
  if (highlight_health) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xe0,iStack28 + 0x3c,0xef,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0x105;
  if (highlight_controlstyle) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x104,iStack28 + 0x3c,0x113,0x32);
  }""",
"""  x = 0x39;
  y = 0xf3;
  if (highlight_controlstyle) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0xf2,iStack28 + 0x3c,0x101,0x32);
  }""")
    text=_replace_once(text,
"""  x = 0x39;
  y = 0x119;
  if (highlight_aimadjustment) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x118,iStack28 + 0x3c,0x127,0x32);
  }
  if (unlock_aim_sight) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,entry,viGetX(),viGetY(),0,0);""",
"""  x = 0x39;
  y = 0x105;
  if (highlight_aimadjustment) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x104,iStack28 + 0x3c,0x113,0x32);
  }
  if (unlock_aim_sight) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,entry,viGetX(),viGetY(),0,0);

  text = (u8 *)native_mp_bot_label();
  textMeasure(&iStack24,&iStack28,text,ptrFontZurichBoldChars,ptrFontZurichBold,0);
  x = 0x39;
  y = 0x117;
  if (highlight_bots) {
    DL = microcode_constructor_related_to_menus(DL,0x37,0x116,iStack28 + 0x3c,0x125,0x32);
  }
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,0xff,viGetX(),viGetY(),0,0);""")
    text=_replace_once(text,
"""  sprintf(acStack12,"%d",selected_num_players);
  x = 0xa0;
  y = 0x79;
  DL = frontPrintText(DL, &x, &y, acStack12, ptrFontZurichBoldChars, ptrFontZurichBold, 0xff, viGetX(), viGetY(), 0, 0);

  text = langGet(mp_player_counts[scenario].stage);
  x = 0xa0;
  y = 0x8d;
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,0xff,viGetX(),viGetY(),0,0);

  text = langGet(multi_stage_setups[MP_stage_selected].folder_text_preset);
  x = 0xa0;
  y = 0xa1;""",
"""  sprintf(acStack12,"%d",selected_num_players);
  x = 0xa0;
  y = 0x75;
  DL = frontPrintText(DL, &x, &y, acStack12, ptrFontZurichBoldChars, ptrFontZurichBold, 0xff, viGetX(), viGetY(), 0, 0);

  text = langGet(mp_player_counts[scenario].stage);
  x = 0xa0;
  y = 0x87;
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,0xff,viGetX(),viGetY(),0,0);

  text = langGet(multi_stage_setups[MP_stage_selected].folder_text_preset);
  x = 0xa0;
  y = 0x99;""")
    text=_replace_once(text,
"""  text = langGet(multi_game_lengths[game_length].text_preset);
  x = 0xa0;
  y = 0xb5;""",
"""  text = langGet(multi_game_lengths[game_length].text_preset);
  x = 0xa0;
  y = 0xab;""")
    text=_replace_once(text,
"""  text = langGet(*(getPtrMPWeaponSetTextID()));
  x = 0xa0;
  y = 0xc9;""",
"""  text = langGet(*(getPtrMPWeaponSetTextID()));
  x = 0xa0;
  y = 0xbd;""")
    text=_replace_once(text,
"""  text = langGet(mp_sight_adjust_table[aim_sight_adjustment].anonymous_0);
  x = 0xa0;
  y = 0x119;
  if (unlock_aim_sight) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,entry,viGetX(),viGetY(),0,0);""",
"""  text = langGet(mp_sight_adjust_table[aim_sight_adjustment].anonymous_0);
  x = 0xa0;
  y = 0x105;
  if (unlock_aim_sight) {
    entry = 0xFF;
  }
  else {
    entry = 0x70;
  }
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,entry,viGetX(),viGetY(),0,0);

  text = (u8 *)native_mp_bot_value_text();
  x = 0xa0;
  y = 0x117;
  DL = frontPrintText(DL,&x,&y,text,ptrFontZurichBoldChars,ptrFontZurichBold,0xff,viGetX(),viGetY(),0,0);""")
    return text
