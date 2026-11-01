"""Keep render-authored title effects on the original elapsed VI timebase."""
def apply(text):
    text='#include <ultra64.h>\nextern s32 g_ClockTimer;\n'+text
    def replace(old,new):
        nonlocal text
        assert text.count(old)==1,old
        text=text.replace(old,new)
    replace('return sub_GAME_7F007F30(gdl, 2, &matrix);','return sub_GAME_7F007F30(gdl, 2*g_ClockTimer, &matrix);')
    replace('return sub_GAME_7F007F30(gdl, 1, &matrix);','return sub_GAME_7F007F30(gdl, g_ClockTimer, &matrix);')
    replace('D_8002A89C += 2.0f;','D_8002A89C += 2.0f*g_ClockTimer;')
    replace('D_8002A89C += 2.4f;','D_8002A89C += 2.4f*g_ClockTimer;')
    replace('if (intro_eye_counter++ >= RAREWARE_LOGO_EYE_COUNT1)', 'if ((intro_eye_counter += g_ClockTimer) > RAREWARE_LOGO_EYE_COUNT1)')
    start=text.index('Gfx *renderGunbarrelEyeIntroSequence (Gfx *gdl) {')
    end=text.index('\nbool ',start) if '\nbool ' in text[start:] else len(text)
    body=text[start:end]
    body=body.replace('g_TitleX += XINC;','g_TitleX += XINC*g_ClockTimer;').replace('g_TitleX -= XDEC3;','g_TitleX -= XDEC3*g_ClockTimer;')
    body=body.replace('word_CODE_bss_80069584 -= 7;','word_CODE_bss_80069584 -= 7*g_ClockTimer;').replace('word_CODE_bss_80069584 -= 6;','word_CODE_bss_80069584 -= 6*g_ClockTimer;')
    body=body.replace('word_CODE_bss_80069584 += INCVAL;','word_CODE_bss_80069584 += INCVAL*g_ClockTimer;')
    body=body.replace('intro_eye_counter--;','intro_eye_counter -= g_ClockTimer;').replace('intro_eye_counter++;','intro_eye_counter += g_ClockTimer;')
    body=body.replace('intro_eye_counter += INTRO_EYE_COUNTER_CASE_5_ADD;','intro_eye_counter += INTRO_EYE_COUNTER_CASE_5_ADD*g_ClockTimer;')
    body=body.replace('if (intro_eye_counter == 0) {','while (intro_eye_counter <= 0) {').replace('            intro_eye_counter = 2;','            intro_eye_counter += 2;\n            if(intro_state_blood_animation)break;')
    body=body.replace('if (intro_eye_counter++ >= INTRO_EYE_COUNTER_CASE_6)', 'if ((intro_eye_counter += g_ClockTimer) > INTRO_EYE_COUNTER_CASE_6)')
    text=text[:start]+body+text[end:]
    return text
