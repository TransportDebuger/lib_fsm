/**
 * @file fsm_core.h
 * @brief Core API for a universal, embed-friendly Finite State Machine (FSM)
 * 
 * This header defines a minimal, high-performance FSM core suitable for real-time
 * and embedded systems. It provides:
 * - Event-driven state transitions
 * - Transition-specific entry/exit callbacks (Mealy-style)
 * - Recursion protection during event processing
 * - Opaque FSM handle for encapsulation
 * - Full C and C++ compatibility
 * 
 * @note This layer is independent of any model description (e.g. fsm_model.h)
 *       to ensure zero overhead when extended features are not used.
 * 
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2024-01-16
 * @version 1.0.0
 */

#ifndef FSM_CORE_H
#define FSM_CORE_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h> // size_t
#include <stdbool.h> // bool

/**
 * @defgroup core Core API
 * @brief Fundamental FSM functionality for event-driven state management
 *
 * This module provides the minimal, high-performance core of the FSM library.
 * It includes the basic types, transition engine, and lifecycle management
 * required to implement a finite state machine without dependencies on extended
 * features (e.g. model introspection, guards, or actions).
 *
 * @note This layer is designed to be embed-friendly: it uses no dynamic allocation
 *       (except via user-provided malloc), has no internal state, and supports
 *       real-time systems with deterministic execution.
 * @note The API is fully compatible with C++ (wraps in `extern "C"`).
 *
 * Example usage:
 * @code
 * const fsm_transition_t transitions[] = {
 *     { STATE_A, EV_GO_B, STATE_B, on_a_exit, on_b_enter },
 *     { STATE_B, EV_GO_A, STATE_A, on_b_exit, on_a_enter }
 * };
 *
 * fsm_t *fsm = fsm_new(transitions, 2, &my_context, STATE_A);
 * if (fsm) {
 *     fsm_process_event(fsm, EV_GO_B); // Triggers transition
 * }
 * @endcode
 * 
 * @{
 */

/**
 * @def FSM_EVENT_NONE
 * @brief Special event identifier indicating no event
 *
 * This constant is used to represent the absence of an event in the FSM.
 * It is commonly used in `fsm_update` to trigger time-based or periodic
 * transitions without relying on external input.
 *
 * @note This value must not conflict with valid user-defined event identifiers.
 * @warning Do not use this value as a regular event in transition rules
 *          unless implementing tick-based or internal update logic.
 * @sa fsm_update
 */
#define FSM_EVENT_NONE (-1)

/**
 * @def FSM_STATE_NONE
 * @brief Special state identifier indicating an invalid or uninitialized state
 *
 * Used to represent a null or undefined state in the FSM, such as:
 * - Initial value before initialization,
 * - Return value for invalid state queries,
 * - Sentinel value in state lookup operations.
 *
 * @note This value must not be used as a valid user-defined state identifier.
 * @warning Comparing states with `FSM_STATE_NONE` does not guarantee error detection
 *          if the FSM logic allows it as a legitimate state; use with care.
 * @sa fsm_get_state
 */
#define FSM_STATE_NONE (-1)

/**
 * @typedef fsm_state_t
 * @brief Identifier type for FSM states
 *
 * Represents a state in the finite state machine. Users should treat this as an opaque
 * identifier and define their own enum or constants for meaningful state names.
 *
 * @note Value `FSM_STATE_NONE` is reserved to indicate an invalid or uninitialized state.
 * @warning Do not rely on the underlying `int` type for comparisons beyond equality.
 *          Future versions may change the type for better range or portability.
 * @sa FSM_STATE_NONE
 */
typedef int fsm_state_t;

/**
 * @typedef fsm_event_t
 * @brief Identifier type for FSM events
 *
 * Represents an event that can trigger a state transition. Users should define
 * meaningful event identifiers using enums or constants.
 *
 * @note Value `FSM_EVENT_NONE` is reserved to indicate an invalid or null event.
 * @warning The underlying `int` type should not be used for arithmetic or bit manipulation.
 *          Future versions may change the type (e.g., to `uint16_t` or enum) for better
 *          portability and range control.
 * @sa FSM_EVENT_NONE
 */
typedef int fsm_event_t;

/**
 * @typedef fsm_t
 * @brief Opaque handle to an FSM instance
 *
 * Represents a finite state machine instance. This type is opaque to ensure encapsulation
 * and maintain ABI stability. Users must interact with the FSM only through the provided API.
 *
 * @note Memory for the FSM is managed by the user via `fsm_new()` and `fsm_free()`.
 * @warning Do not attempt to dereference or modify the contents of this type directly.
 *          Doing so results in undefined behavior.
 * @sa fsm_new, fsm_free
 */
typedef struct fsm fsm_t;

/**
 * @typedef fsm_context_t
 * @brief Opaque pointer to user-defined context data
 *
 * Represents a user-provided context passed to all callback functions. This allows
 * stateful behavior without relying on global variables.
 *
 * @note The FSM does not manage the lifetime of the context. The user must ensure
 *       that the pointed-to data remains valid for the entire lifetime of the FSM.
 * @warning Passing NULL is allowed only if the application logic safely handles it
 *          in all callbacks. Otherwise, undefined behavior may occur.
 * @sa fsm_new, fsm_set_context, fsm_get_context
 */
typedef void *fsm_context_t;

/**
 * @typedef fsm_cb_t
 * @brief Function signature for FSM callback functions
 *
 * Type definition for user-provided callbacks invoked during state transitions.
 * These are typically used for initialization, cleanup, or side effects on entry/exit.
 *
 * @param ctx Opaque user context passed to all callbacks (same as provided to `fsm_new`)
 *
 * @note Callbacks must not call `fsm_process_event` or `fsm_update` on the same FSM instance,
 *       as recursion is explicitly protected and may result in undefined behavior.
 * @warning Callbacks should be reentrant and avoid blocking operations, especially in real-time systems.
 * @sa fsm_new, fsm_process_event
 */
typedef void (*fsm_cb_t)(fsm_context_t ctx);

/**
 * @typedef fsm_transition_t
 * @brief Type representing a state transition rule
 *
 * Describes a single transition in the FSM: when in state `src` and event `event` occurs,
 * the machine transitions to state `dst`, optionally invoking entry and exit callbacks.
 *
 * @note The transition table must remain valid for the entire lifetime of the FSM,
 *       as it is not copied by `fsm_new`.
 * @warning Transitions are processed in array order; the first matching rule is taken.
 *          Ensure proper ordering when multiple rules could apply.
 * @sa fsm_new, struct fsm_transition
 */
typedef struct fsm_transition fsm_transition_t;

/**
 * @struct fsm_transition
 * @brief Definition of a state transition in the FSM
 *
 * Represents a directed transition from state `src` to state `dst` triggered by `event`.
 * Optionally, it can invoke `on_exit` before leaving the source state and `on_enter`
 * after entering the destination state (Mealy-style actions).
 *
 * @note The transition is only valid if:
 *       - `src` and `dst` are valid state identifiers,
 *       - `event` is a defined event,
 *       - The transition is part of a table passed to `fsm_new`.
 * @warning No duplicate or overlapping transitions are enforced by the core —
 *          the first matching entry in the table is used. Design the table with care.
 * @sa fsm_new, fsm_transition_t
 */
struct fsm_transition {
  fsm_state_t src;      ///< Source state identifier
  fsm_event_t event;    ///< Event that triggers the transition
  fsm_state_t dst;      ///< Destination state identifier
  fsm_cb_t on_exit;     ///< Callback invoked before leaving the source state (may be NULL)
  fsm_cb_t on_enter;    ///< Callback invoked after entering the destination state (may be NULL)
};

/**
 * @brief Create a new FSM instance
 *
 * Allocates and initializes a finite state machine with the given transition table.
 * The initial state is set to `start`, and the user context is bound for callbacks.
 *
 * @param transitions  Pointer to a static array of transition rules (must be valid for FSM lifetime)
 * @param count        Number of transition rules in the array (must be > 0)
 * @param ctx          User-defined context passed to all callbacks (may be NULL if not used)
 * @param start        Initial state identifier (must be a valid state in the transition graph)
 *
 * @return
 *   - Pointer to newly allocated `fsm_t` on success,
 *   - `NULL` if memory allocation fails or arguments are invalid.
 *
 * @pre `transitions != NULL` and `count > 0`
 * @pre `start != FSM_STATE_NONE`
 * @pre The transition table must include at least one rule with `src == start` or reachable from it
 *
 * @note The FSM does **not copy** the transition table — it stores a pointer.
 *       Therefore, the array must remain valid and unchanged during the entire lifetime of the FSM.
 * @note Always call `fsm_free()` to avoid memory leaks.
 * @warning Avoid passing `ctx == NULL` unless all callbacks are null-safe.
 *
 * @sa fsm_free, fsm_process_event
 */
fsm_t *fsm_new(const fsm_transition_t *transitions, size_t transitions_count, fsm_context_t ctx, fsm_state_t start);

/**
 * @brief Destroy and free an FSM instance
 *
 * Deallocates memory associated with the FSM instance. No further operations
 * should be performed on the FSM after calling this function.
 *
 * @param fsm  Pointer to the FSM instance to destroy (may be NULL)
 *
 * @pre None — function is safe to call with `fsm == NULL`
 *
 * @note Passing `NULL` is allowed and results in no operation (idempotent).
 * @note After calling `fsm_free`, the pointer becomes invalid and must not be dereferenced.
 * @note To prevent use-after-free bugs, always set the pointer to `NULL` after calling this function.
 *
 * @warning Failure to set `fsm = NULL` after deallocation may lead to undefined behavior
 *          if the pointer is accidentally reused.
 *
 * @sa fsm_new
 *
 * Example usage:
 * @code
 * fsm_t *fsm = fsm_new(transitions, count, ctx, STATE_INIT);
 * // ... use FSM ...
 * fsm_free(fsm);
 * fsm = NULL; // Safe practice
 * @endcode
 */
void fsm_free(fsm_t *fsm);

/**
 * @brief Reset FSM to its initial state
 *
 * Resets the current state of the FSM to the initial state provided during construction via `fsm_new`.
 * This is useful for reusing an FSM instance without reallocating memory.
 *
 * @param fsm  Pointer to the FSM instance (no effect if NULL)
 *
 * @note Does not invoke `on_exit` or `on_enter` callbacks — this is a silent state reset.
 * @note The user context (`fsm_context_t`) remains unchanged.
 * @note Safe to call with `fsm == NULL` — no operation is performed.
 *
 * @warning This function does not perform deep reset (e.g., of user context or external resources).
 *          Any side effects or state held outside the FSM must be reset separately.
 * @warning Only valid FSM instances should be passed; passing a previously freed pointer leads to undefined behavior.
 *
 * @sa fsm_new, fsm_get_state
 *
 * Example usage:
 * @code
 * fsm_reset(fsm); // Return to start state without callbacks
 * assert(fsm_get_state(fsm) == initial_state);
 * @endcode
 */
void fsm_reset(fsm_t *fsm);

/**
 * @brief Process an event and perform state transition if applicable
 *
 * Attempts to handle the given event by searching the transition table for a matching rule
 * from the current state and specified event. The first matching transition is executed.
 *
 * @param fsm    Pointer to the FSM instance
 * @param event  Event to process (should be a valid event identifier)
 * @return
 *   - `true` if a valid transition was found, executed, and state changed,
 *   - `false` if no transition matched, FSM is NULL, or recursion was detected.
 *
 * @pre `fsm` must point to a valid, initialized FSM instance
 *
 * @note The transition table is searched **in order** — the first matching entry is used.
 *       Ensure proper ordering when transitions could overlap.
 * @note Only one transition is ever processed per call, even if multiple rules match.
 * @note `on_exit` and `on_enter` callbacks are invoked if non-NULL in the transition rule.
 * @note This function is **reentrant**: it returns `false` if already processing an event,
 *       preventing recursion and potential stack overflow or state corruption.
 *
 * @warning Do not call `fsm_process_event` from within `on_exit` or `on_enter` callbacks.
 *          While recursion is protected, such calls will be silently ignored, which may
 *          lead to unexpected behavior.
 * @warning The transition table must remain valid and unchanged during execution.
 *
 * @sa fsm_new, fsm_update
 *
 * Example:
 * @code
 * if (fsm_process_event(fsm, EV_BUTTON_PRESS)) {
 *     // Transition succeeded — state has changed
 * } else {
 *     // No valid transition — possibly invalid event or state
 * }
 * @endcode
 */
bool fsm_process_event(fsm_t *fsm, fsm_event_t event);

/**
 * @brief Perform a periodic update tick on the FSM.
 *
 * Advances the internal logic of the finite state machine by processing a time-driven or tick-based event.
 * This function is typically called in a loop or from a timer interrupt to support:
 * - Time-based transitions (e.g., timeouts).
 * - State-specific update logic (e.g., animations, countdowns).
 *
 * The update is implemented as an internal event with `FSM_EVENT_NONE`, which allows transitions
 * to be triggered based on elapsed time or other conditions evaluated during the update.
 *
 * @param[in,out] fsm  Pointer to the FSM instance. No action is taken if NULL.
 *
 * @pre The `fsm` pointer must point to a valid, initialized FSM instance.
 *
 * @post If a transition is defined for the current state with `FSM_EVENT_NONE`, the FSM
 *       will change state and execute associated `on_exit` and `on_enter` actions.
 *
 * @note Designed for real-time systems: should return quickly and not block.
 * @note Reentrant: returns immediately if another event or update is already being processed.
 * @note Safe to call continuously, even if no transitions are defined for `FSM_EVENT_NONE`.
 * @note Thread-unsafe: external synchronization required in multithreaded environments.
 *
 * @warning Do not call `fsm_update` from within `on_exit` or `on_enter` callbacks.
 *          While recursion is prevented, such calls will be silently ignored, potentially leading
 *          to missed updates or unexpected behavior.
 * @warning The transition table (`fsm->transitions`) must remain valid and unchanged during execution.
 *
 * @sa fsm_process_event(), FSM_EVENT_NONE
 *
 * Example usage:
 * @code
 * while (running) {
 *     fsm_update(fsm); // Handle time-based logic
 *     delay_ms(10);
 * }
 * @endcode
 */
void fsm_update(fsm_t *fsm);

/**
 * @brief Get the current state of the FSM
 *
 * Returns the identifier of the currently active state in the finite state machine.
 *
 * @param fsm  Pointer to the FSM instance
 * @return
 *   - The current state identifier (`fsm_state_t`) if `fsm` is valid,
 *   - `FSM_STATE_NONE` if `fsm` is NULL or points to an invalid instance.
 *
 * @pre `fsm` must point to a valid, initialized FSM instance when non-NULL
 *
 * @note This function is safe to call at any time, including from interrupt handlers,
 *       provided the FSM object remains valid and no concurrent modifications occur.
 * @note The return value can be used for debugging, logging, or conditional logic.
 *
 * @warning Do not assume `FSM_STATE_NONE` always indicates an error — it may be a valid
 *          state identifier in some designs. Use only with well-defined state enums.
 * @warning Concurrent access from multiple threads or ISRs without synchronization
 *          may result in undefined behavior.
 *
 * @sa fsm_get_context, fsm_process_event
 *
 * Example:
 * @code
 * fsm_state_t current = fsm_get_state(fsm);
 * if (current == STATE_IDLE) {
 *     start_activity();
 * }
 * @endcode
 */
fsm_state_t fsm_get_state(const fsm_t *fsm);

/**
 * @brief Get the user-defined context associated with the FSM
 *
 * Returns the context pointer that was provided during FSM creation via `fsm_new`,
 * or subsequently updated using `fsm_set_context`.
 *
 * @param fsm  Pointer to the FSM instance
 * @return
 *   - Pointer to the user context (`fsm_context_t`) if `fsm` is valid,
 *   - `NULL` if `fsm` is NULL or uninitialized.
 *
 * @pre `fsm` must point to a valid, initialized FSM instance when non-NULL
 *
 * @note The returned context is not copied or managed by the FSM — ownership remains
 *       with the user. Ensure the pointed-to data remains valid while in use.
 * @note This function is typically used in callbacks or state logic to access
 *       application-specific data (e.g., configuration, sensors, UI state).
 *
 * @warning Do not dereference the returned pointer without confirming its validity
 *          and expected type. Always perform null checks in production code.
 * @warning Concurrent access to the context data from multiple threads or ISRs
 *          must be synchronized externally.
 *
 * @sa fsm_new, fsm_set_context, fsm_get_state
 *
 * Example:
 * @code
 * MyAppData* data = (MyAppData*)fsm_get_context(fsm);
 * if (data) {
 *     data->last_event_time = get_timestamp();
 * }
 * @endcode
 */
fsm_context_t fsm_get_context(fsm_t *fsm);

/**
 * @brief Set or update the user-defined context of the FSM
 *
 * Associates a new context pointer with the FSM instance. This context is passed
 * to all transition callbacks (`on_enter`, `on_exit`) and can be retrieved later
 * using `fsm_get_context`.
 *
 * @param fsm  Pointer to the FSM instance
 * @param ctx  User-defined context (can be a pointer to struct, object, or NULL)
 * @return
 *   - `true` if the context was successfully updated,
 *   - `false` if `fsm` is NULL.
 *
 * @pre `fsm` must point to a valid, initialized FSM instance
 *
 * @note It is legal to set `ctx` to `NULL`, provided all callbacks are null-safe.
 * @note The FSM does not manage the lifetime of the context — the user is responsible
 *       for ensuring the pointed-to data remains valid for as long as it is in use.
 * @note Safe to call at runtime to switch contexts (e.g., during state initialization
 *       or mode changes).
 *
 * @warning Do not pass an invalid or dangling pointer as `ctx`.
 * @warning Concurrent access to the context from multiple threads or ISRs must
 *          be externally synchronized.
 *
 * @sa fsm_new, fsm_get_context
 *
 * Example:
 * @code
 * MyStateData *new_data = allocate_state_data();
 * if (fsm_set_context(fsm, new_data)) {
 *     // Context successfully updated
 * } else {
 *     // Handle invalid FSM
 * }
 * @endcode
 */
bool fsm_set_context(fsm_t *fsm, fsm_context_t ctx);

/** @} */ // end of Core API

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FSM_CORE_H