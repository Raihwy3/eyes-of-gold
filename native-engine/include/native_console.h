#ifndef EOG_NATIVE_CONSOLE_H
#define EOG_NATIVE_CONSOLE_H
/* libctru CFG model identifiers. Kept portable for first-launch tests. */
static inline int native_console_is_new(int model) {
    return model==2 || model==4 || model==5;
}
static inline const char *native_console_name(int model) {
    static const char *const names[]={"Nintendo 3DS","Nintendo 3DS XL","New Nintendo 3DS",
      "Nintendo 2DS","New Nintendo 3DS XL","New Nintendo 2DS XL"};
    return model>=0 && model<6?names[model]:"Unknown 3DS model";
}
#endif
