#ifndef EOG_NATIVE_SETTINGS_H
#define EOG_NATIVE_SETTINGS_H
#ifdef __cplusplus
extern "C" {
#endif
int native_setting_randomiser(void);
void native_setting_set_randomiser(int value);
int native_setting_randomiser_seed(void);
void native_setting_set_randomiser_seed(int value);
const char *native_randomiser_label(void);
const char *native_randomiser_seed_label(void);
const char *native_randomiser_seed_text(void);
int native_setting_subsistence(void);
void native_setting_set_subsistence(int value);
const char *native_subsistence_label(void);
void native_settings_init(const char *directory);
int native_settings_apply_console_defaults(int model);
int native_setting_guard_visibility(void);
void native_setting_set_guard_visibility(int value);
int native_setting_guard_ai_hz(void);
void native_setting_set_guard_ai_hz(int value);
int native_setting_audio_mixer(void);
void native_setting_set_audio_mixer(int value);
int native_setting_style(void);
int native_setting_crosshair(void);
int native_setting_watch(void);
int native_setting_aspect(void);
int native_setting_aa(void);
int native_setting_fps(void);
int native_setting_sensitivity(void);
int native_setting_sharpness(void);
int native_setting_vignette(void);
int native_setting_bloom(void);
int native_setting_grain(void);
int native_setting_skin(void);
int native_setting_glass(void);
int native_setting_lod(void);
int native_setting_lod_strength(void);
void native_setting_dynamic_lod(int value);
unsigned native_setting_render_state(void);
void native_setting_set_render_state(unsigned state);
int native_setting_vignette_mode(void);
void native_setting_set_vignette_mode(int value);
int native_setting_view_distance(void);
void native_setting_set_lod(int value);
void native_setting_set_view_distance(int value);
void native_setting_set_glass(int value);
void native_setting_set_skin(int value);
void native_setting_set_grain(int value);
void native_setting_set_vignette(int value);
void native_setting_set_bloom(int value);
void native_setting_set_sharpness(int value);
void native_setting_set_sensitivity(int value);
void native_setting_set_fps(int value);
int native_setting_unlock_all(void);
void native_setting_set_unlock_all(int value);
int native_setting_game(int index);
void native_setting_set_game(int index,int value);
int native_setting_volume(int effects);
void native_setting_set_volume(int effects,int value);
void native_setting_set_style(int value);
void native_setting_set_crosshair(int value);
void native_setting_set_watch(int value);
void native_setting_set_aspect(int value);
void native_setting_set_aa(int value);
const char *native_string(unsigned id);
const char *native_colour_name(int watch,int value);
#ifdef __cplusplus
}
#endif
#endif
