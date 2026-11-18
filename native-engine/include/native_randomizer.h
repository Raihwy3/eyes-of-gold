#ifndef EOG_NATIVE_RANDOMIZER_H
#define EOG_NATIVE_RANDOMIZER_H
/* Campaign setup only; no per-frame work or changes to the game RNG. */
void native_randomizer_setup(int stage);
void native_randomizer_story_appearance(int character,int *body,int *head);
#endif
