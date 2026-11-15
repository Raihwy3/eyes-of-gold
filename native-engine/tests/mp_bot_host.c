#include <ultra64.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "game/player.h"
#include "game/gun.h"
#include "game/bondview.h"
#include "game/stan.h"
#include "game/mpmenu.h"
#include "bondconstants.h"
#include "native_mp_bot.h"
#include "native_settings.h"

#undef assert
#define test_assert(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); abort(); } } while (0)

/* Minimal host stubs for native_mp_bot.c AI & menu/respawn verification. */
struct player *g_CurrentPlayer;
struct player *g_playerPointers[4];
PadRecord *g_Startpad[0x10];
s32 startpadcount;
f32 g_GlobalTimerDelta = 1.0f;

static s32 mock_cur_player;
static s32 mock_player_count = 2;
static int mock_demo_active;
static s32 mock_controls_locked;
static s32 mock_actions_enabled = 1;
static s32 mock_weapon_id = ITEM_AK47;
static s32 mock_mag_ammo = 30;
static s32 mock_ammo_by_item[64];
static s8 mock_auto_rate = 3;
static int mock_reload_calls;
static ITEM_IDS mock_next_in_cycle[2] = { ITEM_AK47, ITEM_UNARMED };
static ITEM_IDS mock_requested_weapon[2] = { ITEM_NOTHING, ITEM_NOTHING };
static int mock_weapon_change_calls;
static struct PropRecord *mock_active_tail;
static int mock_los_clear = 1;
static int mock_wall_ahead;
static int mock_closed_door_ahead;
static struct StandTile mock_stan_a, mock_stan_b;

s32 get_cur_playernum(void) { return mock_cur_player; }
s32 getPlayerCount(void) { return mock_player_count; }
s32 lvlGetControlsLockedFlag(void) { return mock_controls_locked; }
s32 disablePlayerActionsWhenPausedOrInMpMenu(void) { return mock_actions_enabled; }
int native_demo_active(void) { return mock_demo_active; }
s32 getCurrentPlayerWeaponId(GUNHAND hand) { return hand == GUNRIGHT ? mock_weapon_id : ITEM_UNARMED; }
s32 get_ammo_in_hands_magazine(GUNHAND hand) { (void)hand; return mock_mag_ammo; }
s32 bondwalkItemHasAmmo(ITEM_IDS item) {
    if (item == ITEM_FIST || item == ITEM_KNIFE) return 1;
    if (item >= 0 && item < 64) return mock_ammo_by_item[item];
    return 0;
}
s8 bondwalkItemGetAutomaticFiringRate(ITEM_IDS item) { (void)item; return mock_auto_rate; }
void attempt_reload_item_in_hand(GUNHAND hand) { (void)hand; mock_reload_calls++; }
ITEM_IDS get_next_weapon_in_cycle_for_hand(GUNHAND hand, s32 direction) {
    (void)direction;
    return mock_next_in_cycle[hand == GUNRIGHT ? 0 : 1];
}
void gunRequestHandWeaponChange(enum GUNHAND hand, s32 nextWeapon, s32 cycleDirection) {
    (void)cycleDirection;
    int idx = (hand == GUNRIGHT) ? 0 : 1;
    mock_requested_weapon[idx] = (ITEM_IDS)nextWeapon;
    mock_next_in_cycle[idx] = (ITEM_IDS)nextWeapon;
    mock_weapon_change_calls++;
}
struct PropRecord *chrpropGetActiveTail(void) { return mock_active_tail; }

s32 stanTestLineUnobstructed(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z,
                             s32 cdtypes, f32 unkHeight, f32 unkA, f32 unkB, f32 unkC) {
    (void)unkHeight; (void)unkA; (void)unkB; (void)unkC;
    if (cdtypes == CDTYPE_CLOSEDDOORS) {
        return mock_closed_door_ahead ? 0 : 1;
    }
    if ((cdtypes & CDTYPE_AIOPAQUE) != 0) {
        if (mock_los_clear && pTile) *pTile = &mock_stan_b;
        return mock_los_clear ? 1 : 0;
    }
    if (mock_wall_ahead) {
        float dx = dest_x - p_x;
        float dz = dest_z - p_z;
        if (dz > fabsf(dx) * 0.5f) return 0;
    }
    return 1;
}

int main(void) {
    /* 1. Verify settings persistence for mp_bots. */
    char dir[] = "/tmp/eog-bot-test-XXXXXX";
    test_assert(mkdtemp(dir) != NULL);
    native_settings_init(dir);
    test_assert(native_setting_mp_bots() == 0);
    test_assert(!native_mp_bot_enabled());
    test_assert(!strcmp(native_mp_bot_value_text(), "Off"));
    native_mp_bot_toggle();
    test_assert(native_setting_mp_bots() == 1);
    test_assert(!strcmp(native_mp_bot_value_text(), "On"));
    native_settings_init(dir);
    test_assert(native_setting_mp_bots() == 1);

    /* 2. Predicates: P1 never bot, P2 bot only when enabled & >=2 players & !demo. */
    test_assert(!native_mp_bot_is_active(0, 2, 0));
    test_assert(native_mp_bot_is_active(1, 2, 0));
    test_assert(!native_mp_bot_is_active(1, 1, 0));
    test_assert(!native_mp_bot_is_active(1, 2, 1));
    native_setting_set_mp_bots(0);
    test_assert(!native_mp_bot_is_active(1, 2, 0));
    native_setting_set_mp_bots(1);

    /* 3. Auto-respawn gating (including YOLT 2-life limit). */
    test_assert(!native_mp_bot_should_respawn(0, 2, 0, SCENARIO_NORMAL, 0));
    test_assert(native_mp_bot_should_respawn(1, 2, 0, SCENARIO_NORMAL, 5));
    test_assert(native_mp_bot_should_respawn(1, 2, 0, SCENARIO_YOLT, 1));
    test_assert(!native_mp_bot_should_respawn(1, 2, 0, SCENARIO_YOLT, 2));

    /* 4. Auto-accept match end & quit / pause closing (order-independent P1 vs P2). */
    test_assert(native_mp_bot_menu_action(0, 2, 0, 1, 1, MENU_SCORES) == NATIVE_MP_BOT_MENU_NONE);
    test_assert(native_mp_bot_menu_action(1, 2, 0, 1, 1, MENU_SCORES) == NATIVE_MP_BOT_MENU_FINISH_GAME);
    test_assert(native_mp_bot_menu_action(1, 2, 0, 1, 1, MENU_FINISHED) == NATIVE_MP_BOT_MENU_NONE);
    test_assert(native_mp_bot_menu_action(1, 2, 0, 1, 1, MENU_EXIT_CONFIRM) == NATIVE_MP_BOT_MENU_CONFIRM_QUIT);
    test_assert(native_mp_bot_menu_action(1, 2, 0, 0, 1, MENU_PAUSE) == NATIVE_MP_BOT_MENU_CLOSE_PAUSE);
    test_assert(native_mp_bot_menu_action(1, 2, 0, 2, 1, MENU_SCORES) == NATIVE_MP_BOT_MENU_NONE);

    /* 5. Real combat AI via native_mp_bot_apply. */
    struct player p1, p2;
    struct PropRecord prop1, prop2;
    memset(&p1, 0, sizeof p1);
    memset(&p2, 0, sizeof p2);
    memset(&prop1, 0, sizeof prop1);
    memset(&prop2, 0, sizeof prop2);
    p1.prop = &prop1; p2.prop = &prop2;
    p1.bondhealth = 1.0f; p2.bondhealth = 1.0f;
    prop1.stan = &mock_stan_b; prop2.stan = &mock_stan_a;
    g_playerPointers[0] = &p1;
    g_playerPointers[1] = &p2;
    native_mp_bot_reset();

    /* Place enemy ahead-left (atan2f(-(-250), 500) > 0) and elevated (+120). */
    prop2.pos.x = 0.0f; prop2.pos.y = 0.0f; prop2.pos.z = 0.0f;
    prop1.pos.x = -250.0f; prop1.pos.y = 120.0f; prop1.pos.z = 500.0f;
    p2.vv_theta = 0.0f;
    p2.vv_verta = 0.0f;

    struct MoveData m;
    memset(&m, 0, sizeof m);
    mock_cur_player = 1;
    mock_ammo_by_item[ITEM_AK47] = 1;
    g_CurrentPlayer = &p2;
    native_mp_bot_apply(&m);
    test_assert(m.invertPitch == 1 && m.canNaturalTurn == 1 && m.canNaturalPitch == 1);
    test_assert(m.analogTurn > 0);  /* Turns toward atan2f(+250, 500) > 0 */
    test_assert(m.analogPitch < 0); /* Pitches up because invertPitch=1, enemy is higher */
    test_assert(m.analogWalk > 0);  /* Advances toward distant target */

    /* Align directly with target and confirm bot pulls trigger. */
    p2.vv_theta = atan2f(250.0f, 500.0f) * (180.0f / 3.14159265f);
    p2.vv_verta = atan2f(108.0f, sqrtf(250.0f*250.0f + 500.0f*500.0f)) * (180.0f / 3.14159265f);
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(m.triggerOn == 1);

    /* Semi-auto firing toggles off immediately after firing instead of jamming trigger. */
    mock_auto_rate = -1;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(m.triggerOn == 0);

    /* Empty magazine with reserve ammo reloads instead of shooting. */
    mock_mag_ammo = 0; mock_ammo_by_item[ITEM_AK47] = 1; mock_reload_calls = 0;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(mock_reload_calls == 1 && m.triggerOn == 0);

    /* 6. Direct inventory weapon switching (away from ITEM_UNARMED, ITEM_FIST, or empty guns). */
    InvItem inv_fist, inv_ak, inv_pp7;
    memset(&inv_fist, 0, sizeof inv_fist);
    memset(&inv_ak, 0, sizeof inv_ak);
    memset(&inv_pp7, 0, sizeof inv_pp7);
    inv_fist.type = INV_ITEM_WEAPON;
    inv_fist.type_inv_item.type_weap.weapon = ITEM_FIST;
    inv_ak.type = INV_ITEM_WEAPON;
    inv_ak.type_inv_item.type_weap.weapon = ITEM_AK47;
    inv_pp7.type = INV_ITEM_WEAPON;
    inv_pp7.type_inv_item.type_weap.weapon = ITEM_WPPK;

    /* Circular list: FIST -> AK47 -> PP7 -> FIST. */
    inv_fist.next = &inv_ak; inv_ak.next = &inv_pp7; inv_pp7.next = &inv_fist;
    inv_fist.prev = &inv_pp7; inv_ak.prev = &inv_fist; inv_pp7.prev = &inv_ak;
    p2.ptr_inventory_first_in_cycle = &inv_fist;

    /* Holding ITEM_UNARMED jumps straight to ITEM_AK47 (first firearm with ammo), skipping ITEM_FIST. */
    native_mp_bot_reset();
    mock_weapon_id = ITEM_UNARMED;
    mock_mag_ammo = 0;
    mock_ammo_by_item[ITEM_AK47] = 1;
    mock_ammo_by_item[ITEM_WPPK] = 1;
    mock_next_in_cycle[0] = ITEM_UNARMED;
    mock_requested_weapon[0] = ITEM_NOTHING;
    mock_weapon_change_calls = 0;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(mock_weapon_change_calls >= 1);
    test_assert(mock_requested_weapon[0] == ITEM_AK47);

    /* Holding ITEM_FIST (Slappers) switches directly to ITEM_AK47 when firearms have ammo. */
    native_mp_bot_reset();
    mock_weapon_id = ITEM_FIST;
    mock_next_in_cycle[0] = ITEM_FIST;
    mock_requested_weapon[0] = ITEM_NOTHING;
    mock_weapon_change_calls = 0;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(mock_requested_weapon[0] == ITEM_AK47);
    test_assert(m.triggerOn == 0); /* Does not punch from across arena while awaiting switch */

    /* Holding an empty firearm switches directly to another loaded firearm (ITEM_WPPK). */
    native_mp_bot_reset();
    mock_weapon_id = ITEM_AK47;
    mock_mag_ammo = 0;
    mock_ammo_by_item[ITEM_AK47] = 0;
    mock_ammo_by_item[ITEM_WPPK] = 1;
    mock_next_in_cycle[0] = ITEM_AK47;
    mock_requested_weapon[0] = ITEM_NOTHING;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(mock_requested_weapon[0] == ITEM_WPPK);

    /* Only ITEM_FIST in inventory: equips ITEM_FIST from ITEM_UNARMED AND steers to nearby floor weapon pickup. */
    inv_fist.next = &inv_fist; inv_fist.prev = &inv_fist;
    struct PropRecord pickup_prop;
    struct WeaponObjRecord pickup_obj;
    memset(&pickup_prop, 0, sizeof pickup_prop);
    memset(&pickup_obj, 0, sizeof pickup_obj);
    pickup_obj.type = PROPDEF_COLLECTABLE;
    pickup_obj.weaponnum = ITEM_AK47;
    pickup_prop.type = PROP_TYPE_WEAPON;
    pickup_prop.obj = (struct ObjectRecord *)&pickup_obj;
    pickup_prop.stan = &mock_stan_a;
    pickup_prop.pos.x = 300.0f; pickup_prop.pos.y = 0.0f; pickup_prop.pos.z = 0.0f;
    mock_active_tail = &pickup_prop;

    native_mp_bot_reset();
    prop2.pos.x = 0.0f; prop2.pos.y = 0.0f; prop2.pos.z = 0.0f;
    prop1.pos.x = -800.0f; prop1.pos.y = 0.0f; prop1.pos.z = 0.0f; /* Far enemy */
    p2.vv_theta = 270.0f; /* Facing +X toward pickup */
    p2.vv_verta = 0.0f;
    mock_weapon_id = ITEM_UNARMED;
    mock_mag_ammo = 0;
    mock_next_in_cycle[0] = ITEM_UNARMED;
    mock_requested_weapon[0] = ITEM_NOTHING;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(mock_requested_weapon[0] == ITEM_FIST);
    test_assert(m.analogWalk > 20); /* Runs toward +X floor weapon pickup */

    /* Restore combat test defaults before wall/door tests. */
    mock_active_tail = NULL;
    p2.ptr_inventory_first_in_cycle = NULL;
    mock_weapon_id = ITEM_AK47;
    mock_ammo_by_item[ITEM_AK47] = 1;
    mock_mag_ammo = 30;
    mock_next_in_cycle[0] = ITEM_AK47;
    prop2.pos.x = 0.0f; prop2.pos.y = 0.0f; prop2.pos.z = 0.0f;
    prop1.pos.x = -250.0f; prop1.pos.y = 120.0f; prop1.pos.z = 500.0f;
    p2.vv_theta = 0.0f; p2.vv_verta = 0.0f;

    /* Closed door ahead triggers btap when moving. */
    mock_mag_ammo = 30; mock_auto_rate = 3;
    mock_closed_door_ahead = 1;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(m.btap == 1);
    mock_closed_door_ahead = 0;

    /* Wall ahead during search triggers obstacle detour steering. */
    mock_los_clear = 0; mock_wall_ahead = 1;
    prop1.pos.x = 0.0f; prop1.pos.y = 0.0f; prop1.pos.z = 800.0f;
    p2.vv_theta = 0.0f; p2.vv_verta = 0.0f;
    memset(&m, 0, sizeof m);
    native_mp_bot_apply(&m);
    test_assert(m.analogStrafe != 0 || m.analogWalk < 0);

    /* P1 human slot is untouched by native_mp_bot_apply. */
    mock_cur_player = 0;
    g_CurrentPlayer = &p1;
    memset(&m, 0, sizeof m);
    m.analogWalk = 42;
    native_mp_bot_apply(&m);
    test_assert(m.analogWalk == 42);

    /* Remove temp dir files. */
    char path[512];
    snprintf(path, sizeof path, "%s/native-options.ini", dir);
    remove(path);
    rmdir(dir);

    puts("PASS MP bot host unit suite: settings persistence, predicates, YOLT respawn, menu auto-accept, aiming, semi/auto trigger, reload, door btap, and obstacle detour");
    return 0;
}
