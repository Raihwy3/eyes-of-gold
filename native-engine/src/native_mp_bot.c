#include <ultra64.h>
#include <math.h>
#include <string.h>
#include "game/player.h"
#include "game/gun.h"
#include "game/bondview.h"
#include "game/stan.h"
#include "game/lv.h"
#include "game/mpmenu.h"
#include "bondconstants.h"
#include "native_mp_bot.h"
#include "native_settings.h"
#include "native_demo.h"

extern PadRecord *g_Startpad[];
extern s32 startpadcount;

extern f32 g_GlobalTimerDelta;
extern s32 getCurrentPlayerWeaponId(GUNHAND hand);
extern s32 get_ammo_in_hands_magazine(GUNHAND hand);
extern void attempt_reload_item_in_hand(GUNHAND hand);
extern s32 lvlGetControlsLockedFlag(void);
extern ITEM_IDS get_next_weapon_in_cycle_for_hand(GUNHAND hand, s32 direction);
extern void gunRequestHandWeaponChange(enum GUNHAND hand, s32 nextWeapon, s32 cycleDirection);
extern struct PropRecord *chrpropGetActiveTail(void);

#define BOT_MAX_PLAYERS 4
#define BOT_PI 3.14159265358979323846f
#define BOT_RAD_TO_DEG (180.0f / BOT_PI)
#define BOT_DEG_TO_RAD (BOT_PI / 180.0f)

struct mp_bot_state {
    u32 rng;
    int initialized;
    float strafe_timer;
    float strafe_dir;
    float fire_timer;
    int fire_burst_on;
    int semi_toggle;
    float switch_cooldown;
    float btap_cooldown;
    float los_lost_time;
    int last_known_valid;
    float last_known_x;
    float last_known_y;
    float last_known_z;
    int patrol_idx;
    float patrol_timer;
    float stuck_timer;
    float last_x;
    float last_z;
    float detour_timer;
    float detour_x;
    float detour_z;
    float jitter_timer;
    float aim_jitter_yaw;
    float aim_jitter_pitch;
    float prev_health;
    float alert_timer;
    float seen_time;
    float crouch_timer;
    float strafe_phase;
};

static struct mp_bot_state bot_states[BOT_MAX_PLAYERS];

static u32 bot_rand(struct mp_bot_state *st) {
    u32 x = st->rng;
    if (!x) x = 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    st->rng = x;
    return x;
}

static float bot_randf(struct mp_bot_state *st) {
    return (float)(bot_rand(st) & 0xFFFFu) / 65535.0f;
}

static float bot_wrap_deg(float deg) {
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

static float bot_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static int bot_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Encode desired linear rate in [-1, 1] for bondview2's quadratic analog curve. */
static int bot_encode_quadratic_axis(float normalized_rate) {
    float c = bot_clampf(normalized_rate, -1.0f, 1.0f);
    float mag = sqrtf(fabsf(c)) * 70.0f;
    int iv = (int)(c < 0.0f ? -mag - 0.5f : mag + 0.5f);
    return bot_clampi(iv, -70, 70);
}

static int bot_check_los(struct PropRecord *src, struct PropRecord *dst) {
    if (!src || !dst) return 0;
    if (!src->stan || !dst->stan) return 1;
    struct StandTile *tile = src->stan;
    s32 clear = stanTestLineUnobstructed(
        &tile,
        src->pos.x, src->pos.z,
        dst->pos.x, dst->pos.z,
        CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE,
        0.0f, 1.0f, 0.0f, 1.0f);
    return clear != 0 && tile == dst->stan;
}

static int bot_probe_clear(struct PropRecord *src, float dir_x, float dir_z, float dist) {
    if (!src || !src->stan) return 1;
    struct StandTile *tile = src->stan;
    return stanTestLineUnobstructed(
        &tile,
        src->pos.x, src->pos.z,
        src->pos.x + dir_x * dist,
        src->pos.z + dir_z * dist,
        CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER,
        0.0f, 1.0f, 0.0f, 1.0f) != 0;
}

static int bot_closed_door_ahead(struct PropRecord *src, float dir_x, float dir_z, float dist) {
    if (!src || !src->stan) return 0;
    struct StandTile *tile = src->stan;
    return stanTestLineUnobstructed(
        &tile,
        src->pos.x, src->pos.z,
        src->pos.x + dir_x * dist,
        src->pos.z + dir_z * dist,
        CDTYPE_CLOSEDDOORS,
        0.0f, 1.0f, 0.0f, 1.0f) == 0;
}

static void bot_pick_open_direction(struct PropRecord *src, struct mp_bot_state *st,
                                    float pref_x, float pref_z, float *out_x, float *out_z) {
    static const float offsets_rad[7] = {
        0.785398f, -0.785398f, 1.570796f, -1.570796f, 2.356194f, -2.356194f, 3.141593f
    };
    int start = (bot_rand(st) & 1u) ? 0 : 1;
    float base = atan2f(pref_z, pref_x);
    for (int i = 0; i < 7; i++) {
        int idx = (i & ~1) | ((i ^ start) & 1);
        if (idx >= 7) idx = 6;
        float ang = base + offsets_rad[idx];
        float tx = cosf(ang), tz = sinf(ang);
        if (bot_probe_clear(src, tx, tz, 130.0f)) {
            *out_x = tx;
            *out_z = tz;
            return;
        }
    }
    *out_x = -pref_x;
    *out_z = -pref_z;
}

static int bot_weapon_usable(ITEM_IDS w, ITEM_IDS cur_r, int cur_mag, int equip_all) {
    if (w <= ITEM_UNARMED || w >= ITEM_BOMBCASE) return 0;
    if (w == ITEM_FIST || w == ITEM_KNIFE) return 1;
    if (equip_all) return 1;
    if (w == cur_r && cur_mag > 0) return 1;
    return bondwalkItemHasAmmo(w) != 0;
}

static int bot_weapon_score(ITEM_IDS w, int is_dual, ITEM_IDS cur_r, int cur_mag, int equip_all) {
    if (!bot_weapon_usable(w, cur_r, cur_mag, equip_all)) return 0;
    if (w == ITEM_FIST) return 10;
    if (w == ITEM_KNIFE) return 20;
    if (w == ITEM_THROWKNIFE) return 35;
    if (w == ITEM_GRENADE || w == ITEM_REMOTEMINE ||
        w == ITEM_PROXIMITYMINE || w == ITEM_TIMEDMINE) {
        return 45;
    }
    int score = 100 + (is_dual ? 40 : 0);
    if (w == cur_r && cur_mag > 0) score += 10;
    return score;
}

static void bot_pick_best_inventory_weapon(struct player *me, ITEM_IDS cur_r, int cur_mag,
                                           ITEM_IDS *out_r, ITEM_IDS *out_l, int *out_score) {
    *out_r = ITEM_UNARMED;
    *out_l = ITEM_UNARMED;
    *out_score = 0;
    if (!me) return;

    struct InvItem *first = me->ptr_inventory_first_in_cycle;
    if (!first) return;

    int equip_all = me->equipallguns != 0;
    struct InvItem *it = first;
    int steps = 0;
    do {
        ITEM_IDS rw = ITEM_UNARMED;
        ITEM_IDS lw = ITEM_UNARMED;
        int is_dual = 0;
        if (it->type == INV_ITEM_WEAPON) {
            rw = (ITEM_IDS)it->type_inv_item.type_weap.weapon;
        } else if (it->type == INV_ITEM_DUAL) {
            rw = (ITEM_IDS)it->type_inv_item.type_dual.weapon_right;
            lw = (ITEM_IDS)it->type_inv_item.type_dual.weapon_left;
            is_dual = bot_weapon_usable(lw, cur_r, cur_mag, equip_all);
            if (!is_dual) lw = ITEM_UNARMED;
        }
        int s = bot_weapon_score(rw, is_dual, cur_r, cur_mag, equip_all);
        if (s > *out_score) {
            *out_score = s;
            *out_r = rw;
            *out_l = lw;
        }
        it = it->next;
    } while (it && it != first && ++steps < 64);
}

static int bot_find_nearest_firearm_pickup(struct PropRecord *src, float *out_x, float *out_z) {
    if (!src) return 0;
    struct PropRecord *p = chrpropGetActiveTail();
    float best_d2 = 1e30f;
    int found = 0;
    int guard = 0;

    while (p && guard++ < 512) {
        if ((p->type == PROP_TYPE_OBJ || p->type == PROP_TYPE_WEAPON) &&
            p->timetoregen == 0 && p->obj != NULL && p->stan != NULL &&
            p->obj->type == PROPDEF_COLLECTABLE &&
            !(p->obj->flags & (PROPFLAG_UNCOLLECTABLE | PROPFLAG_00080000)) &&
            !(p->obj->runtime_bitflags & RUNTIMEBITFLAG_DESTROYED)) {
            struct WeaponObjRecord *wep = (struct WeaponObjRecord *)p->obj;
            if (wep->weaponnum > ITEM_THROWKNIFE && wep->weaponnum < ITEM_BOMBCASE) {
                float dx = p->pos.x - src->pos.x;
                float dy = p->pos.y - src->pos.y;
                float dz = p->pos.z - src->pos.z;
                float d2 = dx * dx + dy * dy * 4.0f + dz * dz;
                if (d2 < best_d2) {
                    best_d2 = d2;
                    *out_x = p->pos.x;
                    *out_z = p->pos.z;
                    found = 1;
                }
            }
        }
        p = p->prev;
    }
    return found;
}

int native_mp_bot_is_active(int player_num, int player_count, int demo_active) {
    return native_setting_mp_bots() != 0 && !demo_active && player_count > 1 &&
           player_num >= 1 && player_num < player_count;
}

int native_mp_bot_should_respawn(int player_num, int player_count, int demo_active, int scenario, int deaths) {
    if (!native_mp_bot_is_active(player_num, player_count, demo_active)) return 0;
    if (scenario == SCENARIO_YOLT && deaths >= 2) return 0;
    return 1;
}

int native_mp_bot_menu_action(int player_num, int player_count, int demo_active,
                              int game_over_flag, int menu_on, int menu_mode) {
    if (!native_mp_bot_is_active(player_num, player_count, demo_active)) return NATIVE_MP_BOT_MENU_NONE;
    if (!menu_on) return NATIVE_MP_BOT_MENU_NONE;
    if (game_over_flag == 1) {
        if (menu_mode == MENU_EXIT_CONFIRM) return NATIVE_MP_BOT_MENU_CONFIRM_QUIT;
        if (menu_mode != MENU_FINISHED) return NATIVE_MP_BOT_MENU_FINISH_GAME;
        return NATIVE_MP_BOT_MENU_NONE;
    }
    if (game_over_flag == 0) {
        return NATIVE_MP_BOT_MENU_CLOSE_PAUSE;
    }
    return NATIVE_MP_BOT_MENU_NONE;
}

void native_mp_bot_reset(void) {
    memset(bot_states, 0, sizeof bot_states);
}

void native_mp_bot_apply(struct MoveData *m) {
    if (!m) return;
    int cur = get_cur_playernum();
    int count = getPlayerCount();
    if (!native_mp_bot_is_active(cur, count, native_demo_active())) return;

    struct player *me = g_CurrentPlayer;
    if (cur >= 0 && cur < BOT_MAX_PLAYERS && (!me || me->bonddead)) {
        bot_states[cur].initialized = 0;
    }
    if (!me || me->bonddead || !me->prop ||
        lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu()) {
        m->triggerOn = 0;
        m->analogWalk = 0;
        m->analogStrafe = 0;
        m->analogTurn = 0;
        m->analogPitch = 0;
        return;
    }

    if (cur < 0 || cur >= BOT_MAX_PLAYERS) return;
    struct mp_bot_state *st = &bot_states[cur];
    if (!st->initialized) {
        memset(st, 0, sizeof *st);
        st->rng = 0x9e3779b9u ^ (u32)(cur * 2654435761u);
        st->strafe_dir = (cur & 1) ? 1.0f : -1.0f;
        st->strafe_timer = 0.8f;
        st->fire_burst_on = 1;
        st->fire_timer = 0.45f;
        st->last_x = me->prop->pos.x;
        st->last_z = me->prop->pos.z;
        st->prev_health = me->bondhealth;
        st->initialized = 1;
    }

    float dt = g_GlobalTimerDelta > 0.0f ? (g_GlobalTimerDelta / 60.0f) : (1.0f / 60.0f);
    dt = bot_clampf(dt, 1.0f / 120.0f, 0.1f);

    if (st->switch_cooldown > 0.0f) st->switch_cooldown -= dt;
    if (st->btap_cooldown > 0.0f) st->btap_cooldown -= dt;
    if (st->alert_timer > 0.0f) st->alert_timer -= dt;
    if (st->detour_timer > 0.0f) st->detour_timer -= dt;
    if (st->crouch_timer > 0.0f) st->crouch_timer -= dt;
    st->strafe_phase += dt * 1.9f;

    if (me->bondhealth < st->prev_health - 0.001f) {
        st->alert_timer = 2.5f;
    }
    st->prev_health = me->bondhealth;

    /* Reset human input fields before synthesizing bot actions. */
    m->analogWalk = 0;
    m->analogStrafe = 0;
    m->analogPitch = 0;
    m->analogTurn = 0;
    m->digitalStepLeft = 0;
    m->digitalStepRight = 0;
    m->digitalStepForward = 0;
    m->digitalStepBack = 0;
    m->aimTurnLeftSpeed = 0.0f;
    m->aimTurnRightSpeed = 0.0f;
    m->speedVertaUp = 0.0f;
    m->speedVertaDown = 0.0f;
    m->tankTurnLeftSpeed = 0.0f;
    m->tankTurnRightSpeed = 0.0f;
    m->crouchUp = 0;
    m->crouchDown = 0;
    m->zooming = 0;
    m->aiming = 0;
    m->canAutoAim = 0;
    m->canManualAim = 0;
    m->canSwivelGun = 1;
    m->canNaturalPitch = 1;
    m->canNaturalTurn = 1;
    m->canTurnTank = 1;
    m->canLookAhead = 1;
    m->disableLookAhead = 1;
    m->invertPitch = 1;
    m->btap = 0;
    m->triggerOn = 0;
    m->detonating = 0;
    m->weaponForwardOffset = 0;
    m->weaponBackOffset = 0;

    /* Weapon & magazine management: always equip the best usable weapon in inventory. */
    ITEM_IDS item = (ITEM_IDS)getCurrentPlayerWeaponId(GUNRIGHT);
    int mag = get_ammo_in_hands_magazine(GUNRIGHT);
    int has_ammo = (item > ITEM_UNARMED && item < ITEM_BOMBCASE) ? bondwalkItemHasAmmo(item) : 0;
    int equip_all = me->equipallguns != 0;
    int cur_usable = bot_weapon_usable(item, item, mag, equip_all);
    int cur_score = bot_weapon_score(item, 0, item, mag, equip_all);

    ITEM_IDS best_r = ITEM_UNARMED;
    ITEM_IDS best_l = ITEM_UNARMED;
    int best_score = 0;
    bot_pick_best_inventory_weapon(me, item, mag, &best_r, &best_l, &best_score);

    int have_firearm = (cur_score >= 100 || best_score >= 100 || equip_all);
    int need_switch = (item <= ITEM_UNARMED) || !cur_usable ||
                      (cur_score < 100 && (best_score >= 100 || equip_all));
    int need_reload = 0;

    if (need_switch) {
        if (best_r > ITEM_UNARMED) {
            if (st->switch_cooldown <= 0.0f &&
                get_next_weapon_in_cycle_for_hand(GUNRIGHT, 0) != best_r) {
                gunRequestHandWeaponChange(GUNRIGHT, best_r, 1);
                if (get_next_weapon_in_cycle_for_hand(GUNLEFT, 0) != best_l) {
                    gunRequestHandWeaponChange(GUNLEFT, best_l, 1);
                }
                st->switch_cooldown = 0.25f;
            }
        } else if (st->switch_cooldown <= 0.0f) {
            m->weaponForwardOffset = 1;
            st->switch_cooldown = 0.45f;
        }
    } else if (mag == 0 && has_ammo && item > ITEM_THROWKNIFE) {
        attempt_reload_item_in_hand(GUNRIGHT);
        need_reload = 1;
    }

    float pickup_x = 0.0f, pickup_z = 0.0f;
    int have_pickup = (!have_firearm) ? bot_find_nearest_firearm_pickup(me->prop, &pickup_x, &pickup_z) : 0;

    /* Acquire nearest alive enemy player, preferring clear line of sight. */
    struct player *best_tgt = NULL;
    float best_score_tgt = 1e30f;
    float best_d2 = 1e30f;
    int best_los = 0;
    for (int i = 0; i < count && i < BOT_MAX_PLAYERS; i++) {
        if (i == cur) continue;
        struct player *cand = g_playerPointers[i];
        if (!cand || cand->bonddead || !cand->prop || cand->bondhealth <= 0.0f) continue;
        float dx = cand->prop->pos.x - me->prop->pos.x;
        float dy = cand->prop->pos.y - me->prop->pos.y;
        float dz = cand->prop->pos.z - me->prop->pos.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        int los = bot_check_los(me->prop, cand->prop);
        float score = los ? d2 * 0.25f : d2;
        if (!best_tgt || (los && !best_los) || (los == best_los && score < best_score_tgt)) {
            best_tgt = cand;
            best_score_tgt = score;
            best_d2 = d2;
            best_los = los;
        }
    }

    float move_x = 0.0f, move_z = 0.0f, move_speed = 0.0f;
    float desired_theta = me->vv_theta;
    float desired_verta = -4.0f;
    float aim_kp_yaw = 24.0f;
    float aim_kp_pitch = 26.0f;

    int engage_enemy = (best_tgt && best_los) &&
                       (have_firearm || !have_pickup || best_d2 < 260.0f * 260.0f);

    if (engage_enemy) {
        st->seen_time += dt;
        float dx = best_tgt->prop->pos.x - me->prop->pos.x;
        float dy = (best_tgt->prop->pos.y - 12.0f) - me->prop->pos.y;
        float dz = best_tgt->prop->pos.z - me->prop->pos.z;
        float horiz = sqrtf(dx * dx + dz * dz);

        st->last_known_valid = 1;
        st->last_known_x = best_tgt->prop->pos.x;
        st->last_known_y = best_tgt->prop->pos.y - 12.0f;
        st->last_known_z = best_tgt->prop->pos.z;
        st->los_lost_time = 0.0f;

        st->jitter_timer -= dt;
        if (st->jitter_timer <= 0.0f) {
            st->jitter_timer = 0.22f + bot_randf(st) * 0.28f;
            float spread = horiz > 450.0f ? 4.0f : 2.3f;
            st->aim_jitter_yaw = (bot_randf(st) * 2.0f - 1.0f) * spread;
            st->aim_jitter_pitch = (bot_randf(st) * 2.0f - 1.0f) * (spread * 0.55f);
            if (st->crouch_timer <= 0.0f && horiz > 200.0f && horiz < 580.0f &&
                me->bondhealth > 0.4f && (bot_rand(st) & 7u) == 0u) {
                st->crouch_timer = 0.55f + bot_randf(st) * 0.45f;
            }
        }
        if (st->crouch_timer > 0.0f) {
            m->crouchDown = 1;
        }

        desired_theta = atan2f(-dx, dz) * BOT_RAD_TO_DEG + st->aim_jitter_yaw;
        if (desired_theta < 0.0f) desired_theta += 360.0f;
        desired_verta = horiz > 1.0f ? bot_clampf(atan2f(dy, horiz) * BOT_RAD_TO_DEG + st->aim_jitter_pitch, -40.0f, 35.0f) : -4.0f;

        /* Guard-like strafe and combat spacing with smooth lateral modulation. */
        st->strafe_timer -= dt;
        if (st->strafe_timer <= 0.0f) {
            st->strafe_dir = -st->strafe_dir;
            st->strafe_timer = 0.90f + bot_randf(st) * 1.15f;
        }
        float dir_x = horiz > 1.0f ? dx / horiz : 0.0f;
        float dir_z = horiz > 1.0f ? dz / horiz : 1.0f;
        float perp_x = -dir_z * st->strafe_dir;
        float perp_z = dir_x * st->strafe_dir;
        float fwd_bias = horiz > 420.0f ? 0.90f : (horiz < 190.0f ? -0.55f : 0.15f);
        float strafe_mod = 0.35f + 0.40f * fabsf(sinf(st->strafe_phase));
        float strafe_bias = horiz < 2600.0f ? strafe_mod : 0.0f;
        move_x = dir_x * fwd_bias + perp_x * strafe_bias;
        move_z = dir_z * fwd_bias + perp_z * strafe_bias;
        float mlen = sqrtf(move_x * move_x + move_z * move_z);
        if (mlen > 0.001f) {
            move_x /= mlen;
            move_z /= mlen;
            move_speed = 0.90f;
        }

        /* Trigger control: brief reaction window on fresh LOS, then melee/semi/auto bursts. */
        float yaw_err_raw = fabsf(bot_wrap_deg(desired_theta - me->vv_theta));
        float pitch_err_raw = fabsf(desired_verta - me->vv_verta);
        int is_melee = (item == ITEM_FIST || item == ITEM_KNIFE);
        int in_fire_envelope = is_melee
            ? (horiz < 180.0f && yaw_err_raw < 24.0f && pitch_err_raw < 26.0f)
            : (item > ITEM_KNIFE && mag > 0 && horiz < 2400.0f && yaw_err_raw < 16.0f && pitch_err_raw < 18.0f);
        if (!need_reload && st->seen_time >= 0.08f && in_fire_envelope) {
            int auto_rate = is_melee ? -1 : (int)(s8)bondwalkItemGetAutomaticFiringRate(item);
            if (auto_rate < 0) {
                st->fire_timer -= dt;
                if (st->fire_timer <= 0.0f) {
                    st->semi_toggle ^= 1;
                    m->triggerOn = 1;
                    st->fire_timer = 0.18f + bot_randf(st) * 0.14f;
                }
            } else {
                st->fire_timer -= dt;
                if (st->fire_timer <= 0.0f) {
                    st->fire_burst_on ^= 1;
                    st->fire_timer = st->fire_burst_on ? (0.40f + bot_randf(st) * 0.35f) : (0.18f + bot_randf(st) * 0.16f);
                }
                if (st->fire_burst_on || horiz < 380.0f) {
                    m->triggerOn = 1;
                }
            }
        }
    } else {
        st->seen_time = 0.0f;
        st->los_lost_time += dt;
        float gx = me->prop->pos.x, gz = me->prop->pos.z;
        int have_goal = 0;

        if (have_pickup) {
            gx = pickup_x;
            gz = pickup_z;
            have_goal = 1;
        }

        if (!have_goal && st->last_known_valid && st->los_lost_time < 4.0f) {
            float ldx = st->last_known_x - me->prop->pos.x;
            float ldz = st->last_known_z - me->prop->pos.z;
            if (ldx * ldx + ldz * ldz > 90.0f * 90.0f) {
                gx = st->last_known_x;
                gz = st->last_known_z;
                have_goal = 1;
            } else {
                st->last_known_valid = 0;
            }
        }

        if (!have_goal && best_tgt) {
            gx = best_tgt->prop->pos.x;
            gz = best_tgt->prop->pos.z;
            have_goal = 1;
        } else if (!have_goal && startpadcount > 0) {
            st->patrol_timer -= dt;
            if (st->patrol_idx < 0 || st->patrol_idx >= startpadcount || !g_Startpad[st->patrol_idx]) {
                st->patrol_idx = (int)(bot_rand(st) % (u32)startpadcount);
            }
            struct PadRecord *pad = g_Startpad[st->patrol_idx];
            if (pad) {
                float pdx = pad->pos.x - me->prop->pos.x;
                float pdz = pad->pos.z - me->prop->pos.z;
                if (pdx * pdx + pdz * pdz < 120.0f * 120.0f || st->patrol_timer <= 0.0f) {
                    int next = (int)(bot_rand(st) % (u32)startpadcount);
                    if (next == st->patrol_idx && startpadcount > 1) {
                        next = (st->patrol_idx + 1) % startpadcount;
                    }
                    st->patrol_idx = next;
                    st->patrol_timer = 5.0f + bot_randf(st) * 4.0f;
                    pad = g_Startpad[st->patrol_idx];
                }
                if (pad) {
                    gx = pad->pos.x;
                    gz = pad->pos.z;
                    have_goal = 1;
                }
            }
        }

        if (have_goal) {
            float dx = gx - me->prop->pos.x;
            float dz = gz - me->prop->pos.z;
            float len = sqrtf(dx * dx + dz * dz);
            if (len > 1.0f) {
                move_x = dx / len;
                move_z = dz / len;
                move_speed = 0.85f;
            }
        }

        if (st->alert_timer > 0.0f && best_tgt) {
            float adx = best_tgt->prop->pos.x - me->prop->pos.x;
            float adz = best_tgt->prop->pos.z - me->prop->pos.z;
            desired_theta = atan2f(-adx, adz) * BOT_RAD_TO_DEG;
            if (desired_theta < 0.0f) desired_theta += 360.0f;
            aim_kp_yaw = 20.0f;
        } else if (move_speed > 0.1f) {
            desired_theta = atan2f(-move_x, move_z) * BOT_RAD_TO_DEG;
            if (desired_theta < 0.0f) desired_theta += 360.0f;
            aim_kp_yaw = 32.0f;
        }
        desired_verta = -4.0f;
        aim_kp_pitch = 32.0f;
    }

    /* Stuck detector + door opening + obstacle detouring via STAN feelers. */
    float moved_dx = me->prop->pos.x - st->last_x;
    float moved_dz = me->prop->pos.z - st->last_z;
    if (moved_dx * moved_dx + moved_dz * moved_dz > 16.0f * 16.0f) {
        st->last_x = me->prop->pos.x;
        st->last_z = me->prop->pos.z;
        st->stuck_timer = 0.0f;
    } else if (move_speed > 0.1f) {
        st->stuck_timer += dt;
    }

    if (move_speed > 0.1f && st->btap_cooldown <= 0.0f &&
        bot_closed_door_ahead(me->prop, move_x, move_z, 135.0f)) {
        m->btap = 1;
        st->btap_cooldown = 0.45f;
    }

    if (st->detour_timer > 0.0f) {
        move_x = st->detour_x;
        move_z = st->detour_z;
        move_speed = 0.85f;
        if (!engage_enemy) {
            desired_theta = atan2f(-move_x, move_z) * BOT_RAD_TO_DEG;
            if (desired_theta < 0.0f) desired_theta += 360.0f;
        }
    } else if (move_speed > 0.1f && (!bot_probe_clear(me->prop, move_x, move_z, 95.0f) || st->stuck_timer > 0.65f)) {
        bot_pick_open_direction(me->prop, st, move_x, move_z, &st->detour_x, &st->detour_z);
        st->detour_timer = 0.70f;
        st->stuck_timer = 0.0f;
        move_x = st->detour_x;
        move_z = st->detour_z;
        if (!best_los) {
            desired_theta = atan2f(-move_x, move_z) * BOT_RAD_TO_DEG;
            if (desired_theta < 0.0f) desired_theta += 360.0f;
        }
    }

    /* Turn & pitch smoothly via bondview2's natural analog inputs, with deadband to stop sky drift. */
    float yaw_err = bot_wrap_deg(desired_theta - me->vv_theta);
    float pitch_err = bot_clampf(desired_verta - me->vv_verta, -60.0f, 60.0f);
    m->analogTurn = fabsf(yaw_err) < 0.5f ? 0 : bot_encode_quadratic_axis(yaw_err / aim_kp_yaw);
    m->analogPitch = fabsf(pitch_err) < 0.75f ? 0 : -bot_encode_quadratic_axis(pitch_err / aim_kp_pitch);

    /* Slow forward speed on sharp turns so the bot turns like a human rather than crab-walking. */
    if (!engage_enemy && fabsf(yaw_err) > 70.0f) {
        move_speed *= bot_clampf(1.0f - (fabsf(yaw_err) - 70.0f) / 140.0f, 0.25f, 1.0f);
    }

    /* Project world move vector onto player forward (-sin, cos) and side (-cos, -sin) basis. */
    if (move_speed > 0.05f) {
        float rad = me->vv_theta * BOT_DEG_TO_RAD;
        float fx = -sinf(rad), fz = cosf(rad);
        float sx = -fz, sz = fx;
        float fwd = (move_x * fx + move_z * fz) * move_speed;
        float side = (move_x * sx + move_z * sz) * move_speed;
        m->analogWalk = bot_clampi((int)(fwd * 70.0f), -70, 70);
        m->analogStrafe = bot_clampi((int)(side * 70.0f), -70, 70);
    }
}
