#ifndef FSM_MODEL_H
#define FSM_MODEL_H

#include "fsm_core.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Flags Flags
 * @{
 */
/**
 * @def FSM_FLAGS_LIB_MASK
 * @brief Mask for reserved library flags
 */
 #define FSM_FLAGS_LIB_MASK   0xFF000000u

/**
 * @def FSM_FLAGS_USER_MASK
 * @brief Mask for user flags
 */
#define FSM_FLAGS_USER_MASK  0x00FFFFFFu

/**
 * @def FSM_STATE_FLAG_INITIAL
 * @brief Initial state flag
 * 
 * Explicit indication of initial state by flag.
 * For model initialization and validation: "only one state can be initial".
 */
#define FSM_STATE_FLAG_INITIAL   0x01000000u

/**
 * @def FSM_STATE_FLAG_FINAL
 * @brief Final state flag
 * 
 * Explicit indication of final state by flag.
 * For model initialization and validation: "only one state can be final".
 */
#define FSM_STATE_FLAG_FINAL     0x02000000u

/**
 * @def FSM_STATE_FLAG_ERROR
 * @brief Error state flag
 * 
 * Explicit indication of error state by flag.
 * It must be helpfull for debugging, diagnostics, and restoration logic.
 */
#define FSM_STATE_FLAG_ERROR     0x04000000u


/**
 * @def FSM_STATE_FLAG_ABSTRACT
 * @brief Abstract state flag
 * 
 * State that exist as parent for other states (super-state) and it will have never used as current state.
 */
#define FSM_STATE_FLAG_ABSTRACT  0x08000000u

/** 
 * @def FSM_STATE_FLAG_HIDDEN
 * @brief Hidden state flag
 * 
 * Technical or service state.
 */
#define FSM_STATE_FLAG_HIDDEN    0x10000000u  /* скрывать в UI/логах/диаграммах по умолчанию */

#define FSM_EVENT_FLAG_INTERNAL   0x01000000u  /* внутреннее событие, генерируется самой FSM/системой */
#define FSM_EVENT_FLAG_TIMER      0x02000000u  /* таймерное/временное событие */
#define FSM_EVENT_FLAG_ERROR      0x04000000u  /* событие ошибки/исключения */
#define FSM_EVENT_FLAG_ASYNC      0x08000000u  /* асинхронное, пришедшее «извне» (IRQ, сеть и т.п.) */
#define FSM_EVENT_FLAG_HIDDEN     0x10000000u  /* не показывать в UI/диаграммах по умолчанию */
#define FSM_EVENT_FLAG_IMPORTANT  0x20000000u  /* важное/приоритетное событие для логов/трассировки */

#define FSM_TRANS_FLAG_INTERNAL    0x01000000u  /* внутренний/служебный переход */
#define FSM_TRANS_FLAG_SILENT      0x02000000u  /* «тихий» переход: не логировать по умолчанию */
#define FSM_TRANS_FLAG_LOG         0x04000000u  /* всегда логировать с подробностями */
#define FSM_TRANS_FLAG_GUARDED     0x08000000u  /* есть guard_cb, переход условный */
#define FSM_TRANS_FLAG_SELF        0x10000000u  /* самопереход (src == dst) */
#define FSM_TRANS_FLAG_DISABLED    0x20000000u  /* отключён (игнорировать при поиске) */
/** @} */ // end of Flags

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
 * @typedef fsm_transition_desc_t
 * @brief Type definition for transition structure description
 */
typedef struct fsm_transition_desc fsm_transition_desc_t;

/**
 * @typedef fsm_model_t
 * @brief Type definition for State/Event central warehose
 */
typedef struct fsm_model fsm_model_t;

typedef bool (*fsm_guard_cb_t)(fsm_context_t ctx);
typedef void (*fsm_action_cb_t)(fsm_context_t ctx);

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


struct fsm_transition_desc {
    fsm_state_t src;  ///< Source state ID
    fsm_event_t event; ///< Event ID
    fsm_state_t dst; ///< Destination state ID
    fsm_guard_cb_t  guard_cb;    ///< conditional guard callback
    fsm_action_cb_t action_cb;   ///< action callback
    uint32_t     flags;    ///< transition flags
    void        *user_data; ///< user data pointer
};

/** 
 * @struct fsm_model
 * @brief FSM extended state/event central warehose
 * @todo transitions[] may be added later
*/
struct fsm_model {
    const fsm_state_desc_t *states;  ///< States description array
    size_t                  state_count; ///< States count
    const fsm_event_desc_t *events; ///< Events description array
    size_t                  event_count; ///< Events count
    const fsm_transition_desc_t *transitions; ///< Transitions description array
    size_t                       transition_count; ///< Transitions count
};

#ifdef __cplusplus
}
#endif

#endif // FSM_MODEL_H