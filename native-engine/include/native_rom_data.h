#ifndef NATIVE_ROM_DATA_H
#define NATIVE_ROM_DATA_H
/* Startup-only. Original initialized data is populated before engine workers.
 */
int native_rom_data_init(const char *directory);
const char *native_rom_data_error(void);
unsigned native_rom_data_count(void);
#endif
