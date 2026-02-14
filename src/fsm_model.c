/**
 * @file fsm_model.c
 * @brief Implementation of the FSM model system with validation, lookup, and instantiation support
 *
 * This module provides a complete runtime model for finite state machines, enabling
 * declarative configuration, structural validation, introspection, and dynamic FSM creation.
 * It bridges high-level model definitions (@ref fsm_model_t) with executable instances (@ref fsm_t).
 *
 * Key features:
 * - Model validation via @ref fsm_model_validate: checks for duplicate IDs, invalid references,
 *   and inconsistent hierarchies.
 * - Efficient state and event lookup by ID using linear search (suitable for small-to-medium models).
 * - Safe FSM instantiation with context binding via @ref fsm_model_create_fsm.
 * - Support for hierarchical state machines (HFSM) through parent-child relationships in states.
 * - Extensibility via user data and flags in states, events, and transitions.
 *
 * @note The model is passive — it does not track runtime state. Actual execution is handled
 *       by the @ref fsm_t instance created from it.
 * @note All referenced arrays (states, events, transitions) must remain valid for the lifetime
 *       of any FSM derived from this model. The model does not copy or own them.
 * @note Designed for `const`-correctness and static initialization, making it suitable for
 *       embedded systems and safety-critical applications.
 * @note Transition matching is order-sensitive: the first valid and passing transition
 *       (in array order) is taken. Arrange transitions to ensure correct priority.
 *
 * Example:
 * @code
 * extern const fsm_model_t my_model; // Defined elsewhere
 * fsm_t *fsm = fsm_model_create_fsm(&my_model, app_context, FSM_STATE_NONE);
 * if (!fsm || !fsm_model_validate(&my_model)) {
 *     log_error("FSM setup failed");
 *     return -1;
 * }
 * @endcode
 *
 * @see fsm_model.h      - public interface and type definitions
 * @see struct fsm_model - model structure layout
 * @see fsm_model_validate - integrity checking
 * @see fsm_model_create_fsm - instance creation
 * @see fsm_model_find_state, fsm_model_find_event - runtime lookup utilities
 */
#include "fsm_model.h"
#include <stdbool.h>

/**
 * @brief Error codes returned by FSM model validation functions
 *
 * This enumeration provides detailed diagnostic results from the integrity check
 * of a finite state machine model (@ref fsm_model_t). Each value indicates a specific
 * structural or configuration issue that prevents safe use of the model.
 *
 * @note These codes are used by @ref fsm_model_validate and similar functions to report
 *       validation outcomes. A return value of @ref FSM_MODEL_OK means the model is
 *       structurally sound and ready for use.
 * @note Applications should handle errors appropriately — typically by logging diagnostics
 *       and preventing FSM instantiation until the model is corrected.
 * @note Validation is a critical step before calling @ref fsm_model_create_fsm, especially
 *       when models are generated dynamically or loaded from external sources.
 *
 * Example:
 * @code
 * fsm_model_error_t err = fsm_model_validate(&model);
 * if (err != FSM_MODEL_OK) {
 *     log_error("FSM model validation failed: %s", fsm_model_error_str(err));
 *     return -1;
 * }
 * @endcode
 *
 * @see fsm_model_validate - primary function using this enum
 * @see fsm_model_error_str - optional helper for string representation
 */
typedef enum {
    FSM_MODEL_OK = 0,                        ///< No errors detected; model is valid and consistent
    FSM_MODEL_ERR_NULL,                      ///< The model pointer is NULL — invalid argument
    FSM_MODEL_ERR_BAD_POINTERS,              ///< Mismatch between count and pointer: e.g., state_count > 0 but states == NULL (same for events/transitions)
    FSM_MODEL_ERR_NO_STATES_WITH_TRANSITIONS,///< Transitions exist (transition_count > 0) but no states defined (state_count == 0), making them unusable
    FSM_MODEL_ERR_DUP_STATE_ID,              ///< Duplicate `id` found in the `states` array — each state must have a unique identifier
    FSM_MODEL_ERR_DUP_EVENT_ID,              ///< Duplicate `id` found in the `events` array — each event must be uniquely identifiable
    FSM_MODEL_ERR_BAD_TRANSITION_STATE,      ///< A transition references an unknown state: either `src` or `dst` does not match any ID in the `states` array
    FSM_MODEL_ERR_BAD_TRANSITION_EVENT,      ///< A transition references an unknown event: `event` ID not found in the `events` array
    FSM_MODEL_ERR_STATE_CYCLE                ///< A hierarchical cycle detected: a state is its own ancestor (e.g., A → B → A via parent links)
} fsm_model_error_t;

/**
 * @internal
 * @brief Performs basic structural validation of an FSM model
 *
 * This function checks the physical integrity of the model's core pointers and counts,
 * serving as a fast early-exit check before more expensive logical validation.
 *
 * Verifies:
 * - The model pointer itself is not NULL.
 * - For any non-zero count (states, events, transitions), the corresponding array pointer is non-NULL.
 * - Null arrays are only allowed when their count is zero.
 *
 * @warning This function does **not** validate logical consistency such as:
 *          - Valid state/event IDs in transitions
 *          - Duplicate IDs
 *          - Hierarchical cycles
 *          These must be checked by higher-level validation routines.
 *
 * @param[in]  m    Pointer to the FSM model to validate. May be NULL.
 * @param[out] err  Optional pointer to receive a detailed error code on failure.
 *                  If NULL, no error code is written.
 *
 * @return
 *   - true if all basic structural checks pass;
 *   - false if any check fails, with `*err` set to the specific error code (if `err` is not NULL).
 *
 * @note Designed for internal use in @ref fsm_model_validate and similar functions.
 *       Provides fast rejection of obviously invalid models without deep traversal.
 * @note Safe to call with incomplete or partially initialized models during construction.
 * @note This check is a prerequisite for all deeper validation steps — they assume basic integrity.
 *
 * Example:
 * @code
 * uint32_t err_code;
 * if (!fsm_model_basic_ok(model, &err_code)) {
 *     log_error("Model failed basic check: %d", err_code);
 *     return err_code;
 * }
 * // Proceed to full validation...
 * @endcode
 *
 * @see fsm_model_validate - uses this helper for initial screening
 */
static bool fsm_model_basic_ok(const fsm_model_t *m, uint32_t *err) {
    if (m == NULL) {
        if (err) *err = FSM_MODEL_ERR_NULL;
        return false;
    }

    if ((m->state_count > 0 && m->states == NULL) ||
        (m->event_count > 0 && m->events == NULL) ||
        (m->transition_count > 0 && m->transitions == NULL)) {
        if (err) *err = FSM_MODEL_ERR_BAD_POINTERS;
        return false;
    }

    return true;
}

/**
 * @internal
 * @brief Validates that all transitions reference valid states and events
 *
 * This function performs semantic validation of the model's transition table by ensuring:
 * - Every source (`src`) and destination (`dst`) state ID in each transition
 *   exists in the model's `states` array.
 * - Every referenced event ID (`event`) exists in the model's `events` array.
 *   Note: This function treats `FSM_EVENT_NONE` as a valid special event (e.g., for unconditional transitions).
 *
 * The checks rely on @ref fsm_model_has_state and @ref fsm_model_has_event,
 * which perform linear lookups by ID. As such, performance scales with model size.
 *
 * @param[in]  m    Pointer to the FSM model to validate. Must not be NULL.
 * @param[out] err  Optional pointer to receive a detailed error code on failure.
 *                  If NULL, no error code is written.
 *
 * @return
 *   - true if all transitions are semantically valid;
 *   - false if any transition references an unknown state or event,
 *     with `*err` set accordingly (if `err` is not NULL).
 *
 * @note This is an internal helper used during full model validation.
 *       It assumes the model has already passed basic structural checks
 *       (e.g., via @ref fsm_model_basic_ok) — behavior is undefined otherwise.
 * @note Should be called only after null/checks and pointer validity are confirmed.
 * @note Validation stops at the first error encountered (short-circuit).
 * @note Required for safe FSM instantiation — prevents runtime crashes due to invalid transitions.
 *
 * Example:
 * @code
 * if (!fsm_model_transitions_ok(model, &error)) {
 *     log_error("Transition validation failed: %d", error);
 *     return FSM_MODEL_ERR_BAD_TRANSITION_STATE;
 * }
 * @endcode
 *
 * @see fsm_model_has_state - checks state ID existence
 * @see fsm_model_has_event - checks event ID existence
 * @see fsm_model_validate - uses this function as part of complete validation
 */
static bool fsm_model_transitions_ok(const fsm_model_t *m, uint32_t *err) {
    if (!m) {
        if (err) *err = FSM_MODEL_ERR_NULL;
        return false;
    }

    if (m->transition_count > 0 && m->state_count == 0) {
        if (err) *err = FSM_MODEL_ERR_NO_STATES_WITH_TRANSITIONS;
        return false;
    }

    for (size_t i = 0; i < m->transition_count; ++i) {
        const fsm_transition_desc_t *t = &m->transitions[i];

        if (!fsm_model_has_state(m, t->src) || !fsm_model_has_state(m, t->dst)) {
            if (err) *err = FSM_MODEL_ERR_BAD_TRANSITION_STATE;
            return false;
        }
        if (!fsm_model_has_event(m, t->event)) {
            if (err) *err = FSM_MODEL_ERR_BAD_TRANSITION_EVENT;
            return false;
        }
    }
    return true;
}

/**
 * @internal
 * @brief Validates that the state hierarchy contains no parent cycles
 *
 * This function ensures that the hierarchical structure of states is acyclic.
 * Each state may have a parent (via `parent` field), forming a tree-like topology.
 * A cycle in this hierarchy (e.g., A → B → A) would lead to undefined behavior
 * during entry/exit logic and must be prevented.
 *
 * The algorithm:
 * - For each state, walks upward through the `parent` chain.
 * - Checks if any ancestor is the original state (direct or indirect cycle).
 * - Uses a depth limit (@ref MAX_DEPTH) to detect potential infinite loops.
 *
 * @param[in]  m    Pointer to the FSM model. Must not be NULL.
 * @param[out] err  Optional pointer to receive error code on failure.
 *                  Set to @ref FSM_MODEL_ERR_STATE_CYCLE if a cycle is detected.
 *                  May be NULL if error details are not needed.
 *
 * @return
 *   - true if the hierarchy is cycle-free;
 *   - false if a cycle is detected, a broken parent link is found,
 *     or input validation fails.
 *
 * @note Assumes the model has passed basic structural checks (non-null pointers).
 *       Behavior is undefined otherwise.
 * @note Only checks for cycles involving valid state IDs — broken references
 *       (e.g., invalid `parent`) are caught by other validation steps.
 * @note The @ref MAX_DEPTH limit (16) prevents stack overflow or infinite loops
 *       in malformed models; exceeding it is treated as a cycle.
 * @note Safe to call on flat state machines (all `parent == FSM_STATE_NONE`).
 * @note Used internally by @ref fsm_model_validate to ensure safe HFSM operation.
 *
 * Example:
 * @code
 * uint32_t error;
 * if (!fsm_model_hierarchy_ok(&model, &error)) {
 *     log_error("Invalid state hierarchy: cycle detected");
 * }
 * @endcode
 *
 * @see fsm_model_find_state - used to resolve state descriptors by ID
 * @see fsm_model_validate - integrates this check into full model validation
 */
static bool fsm_model_hierarchy_ok(const fsm_model_t *m, uint32_t *err) {
    if (!m) {
        if (err) *err = FSM_MODEL_ERR_NULL;
        return false;
    }
    if (!m->states || m->state_count == 0) {
        return true; // No states — no cycles
    }

    const int MAX_DEPTH = 16;

    for (size_t i = 0; i < m->state_count; ++i) {
        fsm_state_t current = m->states[i].id;
        int depth = 0;

        while (depth < MAX_DEPTH) {
            const fsm_state_desc_t *desc = fsm_model_find_state(m, current);
            if (!desc) break; // Broken link — handled elsewhere
            if (desc->parent == FSM_STATE_NONE) break; // Root reached

            // Cycle check: does parent point back to the starting state?
            if (desc->parent == m->states[i].id) {
                if (err) *err = FSM_MODEL_ERR_STATE_CYCLE;
                return false;
            }

            current = desc->parent;
            depth++;
        }

        if (depth >= MAX_DEPTH) {
            if (err) *err = FSM_MODEL_ERR_STATE_CYCLE;
            return false; // Excessive depth implies a cycle
        }
    }

    return true;
}

/**
 * @internal
 * @brief Validates uniqueness of state and event IDs within the FSM model
 *
 * This function ensures that:
 * - All state IDs in the `states` array are unique.
 * - All event IDs in the `events` array are unique.
 *
 * It performs pairwise comparison (O(n²)) across each array, exiting early on the first duplicate found.
 * While not optimal for very large models, it is efficient for typical embedded use cases and avoids
 * dynamic memory allocation or sorting.
 *
 * @param[in]  m    Pointer to the FSM model to validate. Must not be NULL.
 * @param[out] err  Optional pointer to receive the specific error code:
 *                  - @ref FSM_MODEL_ERR_DUP_STATE_ID if duplicate state ID is found
 *                  - @ref FSM_MODEL_ERR_DUP_EVENT_ID if duplicate event ID is found
 *                  If NULL, no error code is written.
 *
 * @return
 *   - true if all IDs are unique;
 *   - false if any duplicate is detected, with `*err` set accordingly (if `err` is not NULL).
 *
 * @note This is an internal helper used by @ref fsm_model_validate.
 *       It assumes the model has already passed basic structural checks (e.g., via @ref fsm_model_basic_ok).
 * @note Does not treat special values like @ref FSM_STATE_NONE or @ref FSM_EVENT_NONE as reserved or invalid —
 *       they are subject to the same uniqueness rules as any other ID.
 * @note Duplicate IDs can lead to ambiguous transitions, incorrect state entry/exit, or failed lookups —
 *       hence this check is critical for model integrity.
 * @note The O(n²) complexity is acceptable for small-to-medium models (<100 elements), which is typical
 *       in most FSM applications. For larger models, consider preprocessing IDs during build time.
 *
 * Example:
 * @code
 * uint32_t error;
 * if (!fsm_model_ids_unique(&model, &error)) {
 *     log_error("Duplicate ID detected: %d", error);
 *     return error;
 * }
 * @endcode
 *
 * @see fsm_model_validate - integrates this check into full validation
 */
static bool fsm_model_ids_unique(const fsm_model_t *m, uint32_t *err) {
    if (!m) {
        if (err) *err = FSM_MODEL_ERR_NULL;
        return false;
    }

    for (size_t i = 0; i < m->state_count; ++i) {
        fsm_state_t id_i = m->states[i].id;
        for (size_t j = i + 1; j < m->state_count; ++j) {
            if (m->states[j].id == id_i) {
                if (err) *err = FSM_MODEL_ERR_DUP_STATE_ID;
                return false;
            }
        }
    }

    for (size_t i = 0; i < m->event_count; ++i) {
        fsm_event_t id_i = m->events[i].id;
        for (size_t j = i + 1; j < m->event_count; ++j) {
            if (m->events[j].id == id_i) {
                if (err) *err = FSM_MODEL_ERR_DUP_EVENT_ID;
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Validates the FSM model using default validation rules
 *
 * Performs a comprehensive integrity check on the finite state machine model,
 * including:
 * - Structural validity (non-null pointers for non-empty arrays)
 * - Uniqueness of state and event IDs
 * - Validity of transition endpoints (referenced states/events exist)
 * - Absence of hierarchy cycles in parented states
 *
 * This function provides a simple boolean result indicating overall validity.
 *
 * @param[in] model Pointer to the FSM model to validate. May be NULL.
 *
 * @return
 *   - true if all validation checks pass and the model is safe to use;
 *   - false if any structural, logical, or semantic inconsistency is detected,
 *     including null model pointer, duplicate IDs, invalid transitions, or cycles.
 *
 * @note This is a convenience wrapper around @ref fsm_model_validate_ex.
 *       It suppresses detailed error reporting — suitable for production or
 *       when only pass/fail status is needed.
 * @note For debugging, configuration tools, or diagnostics, prefer
 *       @ref fsm_model_validate_ex to get precise failure reasons.
 * @note Validation is fast and essential before calling @ref fsm_model_create_fsm.
 *       Never instantiate an FSM from an unvalidated model.
 * @note Safe to call multiple times; does not modify the model.
 *
 * Example:
 * @code
 * if (!fsm_model_validate(&my_model)) {
 *     log_error("FSM model failed validation");
 *     return -1;
 * }
 * fsm_t *fsm = fsm_model_create_fsm(&my_model, ctx, FSM_STATE_NONE);
 * @endcode
 *
 * @see fsm_model_validate_ex - extended version with error code and step reporting
 * @see fsm_model_create_fsm - creates an FSM instance from a valid model
 */
bool fsm_model_validate(const fsm_model_t *model) {
    return fsm_model_validate_ex(model, NULL, NULL);
}

/**
 * @brief Validates the FSM model with detailed error reporting on failure
 *
 * Performs a complete, multi-stage integrity check of the finite state machine model,
 * ensuring it is safe to instantiate. The validation includes:
 * - Structural correctness (non-null pointers for non-zero counts)
 * - Uniqueness of state and event IDs
 * - Validity of all transition endpoints (referenced states and events exist)
 * - Absence of cycles in the hierarchical state parent chain
 *
 * On failure, provides precise diagnostic information via optional output parameters.
 *
 * @param[in]  model      Pointer to the FSM model to validate. May be NULL.
 * @param[out] error_code If not NULL, receives a specific error code from @ref fsm_model_error_t
 *                        indicating the first detected failure.
 * @param[out] error_msg  If not NULL, receives a pointer to a null-terminated string literal
 *                        describing the error (e.g., "duplicate state id"). The string resides
 *                        in read-only memory and must not be modified or freed by the caller.
 *
 * @return
 *   - true if all validation stages pass successfully;
 *   - false if any check fails, in which case `*error_code` and `*error_msg` are set
 *     (if the respective pointers are not NULL).
 *
 * @note This is the comprehensive validation function used during model inspection,
 *       debugging, or configuration loading. For simple pass/fail checks, use
 *       @ref fsm_model_validate instead.
 * @note Validation proceeds in order: structure → ID uniqueness → transitions → hierarchy.
 *       The first failure stops further checks (short-circuit).
 * @note Safe to call multiple times; does not modify the model or its data.
 * @note Essential to call before @ref fsm_model_create_fsm — prevents runtime crashes
 *       due to invalid configurations.
 * @note Error messages are concise and designed for logging or diagnostic tools.
 *
 * Example:
 * @code
 * uint32_t err_code;
 * const char *err_msg;
 * if (!fsm_model_validate_ex(&model, &err_code, &err_msg)) {
 *     log_error("Model validation failed: [%d] %s", err_code, err_msg);
 *     return -1;
 * }
 * @endcode
 *
 * @see fsm_model_validate - simplified version returning only bool
 * @see fsm_model_error_t - list of possible error codes
 * @see fsm_model_create_fsm - creates an FSM instance from a valid model
 */
bool fsm_model_validate_ex(const fsm_model_t *model,
                           uint32_t *error_code,
                           const char **error_msg) {
    uint32_t err = FSM_MODEL_OK;

    /* core structure */
    if (!fsm_model_basic_ok(model, &err)) {
        if (error_code) *error_code = err;
        if (error_msg) {
            switch (err) {
            case FSM_MODEL_ERR_NULL:
                *error_msg = "model is NULL";
                break;
            case FSM_MODEL_ERR_BAD_POINTERS:
                *error_msg = "inconsistent pointers/counts in model";
                break;
            case FSM_MODEL_ERR_NO_STATES_WITH_TRANSITIONS:
                *error_msg = "transitions defined but no states";
                break;
            default:
                *error_msg = "basic model validation failed";
                break;
            }
        }
        return false;
    }

    /* id uniqueness */
    if (!fsm_model_ids_unique(model, &err)) {
        if (error_code) *error_code = err;
        if (error_msg) {
            switch (err) {
            case FSM_MODEL_ERR_DUP_STATE_ID:
                *error_msg = "duplicate state id";
                break;
            case FSM_MODEL_ERR_DUP_EVENT_ID:
                *error_msg = "duplicate event id";
                break;
            default:
                *error_msg = "id uniqueness validation failed";
                break;
            }
        }
        return false;
    }

    /* transition correctness */
    if (!fsm_model_transitions_ok(model, &err)) {
        if (error_code) *error_code = err;
        if (error_msg) {
            switch (err) {
            case FSM_MODEL_ERR_BAD_TRANSITION_STATE:
                *error_msg = "transition references unknown state";
                break;
            case FSM_MODEL_ERR_BAD_TRANSITION_EVENT:
                *error_msg = "transition references unknown event";
                break;
            default:
                *error_msg = "transition validation failed";
                break;
            }
        }
        return false;
    }

    /* Hierarchy problems detection */
    if (!fsm_model_hierarchy_ok(model, &err)) {
      if (error_code) *error_code = err;
      if (error_msg) {
        switch (err) {
        case FSM_MODEL_ERR_STATE_CYCLE:
            *error_msg = "cycle detected in state parent hierarchy";
            break;
        case FSM_MODEL_ERR_NULL:
            *error_msg = "model is NULL";
            break;
        default:
            *error_msg = "state hierarchy validation failed";
            break;
        }
      }
      return false;
    }

    /* all OK */
    if (error_code) *error_code = FSM_MODEL_OK;
    if (error_msg)  *error_msg  = NULL;
    return true;
}

/**
 * @brief Checks if a state with the given ID exists in the FSM model
 *
 * Performs a linear search through the `states` array to find a state descriptor
 * whose `id` matches the specified value.
 *
 * The special sentinel value @ref FSM_STATE_NONE is explicitly treated as invalid
 * and will always return `false`, even if such an ID were somehow present in the array.
 *
 * @param[in] m  Pointer to the FSM model. If NULL, the function returns `false`.
 * @param[in] id State identifier to search for.
 *
 * @return
 *   - true if a valid state with the specified ID (other than @ref FSM_STATE_NONE) is found;
 *   - false if:
 *     - the model pointer is NULL,
 *     - the ID is @ref FSM_STATE_NONE,
 *     - no matching state ID exists in the array.
 *
 * @note This function is used during model validation (e.g., transition target checks).
 *       It assumes the model’s `states` array is well-formed (non-null if count > 0).
 * @note Time complexity is O(n) — suitable for small-to-medium models typical in embedded systems.
 * @note Not intended for performance-critical runtime paths; consider caching results if needed.
 * @note Used by functions like @ref fsm_model_transitions_ok to validate transition endpoints.
 *
 * Example:
 * @code
 * if (!fsm_model_has_state(model, transition->src)) {
 *     // Handle invalid source state
 * }
 * @endcode
 *
 * @see fsm_model_find_state - similar lookup that returns the descriptor pointer
 * @see fsm_model_transitions_ok - uses this function to validate transition states
 */
bool fsm_model_has_state(const fsm_model_t *m, fsm_state_t id) {
    if (!m) return false;

    if (id == FSM_STATE_NONE) return false;

    for (size_t i = 0; i < m->state_count; ++i) {
        if (m->states[i].id == id) return true;
    }
    return false;
}

/**
 * @brief Finds the state descriptor by its identifier in the FSM model
 *
 * Performs a linear search through the model's `states` array to locate a state
 * with the specified ID and returns a pointer to its full descriptor.
 *
 * The special sentinel value @ref FSM_STATE_NONE is not considered a valid state
 * and will always result in NULL being returned, even if a state with that ID exists.
 *
 * @param[in] model Pointer to the FSM model. May be NULL.
 * @param[in] id    State identifier to search for.
 *
 * @return
 *   - Pointer to the matching @ref fsm_state_desc_t if found;
 *   - NULL if:
 *     - the model is NULL,
 *     - the ID is @ref FSM_STATE_NONE,
 *     - the states array is NULL or empty,
 *     - no state with the given ID exists.
 *
 * @note This function is intended for metadata access — such as retrieving a state's
 *       name, description, flags, or user data — during logging, debugging, or UI rendering.
 * @note For simple existence checks (e.g., during transition validation), prefer
 *       @ref fsm_model_has_state(), which is slightly more efficient.
 * @note Time complexity is O(n); suitable for small-to-medium models typical in embedded systems.
 * @note The returned pointer is const and remains valid only as long as the original model
 *       and its states array are alive.
 *
 * Example:
 * @code
 * const fsm_state_desc_t *desc = fsm_model_find_state(model, current_state);
 * if (desc) {
 *     printf("Entering state: %s\n", desc->name);
 * }
 * @endcode
 *
 * @see fsm_model_has_state - use when only presence check is needed
 * @see fsm_model_find_event - equivalent lookup for events
 */
const fsm_state_desc_t *fsm_model_find_state(const fsm_model_t *model,
                                             fsm_state_t id) {
    if (!model) return NULL;
    if (id == FSM_STATE_NONE) return NULL;
    if (!model->states || model->state_count == 0) return NULL;

    for (size_t i = 0; i < model->state_count; ++i) {
        if (model->states[i].id == id) {
            return &model->states[i];
        }
    }

    return NULL; //state not found
}

/**
 * @brief Checks whether an event identifier is valid within the FSM model
 *
 * Determines if a given event ID is usable in the context of the specified model.
 * An event is considered valid if:
 * - It matches any `id` in the model's `events` array, or
 * - It equals @ref FSM_EVENT_NONE, which represents a special internal (null) event
 *   used for timer-driven, immediate, or unconditional transitions.
 *
 * @param[in] model Pointer to the FSM model. May be NULL — in this case, returns `false`.
 * @param[in] id    Event identifier to validate.
 *
 * @return
 *   - `true` if the event is valid:
 *     - exists in the model's event list, or
 *     - is @ref FSM_EVENT_NONE;
 *   - `false` if:
 *     - the model is NULL,
 *     - the event array is NULL or empty,
 *     - the event ID is not found and is not @ref FSM_EVENT_NONE.
 *
 * @note This function is intended for pre-validation of incoming events (e.g., from queues,
 *       sensors, or external inputs) before passing them to @ref fsm_process_event.
 *       Helps prevent processing of undefined or malformed events.
 * @note For retrieving metadata (name, description, flags), use @ref fsm_model_find_event()
 *       instead — this function only answers "is valid?", not "what is it?".
 * @note Time complexity is O(n); acceptable for typical FSM sizes in embedded systems.
 * @note Safe to call concurrently with other read-only model operations, assuming no model mutation.
 *
 * Example:
 * @code
 * if (!fsm_model_has_event(model, received_event_id)) {
 *     log_warning("Dropped invalid event: %d", received_event_id);
 *     return;
 * }
 * fsm_process_event(fsm, received_event_id, event_data);
 * @endcode
 *
 * @see fsm_model_find_event - retrieve full event descriptor by ID
 * @see fsm_model_validate - full model integrity check including event ID uniqueness
 */
bool fsm_model_has_event(const fsm_model_t *model, fsm_event_t id) {
    if (!model) return false;

    // FSM_EVENT_NONE is a special internal event, always valid
    if (id == FSM_EVENT_NONE) return true;

    if (!model->events || model->event_count == 0) return false;

    for (size_t i = 0; i < model->event_count; ++i) {
        if (model->events[i].id == id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Finds the event description by its identifier in the FSM model.
 *
 * Performs a linear search through the model's array of event descriptions
 * until an entry with the specified event ID is found.
 *
 * @details
 * This function is intended exclusively for retrieving metadata associated with
 * an event, such as its name (@c name), debug information, or other attributes
 * stored in @ref fsm_event_desc_t. It MUST NOT be used to validate events
 * in the context of state transitions.
 *
 * The special value @ref FSM_EVENT_NONE is considered internal and has
 * no associated description — this function always returns NULL for it.
 *
 * @param[in] model Pointer to the FSM model configuration. May be NULL.
 *                  If the model is invalid, the function safely returns NULL.
 * @param[in] id    Event identifier of type @ref fsm_event_t to search for.
 *                  Must correspond to one of the values defined in the model.
 *
 * @return
 *   - Pointer to @ref fsm_event_desc_t if the event is found;
 *   - NULL if:
 *     - model is NULL;
 *     - model has no events (@c events == NULL or @c event_count == 0);
 *     - @p id equals @ref FSM_EVENT_NONE;
 *     - event with the given @p id is not present in the model.
 *
 * @note
 * To check whether an event is valid (including @ref FSM_EVENT_NONE),
 * use @ref fsm_model_has_event(), which is optimized for this purpose
 * and should be preferred for validation.
 *
 * @see fsm_model_has_event - checks if an event is valid within the model
 * @see fsm_event_desc_t     - structure containing event metadata
 *
 * @par Example usage:
 * @code
 * const fsm_event_desc_t *desc = fsm_model_find_event(model, MY_EVENT_ID);
 * if (desc) {
 *     printf("Event name: %s\n", desc->name);
 * }
 * @endcode
 *
 * @pre   The model must be properly initialized before calling this function.
 * @post  This function does not modify the model and is thread-safe,
 *        provided that the model is not modified concurrently.
 */
const fsm_event_desc_t *fsm_model_find_event(const fsm_model_t *model,
                                             fsm_event_t id) {
    if (!model) return NULL;
    if (!model->events || model->event_count == 0) return NULL;
    if (id == FSM_EVENT_NONE) return NULL; // no description for internal event

    for (size_t i = 0; i < model->event_count; ++i) {
        if (model->events[i].id == id) {
            return &model->events[i];
        }
    }

    return NULL; // event not found
}

/**
 * @brief Finds a transition in the FSM model by source state and event.
 *
 * Performs a linear search through the model's transition array to locate
 * the first transition matching the specified source state and event.
 *
 * @details
 * The search considers both fields:
 * - @p src must equal @c t->src
 * - @p event must equal @c t->event
 *
 * Transitions with @ref FSM_STATE_NONE as source are invalid and not allowed.
 * However, @ref FSM_EVENT_NONE is treated as a valid trigger — if such a transition
 * exists, it will be matched on exact event equality.
 *
 * @param[in] model Pointer to the FSM model. May be NULL.
 * @param[in] src   Source state identifier. Using @ref FSM_STATE_NONE returns NULL.
 * @param[in] event Event that triggers the transition. May be any value, including @ref FSM_EVENT_NONE.
 *
 * @retval non-NULL Pointer to the matching @ref fsm_transition_desc_t; valid as long as model exists.
 * @retval NULL     If no match found, model is invalid, has no transitions, or @p src is @ref FSM_STATE_NONE.
 *
 * @note
 * Only the first matching transition is returned. Duplicate (src,event) entries
 * are considered invalid per model validation rules — behavior with duplicates is undefined.
 *
 * @attention
 * The returned pointer is **not owned** by the caller and must not be freed.
 * It becomes invalid if the model is destroyed or reloaded.
 *
 * @see fsm_model_validate - ensures model consistency, including absence of duplicates
 *
 * @par Example usage:
 * @code
 * const fsm_transition_desc_t *trans = fsm_model_find_transition(model, STATE_ACTIVE, EV_START);
 * if (trans) {
 *     printf("Found action: %s\n", trans->action_name);
 * }
 * @endcode
 *
 * @pre   Model must be initialized and valid (preferably validated via fsm_model_validate).
 * @post  Function is thread-safe if model is not modified concurrently.
 */
const fsm_transition_desc_t *fsm_model_find_transition(const fsm_model_t *model,
                                                       fsm_state_t src,
                                                       fsm_event_t event) {
    if (!model) return NULL;
    if (!model->transitions || model->transition_count == 0) return NULL;
    if (src == FSM_STATE_NONE) return NULL;  // no transitions from "none" state

    for (size_t i = 0; i < model->transition_count; ++i) {
        const fsm_transition_desc_t *t = &model->transitions[i];

        if (t->src != src) continue;

        /* For FSM_EVENT_NONE, we look for an exact match in the table.
           It is already considered valid during model validation,
           so no special handling is required here. */
        if (t->event == event) {
            return t;
        }
    }

    return NULL; // transition not found
}

/**
 * @brief Returns the parent state ID for the given state.
 *
 * This function is intended for use in hierarchical finite state machines (HFSM).
 * It finds the state description by its identifier and returns the value of the parent field.
 *
 * If the state is not found, the model is invalid, or FSM_STATE_NONE is passed,
 * the function returns FSM_STATE_NONE, which indicates that the state has no parent.
 *
 * @param[in] model Pointer to the FSM model. May be NULL.
 * @param[in] state State identifier for which the parent is requested.
 *
 * @return
 *   - Parent state identifier, if defined;
 *   - FSM_STATE_NONE if the state has no parent, is not found,
 *     or if input parameters are invalid.
 *
 * @note This function assumes that the fsm_state_desc_t structure contains
 *       a 'parent' field of type fsm_state_t. Behavior is undefined if this field
 *       is missing or uninitialized.
 */
fsm_state_t fsm_model_get_parent_state(const fsm_model_t *model,
                                       fsm_state_t state) {
    if (!model) return FSM_STATE_NONE;
    if (state == FSM_STATE_NONE) return FSM_STATE_NONE;

    const fsm_state_desc_t *desc = fsm_model_find_state(model, state);
    if (!desc) return FSM_STATE_NONE;

    return desc->parent;
}

/**
 * @brief Checks if one state is a strict ancestor of another in the FSM hierarchy.
 *
 * This function determines whether the 'ancestor' state is above the 'state'
 * in the hierarchical finite state machine (HFSM), i.e., whether 'ancestor'
 * can be reached by traversing the parent chain upward from 'state'.
 *
 * The check includes protection against:
 * - Invalid input parameters;
 * - Corrupted models (missing state descriptions);
 * - Cycles in the hierarchy (via depth limit).
 *
 * @param[in] model    Pointer to the FSM model. May be NULL — returns false.
 * @param[in] ancestor State identifier of the potential ancestor.
 *                     Must not be FSM_STATE_NONE.
 * @param[in] state    State identifier of the child state being checked.
 *                     Must not be FSM_STATE_NONE.
 *
 * @return
 *   - true if 'ancestor' is a strict ancestor of 'state';
 *   - false otherwise, including cases when:
 *     - any input is invalid;
 *     - model == NULL;
 *     - ancestor == state;
 *     - model has cycles or broken references;
 *     - state is not found.
 *
 * @note The function does not consider a state to be its own ancestor (strict inclusion).
 *       To include self-check, compare states separately before calling this function.
 */
bool fsm_model_is_ancestor_state(const fsm_model_t *model,
                                 fsm_state_t ancestor,
                                 fsm_state_t state) {
    if (!model) return false;
    if (ancestor == FSM_STATE_NONE) return false;
    if (state == FSM_STATE_NONE) return false;
    if (ancestor == state) return false; /* strict ancestor */

    fsm_state_t current = fsm_model_get_parent_state(model, state);

    while (current != FSM_STATE_NONE) {
        if (current == ancestor) {
            return true;
        }
        current = fsm_model_get_parent_state(model, current);
    }

    return false;
}

/**
 * @brief Returns the initial state of the FSM model.
 *
 * This function searches through the model's states array to find the first state
 * marked with the FSM_STATE_FLAG_INITIAL flag and returns its identifier.
 *
 * If multiple states are marked as initial, the first one encountered is returned.
 * If no initial state is defined, or if the model is invalid or empty,
 * FSM_STATE_NONE is returned.
 *
 * @param[in] model Pointer to the FSM model. May be NULL.
 *
 * @return
 *   - State identifier of the initial state if found;
 *   - FSM_STATE_NONE if no initial state is defined, or if the model is invalid,
 *     has no states, or the state_count is zero.
 *
 * @note It is recommended that a valid FSM model contains exactly one initial state.
 *       The behavior when multiple initial states are present is implementation-defined.
 */
fsm_state_t fsm_model_get_initial_state(const fsm_model_t *model) {
    if (!model || !model->states || model->state_count == 0) {
        return FSM_STATE_NONE;
    }

    for (size_t i = 0; i < model->state_count; ++i) {
        if (model->states[i].flags & FSM_STATE_FLAG_INITIAL) {
            return model->states[i].id;
        }
    }

    return FSM_STATE_NONE;
}

/**
 * @brief Checks whether the FSM model contains at least one final state.
 *
 * This function iterates through the model's states array and checks if any state
 * is marked with the FSM_STATE_FLAG_FINAL flag.
 *
 * @param[in] model Pointer to the FSM model. May be NULL.
 *
 * @return
 *   - true if at least one state has the FSM_STATE_FLAG_FINAL flag set;
 *   - false if no final states are found, or if the model is invalid,
 *     has no states, or state_count is zero.
 *
 * @note This function only checks for the presence of final states.
 *       It does not validate whether the model is otherwise correct or complete.
 */
bool fsm_model_has_final_state(const fsm_model_t *model) {
    if (!model || !model->states || model->state_count == 0) {
        return false;
    }

    for (size_t i = 0; i < model->state_count; ++i) {
        if (model->states[i].flags & FSM_STATE_FLAG_FINAL) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Builds a flat array of runtime transitions from the FSM model.
 *
 * This function converts the high-level transition descriptions in the model
 * into a simplified, executable transition table suitable for use by the FSM engine.
 * It copies source, event, and destination states, and resolves entry/exit callbacks
 * by looking up the corresponding state descriptors.
 *
 * @param[in]  model             Pointer to the validated FSM model. Must not be NULL.
 * @param[out] out_transitions   Output buffer to store the built transitions.
 *                               Must not be NULL.
 * @param[in]  max_count         Maximum number of transitions that can be written
 *                               to out_transitions.
 * @param[out] out_count         Optional pointer to store the actual number of
 *                               transitions written on success. May be NULL.
 *
 * @return
 *   - true if all transitions were successfully copied and fit in the buffer;
 *   - false if input is invalid or the output buffer is too small (max_count < model->transition_count).
 *
 * @note This function assumes that the model has already been validated
 *       (e.g., via fsm_model_validate()). It does not check transition validity,
 *       only performs data copying and callback resolution.
 * @note If a source or destination state is not found (should not happen in valid model),
 *       the corresponding callback (on_exit or on_enter) will be set to NULL.
 */
bool fsm_model_build_core_transitions(const fsm_model_t *model,
                                      fsm_transition_t *out_transitions,
                                      size_t max_count,
                                      size_t *out_count) {
    // Input validation
    if (!model || !out_transitions) {
        if (out_count) *out_count = 0;
        return false;
    }

    // Handle empty model
    if (model->transition_count == 0) {
        if (out_count) *out_count = 0;
        return true;
    }

    // Buffer size check
    if (model->transition_count > max_count) {
        if (out_count) *out_count = 0;
        return false;
    }

    // Ensure transitions array is valid (after checking count > 0)
    if (!model->transitions) {
        if (out_count) *out_count = 0;
        return false;
    }

    // Pre-resolve state callbacks to avoid repeated lookups
    const size_t state_count = model->state_count;
    const fsm_state_desc_t *states = model->states;

    // Build transitions
    for (size_t i = 0; i < model->transition_count; ++i) {
        const fsm_transition_desc_t *t_desc = &model->transitions[i];
        fsm_transition_t *t = &out_transitions[i];

        t->src   = t_desc->src;
        t->event = t_desc->event;
        t->dst   = t_desc->dst;

        // Optimized lookup: cache on_enter/on_exit callbacks by state ID
        void (*on_exit)(void)  = NULL;
        void (*on_enter)(void) = NULL;

        for (size_t j = 0; j < state_count; ++j) {
            if (states[j].id == t_desc->src) {
                on_exit = states[j].on_exit_cb;
            }
            if (states[j].id == t_desc->dst) {
                on_enter = states[j].on_enter_cb;
            }
        }

        t->on_exit  = on_exit;
        t->on_enter = on_enter;
    }

    if (out_count) *out_count = model->transition_count;
    return true;
}

/**
 * @brief Creates an FSM instance from a validated model description.
 *
 * This function builds a runtime-executable FSM by:
 * - Validating the input model,
 * - Resolving the initial state (either provided or marked with INITIAL flag),
 * - Converting high-level transitions into a flat core-compatible table,
 * - Allocating and initializing the FSM instance.
 *
 * The resulting FSM owns its transition table: memory will be freed automatically
 * when fsm_free() is called.
 *
 * @param[in] model  Pointer to a fully defined and valid FSM model. Must not be NULL.
 * @param[in] ctx    User context passed to all callbacks during transitions.
 * @param[in] start  Initial state ID. If FSM_STATE_NONE, the state marked with
 *                   FSM_STATE_FLAG_INITIAL will be used.
 *
 * @return
 *   - Pointer to a newly allocated and initialized fsm_t on success;
 *   - NULL if model is invalid, memory allocation fails, no valid initial state,
 *     or transition table cannot be built.
 *
 * @note The returned FSM must be destroyed using fsm_free() to avoid memory leaks.
 *       After freeing, the pointer becomes invalid.
 * @note This function allocates memory for both the FSM and its transition table.
 *       Both are deallocated by a single call to fsm_free().
 *
 * @warning Do not manually free or modify the internal transition table.
 * @warning Avoid calling with untrusted or unvalidated models without prior validation.
 *
 * Example usage:
 * @code
 * fsm_model_t *model = ...; // populated model
 * fsm_t *fsm = fsm_model_create_fsm(model, &my_data, FSM_STATE_NONE);
 * if (fsm) {
 *     fsm_process_event(fsm, EV_START);
 *     fsm_free(fsm); // frees everything
 * }
 * @endcode
 */
fsm_t *fsm_model_create_fsm(const fsm_model_t *model,
                            fsm_context_t ctx,
                            fsm_state_t start) {
    if (!model) {
        return NULL;
    }

    // Validate model integrity before use
    if (!fsm_model_validate(model)) {
        return NULL;
    }

    // Determine initial state: use provided one or find via flag
    fsm_state_t initial = start;
    if (initial == FSM_STATE_NONE) {
        initial = fsm_model_get_initial_state(model);
        if (initial == FSM_STATE_NONE) {
            return NULL; // No initial state defined in model
        }
    }

    // Handle models with no transitions
    if (model->transition_count == 0) {
        return fsm_new(NULL, 0, ctx, initial);
    }

    if (!model->transitions) {
        return NULL;
    }

    // Allocate transition table — will be owned by the FSM
    const size_t count = model->transition_count;
    fsm_transition_t *transitions = malloc(count * sizeof(fsm_transition_t));
    if (!transitions) {
        return NULL;
    }

    // Build core transitions
    size_t built = 0;
    bool result = fsm_model_build_core_transitions(model, transitions, count, &built);
    if (!result || built == 0) {
        free(transitions);
        return NULL;
    }

    // Create FSM instance — now both FSM and transitions are ready
    fsm_t *fsm = fsm_new(transitions, built, ctx, initial);
    if (!fsm) {
        free(transitions);
        return NULL;
    }

    return fsm;
}

/**
 * @brief Retrieves all transitions originating from a specific state.
 *
 * This function scans the entire transition table and returns the count of transitions
 * where the source state (src) matches the given `src`. Optionally, it provides a pointer
 * to the first such transition. Unlike the previous version, this implementation does
 * not assume contiguous grouping — it finds all matching transitions regardless of order.
 *
 * @param[in]  model     Pointer to the valid FSM model. Must not be NULL.
 * @param[in]  src       Source state identifier to filter transitions by.
 *                       If FSM_STATE_NONE, no transitions are matched.
 * @param[out] out_array Optional pointer to store the address of the first transition
 *                       that matches the source state. May be NULL if only count is needed.
 *
 * @return
 *   - Number of transitions found with the given source state;
 *   - 0 if no such transitions exist, model is invalid, or src is FSM_STATE_NONE.
 *
 * @note The returned pointer points directly into the model's transition array and must not be freed.
 *       Its validity is tied to the lifetime of the model.
 * @note The function performs a full linear scan and does not rely on any ordering assumptions.
 *       Suitable for models with unsorted or dynamically defined transitions.
 * @note If multiple transitions match, only the first encountered one is returned via out_array.
 *       Use manual iteration if you need access to all individual transition pointers.
 *
 * Example usage:
 * @code
 * const fsm_transition_desc_t *first_trans;
 * size_t count = fsm_model_get_transitions_from(model, STATE_A, &first_trans);
 * printf("Found %zu outgoing transitions from STATE_A\n", count);
 * if (count > 0) {
 *     printf("First: event=%d -> dst=%d\n", first_trans->event, first_trans->dst);
 * }
 * @endcode
 */
size_t fsm_model_get_transitions_from(const fsm_model_t *model,
                                      fsm_state_t src,
                                      const fsm_transition_desc_t **out_array) {
    if (!model || !model->transitions || model->transition_count == 0) {
        if (out_array) *out_array = NULL;
        return 0;
    }
    if (src == FSM_STATE_NONE) {
        if (out_array) *out_array = NULL;
        return 0;
    }

    const fsm_transition_desc_t *first_match = NULL;
    size_t count = 0;

    for (size_t i = 0; i < model->transition_count; ++i) {
        const fsm_transition_desc_t *t = &model->transitions[i];
        if (t->src == src) {
            if (!first_match) first_match = t;
            ++count;
        }
    }

    if (out_array) *out_array = first_match;
    return count;
}