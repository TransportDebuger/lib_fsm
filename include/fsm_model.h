#ifndef FSM_MODEL_H
#define FSM_MODEL_H

#include "fsm_core.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @todo May extend with event driven handler
 *       typedef void (*fsm_event_cb_t)(fsm_context_t ctx);
 */

/** @typedef fsm_state_desc_t
 * @brief Type definition for state structure description
*/
typedef struct fsm_state_desc fsm_state_desc_t;

/**
 * @typedef fsm_event_desc_t
 * @brief Type definition for event structure description
 */
typedef struct fsm_event_desc fsm_event_desc_t;

/**
 * @typedef fsm_model_t
 * @brief Type definition for State/Event central warehose
 */
typedef struct fsm_model fsm_model_t;

/**
 * @struct fsm_state_desc
 * @brief Extended state structure description
 * @todo Other cllbacks and data fields may added if it's needed
 */
struct fsm_state_desc {
    fsm_state_t id; ///< Unique state identifier
    const char *name; ///< State name
    const char *description; ///< State description
    fsm_state_t  parent; ///< Parent state ID (if it's root state, use constant or variable with FSM_STATE_NONE)
    uint32_t flags; ///< State flags (hidden, service, etc.)
    fsm_cb_t on_enter_cb; ///< Callback on state entry, may be null
    fsm_cb_t on_exit_cb; ///< Callback on state exit, may be null
    fsm_cb_t on_update_cb; ///< Callback on state update (calls every tick), may be null
    void *user_data; ///< User data pointer, may be null
};

/**
 * @struct fsm_event_desc
 * @brief Event structure description
 * @todo May be extend with other fields and callbacks
 */
struct fsm_event_desc {
    fsm_event_t id;  ///< Unique event identifier
    const char *name; ///< Event name
    const char *description; ///< Event description
    uint32_t flags; ///< Event flags (hidden, service, priority, etc.)
    void *user_data; ///< User data pointer, may be null
    void *reserved; ///< For future architecture extension (e.g. on_dispatch)
};

/** 
 * @struct fsm_model
 * @brief FSM extended state/event central warehose
 * @todo transitions[] may be added later
*/
struct fsm_model {
    const fsm_state_desc_t *states;
    size_t                  state_count;
    const fsm_event_desc_t *events;
    size_t                  event_count;
    // позже можно добавить transitions[]
};

#ifdef __cplusplus
}
#endif

#endif // FSM_MODEL_H