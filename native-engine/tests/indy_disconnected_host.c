#include <ultra64.h>
#include "game/indy_comms.h"
#include <stdlib.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) abort(); } while (0)
extern s32 indy_ready;
/* Any attempted workstation transfer fails this regression check. */
void osWriteHost(void *buffer, u32 size) { (void)buffer; (void)size; abort(); }
void osReadHost(void *buffer, u32 size) { (void)buffer; (void)size; abort(); }
int main(void) {
    u8 data[16] = {0};
    s32 size = 123;
    indy_ready = 1;
    CHECK(indycommInit() == 0);
    CHECK(indy_ready == 0);
    indycommHostinit();
    indycommHostLoadFile("absent", data);
    indycommHostSendDump("absent", data, sizeof(data));
    indycommHostRamRomLoad("absent", data, sizeof(data));
    indycommHostSaveFile("absent", sizeof(data), data);
    CHECK(indycommHostCheckFileExists("absent", &size) == NULL);
    CHECK(size == 123);
    CHECK(indycommHostSendCmd(data) == NULL);
    CHECK(indycommInit() == 0);
    CHECK(indy_ready == 0);
    puts("PASS disconnected Indy boot and guarded host operations send no transfers");
    return 0;
}
