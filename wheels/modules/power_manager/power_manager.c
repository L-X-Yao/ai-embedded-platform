// SPDX-License-Identifier: MIT
/*
 * power_manager — cooperative sleep/wake orchestrator (no OS)
 *
 * Layer:     L3 (modules)
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "power_manager.h"
#include "contract.h"
#include <string.h>

static uint32_t now_ms(const power_manager_t *pm)
{
    return pm->io->get_tick_ms(pm->io->ctx);
}

power_manager_err_t power_manager_init(power_manager_t *pm,
                                       const power_manager_io_t *io)
{
    REQUIRE(pm != NULL);
    REQUIRE(io != NULL);
    if (!pm || !io || !io->get_tick_ms) return POWER_MANAGER_ERR_PARAM;
    memset(pm, 0, sizeof(*pm));
    pm->io            = io;
    pm->current_level = POWER_LEVEL_SHUTDOWN;
    pm->idle_since_ms = now_ms(pm);
    return POWER_MANAGER_OK;
}

power_manager_err_t power_manager_register(power_manager_t *pm,
                                           const char *name,
                                           power_level_t max_sleep,
                                           uint32_t timeout_ms,
                                           uint8_t *id_out)
{
    REQUIRE(pm != NULL);
    REQUIRE(id_out != NULL);
    if (!pm || !id_out) return POWER_MANAGER_ERR_PARAM;
    if (pm->voter_count >= POWER_MANAGER_MAX_VOTERS) return POWER_MANAGER_ERR_FULL;
    if ((unsigned)max_sleep >= POWER_LEVEL_COUNT) return POWER_MANAGER_ERR_PARAM;

    uint8_t id = pm->voter_count;
    power_voter_t *v = &pm->voters[id];
    v->name             = name;
    v->max_sleep        = max_sleep;
    v->timeout_ms       = timeout_ms;
    v->last_activity_ms = 0;
    v->active           = false;

    pm->voter_count++;
    *id_out = id;
    return POWER_MANAGER_OK;
}

power_manager_err_t power_manager_activity(power_manager_t *pm, uint8_t voter_id)
{
    REQUIRE(pm != NULL);
    if (!pm || voter_id >= pm->voter_count) return POWER_MANAGER_ERR_ID;
    power_voter_t *v = &pm->voters[voter_id];
    v->active           = true;
    v->last_activity_ms = now_ms(pm);
    return POWER_MANAGER_OK;
}

power_manager_err_t power_manager_release(power_manager_t *pm, uint8_t voter_id)
{
    REQUIRE(pm != NULL);
    if (!pm || voter_id >= pm->voter_count) return POWER_MANAGER_ERR_ID;
    pm->voters[voter_id].active = false;
    return POWER_MANAGER_OK;
}

power_level_t power_manager_poll(power_manager_t *pm)
{
    REQUIRE(pm != NULL);
    if (!pm) return POWER_LEVEL_ACTIVE;

    uint32_t t = now_ms(pm);
    power_level_t deepest = POWER_LEVEL_SHUTDOWN;

    for (uint8_t i = 0; i < pm->voter_count; i++) {
        power_voter_t *v = &pm->voters[i];

        if (v->active && v->timeout_ms > 0) {
            if ((t - v->last_activity_ms) >= v->timeout_ms) {
                v->active = false;
            }
        }

        if (v->active && v->max_sleep < deepest) {
            deepest = v->max_sleep;
        }
    }

    pm->current_level = deepest;
    return deepest;
}

power_level_t power_manager_get_level(const power_manager_t *pm)
{
    REQUIRE(pm != NULL);
    if (!pm) return POWER_LEVEL_ACTIVE;
    return pm->current_level;
}

uint8_t power_manager_active_count(const power_manager_t *pm)
{
    REQUIRE(pm != NULL);
    if (!pm) return 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < pm->voter_count; i++) {
        if (pm->voters[i].active) count++;
    }
    return count;
}

bool power_manager_is_active(const power_manager_t *pm, uint8_t voter_id)
{
    REQUIRE(pm != NULL);
    if (!pm || voter_id >= pm->voter_count) return false;
    return pm->voters[voter_id].active;
}

const char *power_manager_voter_name(const power_manager_t *pm, uint8_t voter_id)
{
    REQUIRE(pm != NULL);
    if (!pm || voter_id >= pm->voter_count) return NULL;
    return pm->voters[voter_id].name;
}
