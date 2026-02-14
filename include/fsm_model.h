/**
 * @file fsm_model.h
 * @brief Extended model-based Finite State Machine (FSM) definition and
 * utilities
 *
 * This header defines a high-level, model-driven approach to FSM construction,
 * enabling declarative state machine design with rich metadata, validation,
 * introspection, and runtime generation. It builds upon the minimal core API
 * defined in `fsm_core.h`, adding:
 * - Descriptive structures for states, events, and transitions with user data
 * and callbacks.
 * - Hierarchical state support via parent-child relationships.
 * - Model validation with detailed error reporting.
 * - Dynamic FSM instance creation from validated models.
 * - Introspection functions for runtime analysis and debugging.
 *
 * The model layer is optional and can be omitted in resource-constrained
 * environments where only the core engine (`fsm_core.h`) is used. When
 * included, it enables safer, more maintainable FSMs suitable for complex
 * applications, diagnostics, and dynamic configuration.
 *
 * Key features:
 * - Full model validation (ID uniqueness, transition correctness, hierarchy
 * cycles).
 * - Automatic resolution of initial/final/error states via flags.
 * - Support for guards, actions, and user-defined context in transitions.
 * - Safe memory management: generated FSMs own their transition tables.
 * - Opaque interfaces with const-correctness for thread safety and integrity.
 *
 * Example usage:
 * @code
 * // Define your model (typically static or const)
 * const fsm_state_desc_t states[] = { ... };
 * const fsm_event_desc_t events[] = { ... };
 * const fsm_transition_desc_t transitions[] = { ... };
 *
 * const fsm_model_t model = {
 *   .states = states, .state_count = COUNT(states),
 *   .events = events, .event_count = COUNT(events),
 *   .transitions = transitions, .transition_count = COUNT(transitions)
 * };
 *
 * // Validate and create FSM
 * if (!fsm_model_validate(&model)) {
 *   // Handle error
 * }
 *
 * fsm_t *fsm = fsm_model_create_fsm(&model, &my_context, FSM_STATE_NONE);
 * if (fsm) {
 *   fsm_process_event(fsm, EV_START);
 *   fsm_free(fsm);
 * }
 * @endcode
 *
 * @note This module assumes the model data remains valid during FSM lifetime.
 *       Avoid freeing or modifying the model after `fsm_model_create_fsm`.
 * @note Designed for C99+ and C++ compatibility (wrapped in `extern "C"`).
 * @note All find/has functions perform linear search — O(n). For
 * performance-critical paths, cache results or use direct array access when
 * possible.
 *
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-02-07
 * @version 1.0.0
 */

#ifndef FSM_MODEL_H
#define FSM_MODEL_H

#include <stdint.h>

#include "fsm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Flags FSM Model Flags
 * @brief Bitmask flags for states, events, and transitions metadata
 *
 * This group defines a set of bitmasks used to annotate state machine elements
 * (states, events, transitions) with additional runtime behavior, visibility,
 * and semantic meaning. Flags are stored in the `flags` field of respective
 * descriptor structures and can be combined using bitwise OR.
 *
 * The flag space is divided into two ranges:
 * - Library-reserved (upper 8 bits): Used by the FSM framework for internal
 * logic.
 * - User-available (lower 24 bits): Available for application-specific
 * extensions.
 *
 * @note Always use masks (@ref FSM_FLAGS_LIB_MASK, @ref FSM_FLAGS_USER_MASK)
 *       when testing or clearing flags to avoid interference with reserved
 * bits.
 * @note Flags do not alter core FSM behavior directly — they are intended for
 *       use in guards, actions, logging, UI, diagnostics, and custom logic.
 * @note When defining custom flags, ensure they do not overlap with existing
 * ones.
 *
 * Example usage:
 * @code
 * if (state->flags & FSM_STATE_FLAG_INITIAL) {
 *     printf("Initial state: %s\n", state->name);
 * }
 *
 * // Check only user-defined flags
 * uint32_t user_flags = my_state->flags & FSM_FLAGS_USER_MASK;
 * if (user_flags & MY_APP_CRITICAL_STATE) { ... }
 * @endcode
 * @{
 */

/**
 * @def FSM_FLAGS_LIB_MASK
 * @brief Bitmask for library-reserved flags (upper 8 bits)
 *
 * This mask covers the most significant 8 bits (bits 24–31) of the 32-bit flags
 * field, reserved for use by the FSM library. These flags control internal
 * behavior, state semantics, and model validation.
 *
 * @note Applications must not define custom flags in this range to avoid
 * conflicts with current or future library extensions.
 * @note When modifying or copying flags, preserve library bits unless
 * explicitly intended.
 * @warning Violating this restriction may lead to undefined behavior, failed
 * validation, or broken state machine logic.
 *
 * Example:
 * @code
 * // Safe: isolate user flags for application logic
 * if ((state->flags & FSM_FLAGS_USER_MASK) & MY_CUSTOM_FLAG) { ... }
 *
 * // Unsafe: accidentally clearing library flags
 * state->flags &= ~MY_CUSTOM_FLAG; // May clear FSM_STATE_FLAG_INITIAL!
 *
 * // Correct: preserve library flags
 * state->flags = (state->flags & FSM_FLAGS_LIB_MASK) | new_user_flags;
 * @endcode
 *
 * @see FSM_FLAGS_USER_MASK
 */
#define FSM_FLAGS_LIB_MASK 0xFF000000u

/**
 * @def FSM_FLAGS_USER_MASK
 * @brief Mask for user-defined flags (lower 24 bits)
 *
 * This mask covers the lower 24 bits (bits 0–23) of the 32-bit flags field,
 * reserved for application-specific use. It allows users to define custom
 * behavior, logging levels, UI hints, or domain-specific attributes without
 * interfering with library-reserved flags in the upper 8 bits.
 *
 * @note Always combine this mask when reading or modifying user flags to avoid
 *       unintentionally altering system behavior controlled by @ref
 * FSM_FLAGS_LIB_MASK.
 * @note When setting custom flags, preserve library bits:
 *       @code
 *       obj->flags = (obj->flags & FSM_FLAGS_LIB_MASK) | MY_USER_FLAG;
 *       @endcode
 * @note This mask enables safe extensibility: the library can add new internal
 * flags without breaking existing user code.
 *
 * Example usage:
 * @code
 * #define MYAPP_FLAG_DEBUG_TRACE  0x00000001u
 * #define MYAPP_FLAG_SECURE       0x00000002u
 *
 * if (state->flags & MYAPP_FLAG_SECURE) {
 *     enforce_security_policy();
 * }
 * @endcode
 *
 * @see FSM_FLAGS_LIB_MASK
 */
#define FSM_FLAGS_USER_MASK 0x00FFFFFFu

/**
 * @def FSM_STATE_FLAG_INITIAL
 * @brief Indicates the initial state of the FSM
 *
 * This flag marks a state as the starting point for the finite state machine.
 * During model validation (@ref fsm_model_validate), the system checks that
 * exactly one state in the model carries this flag. Having zero or multiple
 * initial states results in a validation error.
 *
 * @note Required for models where no start state is passed explicitly to
 *       @ref fsm_model_create_fsm. If a start state is provided, it overrides
 *       the flagged initial state.
 * @note The presence of this flag enables automatic initialization and improves
 *       model self-description for debugging, serialization, or UI tools.
 * @note Must be used with states that are otherwise valid (unique ID, proper
 * hierarchy).
 * @warning Defining more than one state with this flag will cause model
 * validation to fail.
 * @warning Removing this flag without providing an explicit start state may
 * make the model unusable during FSM creation.
 *
 * Example:
 * @code
 * const fsm_state_desc_t states[] = {
 *     { .id = STATE_IDLE, .name = "idle", .flags = FSM_STATE_FLAG_INITIAL, ...
 * }, { .id = STATE_BUSY, .name = "busy", .flags = 0, ... }
 * };
 * // Valid: exactly one initial state
 * @endcode
 */
#define FSM_STATE_FLAG_INITIAL 0x01000000u

/**
 * @def FSM_STATE_FLAG_FINAL
 * @brief Indicates a final (terminal) state in the FSM
 *
 * This flag marks a state as a terminal point in the state machine’s lifecycle.
 * While not required for normal operation, it is used during model validation
 * and can be leveraged by analysis tools, debuggers, or runtime logic to detect
 * completion or termination conditions.
 *
 * @note The model validation (@ref fsm_model_validate) ensures that at most one
 *       state is marked with this flag. Multiple final states will cause
 * validation to fail.
 * @note Unlike @ref FSM_STATE_FLAG_INITIAL, a final state is optional — models
 * may have zero.
 * @note Useful for workflow-driven systems where reaching a final state means
 * task completion.
 * @note Can be used in conjunction with guards or logging to trigger cleanup or
 * notifications.
 * @warning Do not rely solely on this flag for critical termination logic
 * unless your use case explicitly requires single final state semantics.
 * @warning Changing the final state dynamically at runtime (by modifying flags)
 * is undefined behavior and not supported.
 *
 * Example:
 * @code
 * const fsm_state_desc_t states[] = {
 *     { .id = STATE_READY,    .name = "ready",   .flags = 0, ... },
 *     { .id = STATE_PROCESS,  .name = "process", .flags = 0, ... },
 *     { .id = STATE_DONE,     .name = "done",    .flags = FSM_STATE_FLAG_FINAL,
 * ... }
 * };
 *
 * // Later, check if FSM has reached final state:
 * const fsm_state_desc_t *current = fsm_model_find_state(model,
 * fsm_get_state(fsm)); if (current && (current->flags & FSM_STATE_FLAG_FINAL))
 * { log_info("Workflow completed");
 * }
 * @endcode
 */
#define FSM_STATE_FLAG_FINAL 0x02000000u

/**
 * @def FSM_STATE_FLAG_ERROR
 * @brief Error state flag
 *
 * Explicit indication of error state by flag.
 * It must be helpfull for debugging, diagnostics, and restoration logic.
 */
#define FSM_STATE_FLAG_ERROR 0x04000000u

/**
 * @def FSM_STATE_FLAG_ABSTRACT
 * @brief Abstract state flag
 *
 * State that exist as parent for other states (super-state) and it will have
 * never used as current state.
 */
#define FSM_STATE_FLAG_ABSTRACT 0x08000000u

/**
 * @def FSM_STATE_FLAG_HIDDEN
 * @brief Hidden state flag
 *
 * Technical or service state.
 *
 * @note Hidden states are not shown in UI/logging/diagrams by default.
 */
#define FSM_STATE_FLAG_HIDDEN 0x10000000u

/**
 * @def FSM_EVENT_FLAG_INTERNAL
 * @brief Internal event flag
 *
 * Internal event flag, generated by FSM itself.
 */
#define FSM_EVENT_FLAG_INTERNAL 0x01000000u

/**
 * @def FSM_EVENT_FLAG_TIMER
 * @brief Timer event flag
 *
 * Timer event flag, generated by timer.
 */
#define FSM_EVENT_FLAG_TIMER 0x02000000u

/**
 * @def FSM_EVENT_FLAG_ERROR
 * @brief Error event flag
 *
 * Error event flag, generated by FSM itself.
 */
#define FSM_EVENT_FLAG_ERROR 0x04000000u

/**
 * @def FSM_EVENT_FLAG_ASYNC
 * @brief Async event flag
 *
 * Async event flag, generated by external event.
 */
#define FSM_EVENT_FLAG_ASYNC 0x08000000u

/**
 * @def FSM_EVENT_FLAG_HIDDEN
 * @brief Hidden event flag
 *
 * Hidden event flag, not shown in UI/logging/diagrams by default.
 */
#define FSM_EVENT_FLAG_HIDDEN 0x10000000u

/**
 * @def FSM_EVENT_FLAG_IMPORTANT
 * @brief Important event flag
 *
 * Important event flag, for logging/trace purposes.
 */
#define FSM_EVENT_FLAG_IMPORTANT 0x20000000u

/**
 * @def FSM_TRANS_FLAG_INTERNAL
 * @brief Internal transition flag
 *
 * Internal transition flag, generated by FSM itself.
 */
#define FSM_TRANS_FLAG_INTERNAL 0x01000000u

/**
 * @def FSM_TRANS_FLAG_SILENT
 * @brief Silent transition flag
 *
 * Silent transition flag, not shown in UI/logging/diagrams by default.
 */
#define FSM_TRANS_FLAG_SILENT 0x02000000u

/**
 * @def FSM_TRANS_FLAG_LOG
 * @brief Log transition flag
 *
 * Log transition flag, always log with details.
 */
#define FSM_TRANS_FLAG_LOG 0x04000000u

/**
 * @def FSM_TRANS_FLAG_GUARDED
 * @brief Guarded transition flag
 *
 * Guarded transition flag, has guard callback.
 */
#define FSM_TRANS_FLAG_GUARDED 0x08000000u

/**
 * @def FSM_TRANS_FLAG_SELF
 * @brief Self transition flag
 *
 * Self transition flag, transition to the same state.
 */
#define FSM_TRANS_FLAG_SELF 0x10000000u

/**
 * @def FSM_TRANS_FLAG_DISABLED
 * @brief Disabled transition flag
 *
 * Disabled transition flag, transition is ignored.
 */
#define FSM_TRANS_FLAG_DISABLED 0x20000000u
/** @} */  // end of Flags

/**
 * @todo May extend with event driven handler
 *       typedef void (*fsm_event_cb_t)(fsm_context_t ctx);
 */

/**
 * @typedef fsm_state_desc_t
 * @brief Opaque type for state descriptor structure
 *
 * Represents a user-defined description of a state in the FSM model. This type
 * is used to declare and initialize state metadata such as ID, name, callbacks,
 * and flags.
 *
 * @note The actual definition is provided by @ref struct fsm_state_desc.
 *       Users should treat this as a complete struct type, not a pointer.
 * @note All instances must remain valid for the lifetime of the associated FSM
 * model, as the model stores pointers to these descriptors.
 * @note Designed for static initialization — typically defined as `const`
 * arrays.
 *
 * Example:
 * @code
 * const fsm_state_desc_t my_states[] = {
 *     { .id = STATE_IDLE, .name = "idle", .on_enter_cb = on_idle_enter, ... },
 *     { .id = STATE_BUSY, .name = "busy", .on_exit_cb  = on_busy_exit,  ... }
 * };
 * @endcode
 *
 * @see struct fsm_state_desc for full field documentation.
 */
typedef struct fsm_state_desc fsm_state_desc_t;

/**
 * @typedef fsm_event_desc_t
 * @brief Opaque type for event descriptor structure
 *
 * Represents a user-defined description of an event in the FSM model. This type
 * is used to declare and initialize event metadata such as ID, name, flags, and
 * optional user data.
 *
 * @note The actual definition is provided by @ref struct fsm_event_desc.
 *       Users should treat this as a complete struct type, not a pointer.
 * @note All instances must remain valid for the lifetime of the associated FSM
 * model, as the model stores pointers to these descriptors.
 * @note Designed for static initialization — typically defined as `const`
 * arrays.
 * @note While optional for basic operation, event descriptors enable
 * introspection, logging, debugging, and runtime analysis when used with
 * model-aware tools.
 *
 * Example:
 * @code
 * const fsm_event_desc_t my_events[] = {
 *     { .id = EV_START, .name = "start", .flags = 0, .user_data = NULL },
 *     { .id = EV_DONE,  .name = "done",  .flags = FSM_EVENT_FLAG_FINAL }
 * };
 * @endcode
 *
 * @see struct fsm_event_desc for full field documentation.
 */
typedef struct fsm_event_desc fsm_event_desc_t;

/**
 * @typedef fsm_transition_desc_t
 * @brief Opaque type for transition descriptor structure
 *
 * Represents a user-defined description of a state transition in the FSM model.
 * This type is used to declare and initialize full-featured transitions
 * including source, destination, event, guard and action callbacks, flags, and
 * user data.
 *
 * @note The actual definition is provided by @ref struct fsm_transition_desc.
 *       Users should treat this as a complete struct type, not a pointer.
 * @note All instances must remain valid for the lifetime of the associated FSM
 * model, as the model stores pointers to these descriptors.
 * @note Designed for static initialization — typically defined as `const`
 * arrays.
 * @note Unlike core transitions (@ref fsm_transition_t), this extended version
 * supports guards, user data, and metadata (flags), enabling rich runtime
 * behavior and introspection.
 * @note Transitions are processed in array order — the first matching rule is
 * taken. Ensure proper ordering when multiple rules could apply.
 *
 * Example:
 * @code
 * const fsm_transition_desc_t my_transitions[] = {
 *     {
 *         .src = STATE_IDLE,
 *         .event = EV_START,
 *         .dst = STATE_RUNNING,
 *         .guard_cb = can_start,
 *         .action_cb = on_start,
 *         .flags = 0,
 *         .user_data = NULL
 *     },
 *     {
 *         .src = STATE_RUNNING,
 *         .event = EV_STOP,
 *         .dst = STATE_IDLE,
 *         .guard_cb = NULL,
 *         .action_cb = on_stop,
 *         .flags = FSM_TRANS_FLAG_LOG
 *     }
 * };
 * @endcode
 *
 * @see struct fsm_transition_desc for full field documentation.
 * @see fsm_model_build_core_transitions for how these are converted to runtime
 * transitions.
 */
typedef struct fsm_transition_desc fsm_transition_desc_t;

/**
 * @typedef fsm_model_t
 * @brief Opaque type for the FSM model descriptor
 *
 * Represents a complete, self-contained definition of a finite state machine,
 * acting as a central repository (warehouse) for states, events, and
 * transitions. This structure enables declarative FSM design, validation,
 * introspection, and dynamic instantiation.
 *
 * @note The actual definition is provided by @ref struct fsm_model.
 *       Users should treat this as a complete struct type, not a pointer.
 * @note The model does not own the memory of its referenced arrays — all
 * pointers (states, events, transitions) must remain valid for the lifetime of
 * any FSM created from this model.
 * @note Designed for static or const initialization, enabling read-only models
 *       suitable for embedded and safety-critical systems.
 * @note Required for advanced features like model validation (@ref
 * fsm_model_validate), runtime inspection, hierarchical state support, and
 * automatic FSM creation via @ref fsm_model_create_fsm.
 *
 * Example:
 * @code
 * const fsm_model_t my_fsm_model = {
 *     .states        = my_states,
 *     .state_count   = COUNT(my_states),
 *     .events        = my_events,
 *     .event_count   = COUNT(my_events),
 *     .transitions   = my_transitions,
 *     .transition_count = COUNT(my_transitions)
 * };
 *
 * // Validate and create FSM instance
 * if (!fsm_model_validate(&my_fsm_model)) {
 *     log_error("Invalid FSM model");
 *     return;
 * }
 *
 * fsm_t *fsm = fsm_model_create_fsm(&my_fsm_model, &app_ctx, FSM_STATE_NONE);
 * @endcode
 *
 * @see struct fsm_model for full field documentation.
 * @see fsm_model_create_fsm for model-to-runtime conversion.
 */
typedef struct fsm_model fsm_model_t;

/**
 * @brief Function pointer type for guard conditions in a transition.
 *
 * A guard callback is a user-defined function that determines whether a
 * transition should be taken based on runtime conditions. It is evaluated when
 * an event occurs and the corresponding transition is considered.
 *
 * @param ctx Opaque user context passed to the FSM, typically containing
 * application-specific data or state needed to evaluate the guard condition.
 *
 * @return true if the transition is allowed (guard passed),
 *         false if the transition should be blocked (guard failed).
 *
 * @note Guard callbacks must be pure or idempotent — they should not modify
 * critical state or perform side effects, as their execution is part of the
 * transition check phase.
 * @note This function is called during event processing; avoid long-running or
 * blocking operations, especially in real-time or embedded systems.
 * @note If multiple transitions match the current state and event, guards are
 * evaluated in the order of the transition table until one passes.
 *
 * Example:
 * @code
 * bool can_proceed_if_ready(fsm_context_t ctx) {
 *     MyAppData *data = (MyAppData*)ctx;
 *     return data->is_ready;
 * }
 * @endcode
 *
 * @see fsm_transition_desc_t for use in transition definitions.
 */
typedef bool (*fsm_guard_cb_t)(fsm_context_t ctx);

/**
 * @brief Function pointer type for action callbacks in a transition.
 *
 * An action callback is a user-defined function that is executed when a
 * transition is taken, typically used to perform side effects such as updating
 * variables, triggering I/O, sending messages, or logging. It runs after the
 * source state's exit callback and before the destination state's entry
 * callback.
 *
 * @param ctx Opaque user context passed to the FSM, containing
 * application-specific data needed to perform the action.
 *
 * @note Action callbacks should be short and non-blocking, especially in
 * real-time or embedded systems.
 * @note These functions are called during the transition phase — avoid calling
 *       `fsm_process_event` or `fsm_update` on the same FSM instance from
 * within an action, as recursion is protected and may lead to ignored events or
 * undefined behavior.
 * @note If multiple transitions are eligible, only the action of the first
 * matching and passing (via guard) transition is executed.
 *
 * Example:
 * @code
 * void log_state_change(fsm_context_t ctx) {
 *     MyAppData *data = (MyAppData*)ctx;
 *     printf("State changed to %d\n", fsm_get_state(data->fsm));
 * }
 * @endcode
 *
 * @see fsm_transition_desc_t for use in transition definitions.
 */
typedef void (*fsm_action_cb_t)(fsm_context_t ctx);

/**
 * @struct fsm_state_desc
 * @brief Extended state descriptor with metadata, hierarchy, and behavior
 * callbacks
 *
 * Defines a complete description of a state in the FSM model, including its
 * identity, hierarchical relationship, runtime behavior (callbacks), and
 * user-defined data. This structure is used to build validated, introspectable
 * state machines via @ref fsm_model_t.
 *
 * @note All instances should be declared as `const` and initialized statically
 * for safety and compatibility with embedded systems.
 * @note The `id` field must be unique across all states in the model;
 * duplicates will cause validation to fail (@ref fsm_model_validate).
 * @note The `parent` field enables hierarchical FSMs (HFSM): if set to a valid
 * state ID, this state becomes a substate. Use @ref FSM_STATE_NONE for
 * top-level states.
 * @note Callbacks (`on_enter_cb`, `on_exit_cb`, `on_update_cb`) are optional —
 * set to `NULL` if not needed. They are invoked with the FSM's context (@ref
 * fsm_context_t).
 * @note The `on_update_cb` is called on every call to @ref fsm_update, enabling
 * time-driven logic such as animations, timeouts, or polling.
 * @note The `user_data` pointer is never touched by the FSM core — it is solely
 * for application use. Ownership and lifetime must be managed by the user.
 * @note String fields (`name`, `description`) are optional but highly
 * recommended for debugging, logging, and visualization tools.
 *
 * Example:
 * @code
 * const fsm_state_desc_t state_idle = {
 *     .id          = STATE_IDLE,
 *     .name        = "idle",
 *     .description = "Waiting for start event",
 *     .parent      = FSM_STATE_NONE,
 *     .flags       = FSM_STATE_FLAG_INITIAL,
 *     .on_enter_cb = on_idle_enter,
 *     .on_exit_cb  = NULL,
 *     .on_update_cb = check_for_autostart,
 *     .user_data   = NULL
 * };
 * @endcode
 *
 * @see fsm_model_t for model-level usage.
 * @see fsm_state_t for valid state identifier semantics.
 */
struct fsm_state_desc {
  fsm_state_t
      id;  ///< Unique state identifier; must be distinct within the model
  const char
      *name;  ///< Null-terminated name of the state (optional, but recommended)
  const char *description;  ///< Human-readable description (optional)
  fsm_state_t parent;  ///< Parent state ID; use FSM_STATE_NONE for root states
  uint32_t flags;      ///< Bitmask of FSM_STATE_FLAG_* flags
  fsm_cb_t on_enter_cb;   ///< Called when entering the state (may be NULL)
  fsm_cb_t on_exit_cb;    ///< Called when exiting the state (may be NULL)
  fsm_cb_t on_update_cb;  ///< Called on every fsm_update() call while in this
                          ///< state (may be NULL)
  void *
      user_data;  ///< Application-specific data (may be NULL; not owned by FSM)
};

/**
 * @struct fsm_event_desc
 * @brief Complete event descriptor with metadata and extensibility support
 *
 * Defines a full description of an event in the FSM model, including its
 * identity, semantic attributes (flags), human-readable information, and
 * user-defined data. This structure enables introspection, logging, and
 * advanced event handling policies.
 *
 * @note All instances should be declared as `const` and initialized statically
 * for safety and compatibility with embedded systems.
 * @note The `id` field must be unique across all events in the model;
 * duplicates will cause validation to fail (@ref fsm_model_validate).
 * @note String fields (`name`, `description`) are optional but highly
 * recommended for debugging, runtime inspection, and visualization tools.
 * @note The `flags` field can include predefined values like @ref
 * FSM_EVENT_FLAG_INTERNAL,
 *       @ref FSM_EVENT_FLAG_ASYNC, or user-defined bits via @ref
 * FSM_FLAGS_USER_MASK.
 * @note The `user_data` pointer is never touched by the FSM core — it is solely
 * for application use. Ownership and lifetime must be managed by the user. It
 * can point to event-specific parameters, configuration, or context.
 * @note The `reserved` field is provided for future expansion (e.g., attaching
 * dispatch callbacks, priority queues, or payload types) without breaking ABI.
 * It must be set to NULL unless explicitly extended by a compatible
 * implementation.
 *
 * Example:
 * @code
 * const fsm_event_desc_t ev_start = {
 *     .id          = EV_START,
 *     .name        = "start",
 *     .description = "Request to start the process",
 *     .flags       = FSM_EVENT_FLAG_USER | FSM_EVENT_FLAG_IMPORTANT,
 *     .user_data   = NULL,
 *     .reserved    = NULL
 * };
 * @endcode
 *
 * @see fsm_model_t for model-level usage.
 * @see fsm_event_t for valid event identifier semantics.
 */
struct fsm_event_desc {
  fsm_event_t
      id;  ///< Unique event identifier; must be distinct within the model
  const char
      *name;  ///< Null-terminated name of the event (optional, but recommended)
  const char *description;  ///< Human-readable description (optional)
  uint32_t flags;           ///< Bitmask of FSM_EVENT_FLAG_* flags
  void *user_data;  ///< Application-specific data associated with the event
                    ///< (may be NULL)
  void *reserved;   ///< Reserved for future use — set to NULL
};

/**
 * @struct fsm_transition_desc
 * @brief Extended transition descriptor with guard, action, and metadata
 * support
 *
 * Defines a complete state transition rule in the FSM model, including source
 * and destination states, triggering event, optional guard and action
 * callbacks, flags, and user data. This structure enables rich, introspectable
 * transitions beyond the basic core FSM engine.
 *
 * @note All instances should be declared as `const` and initialized statically
 * for safety and compatibility with embedded systems.
 * @note The transition is evaluated when the FSM is in the `src` state and the
 * `event` is processed. If multiple transitions match, the first one (in array
 * order) whose guard allows it is taken.
 * @note The `guard_cb` is called first — if it returns `false`, the transition
 * is blocked. If no guard is set (`NULL`), the transition is allowed
 * unconditionally.
 * @note The `action_cb` is executed after the source state's exit callback and
 * before the destination state's entry callback, enabling mid-transition logic.
 * @note The `flags` field can include predefined values like @ref
 * FSM_TRANS_FLAG_LOG,
 *       @ref FSM_TRANS_FLAG_SILENT, or custom user flags via @ref
 * FSM_FLAGS_USER_MASK.
 * @note The `user_data` pointer is never touched by the FSM core — it is solely
 * for application use. It can carry parameters, context, or configuration
 * specific to this transition.
 * @note This structure is used in @ref fsm_model_t and converted to @ref
 * fsm_transition_t at runtime via @ref fsm_model_build_core_transitions.
 *
 * Example:
 * @code
 * const fsm_transition_desc_t trans_start = {
 *     .src       = STATE_IDLE,
 *     .event     = EV_START,
 *     .dst       = STATE_RUNNING,
 *     .guard_cb  = can_start_process,
 *     .action_cb = on_start_action,
 *     .flags     = FSM_TRANS_FLAG_LOG,
 *     .user_data = &start_config
 * };
 * @endcode
 *
 * @see fsm_model_t for model-level usage.
 * @see fsm_guard_cb_t, fsm_action_cb_t for callback semantics.
 * @see fsm_model_build_core_transitions for conversion to runtime format.
 */
struct fsm_transition_desc {
  fsm_state_t src;    ///< Source state ID; must exist in the model
  fsm_event_t event;  ///< Event that triggers the transition; must exist or be
                      ///< FSM_EVENT_NONE
  fsm_state_t dst;    ///< Destination state ID; must exist in the model
  fsm_guard_cb_t guard_cb;  ///< Optional guard function: if present and returns
                            ///< false, transition is blocked
  fsm_action_cb_t action_cb;  ///< Optional action function: called during
                              ///< transition (after exit, before enter)
  uint32_t flags;             ///< Bitmask of FSM_TRANS_FLAG_* flags
  void *user_data;            ///< Application-specific data associated with the
                              ///< transition (may be NULL)
};

/**
 * @struct fsm_model
 * @brief Central repository for FSM metadata and structure definition
 *
 * This structure encapsulates the complete declarative model of a finite state
 * machine, including all states, events, and transitions. It serves as the
 * source of truth for constructing, validating, and introspecting an FSM
 * instance at runtime.
 *
 * @note The model is data-driven and does not own the memory of its referenced
 * arrays. All pointers must remain valid for the lifetime of any FSM created
 * from this model.
 * @note Designed for static initialization (typically `const`), enabling use in
 * embedded and safety-critical systems where dynamic configuration is not
 * required.
 * @note Transition evaluation is order-dependent: the first matching transition
 * (by array index) whose guard condition passes will be executed. Order matters
 * when multiple rules could apply.
 * @note The model supports hierarchical FSMs (HFSM) via the `parent` field in
 * @ref fsm_state_desc_t.
 * @note Validation using @ref fsm_model_validate or @ref fsm_model_validate_ex
 * is strongly recommended before creating an FSM to catch configuration errors
 * such as duplicate IDs or invalid references.
 * @note This structure is immutable during FSM execution — modifications after
 * instantiation lead to undefined behavior.
 *
 * Example usage:
 * @code
 * const fsm_model_t door_fsm_model = {
 *     .states         = door_states,
 *     .state_count    = COUNT(door_states),
 *     .events         = door_events,
 *     .event_count    = COUNT(door_events),
 *     .transitions    = door_transitions,
 *     .transition_count = COUNT(door_transitions)
 * };
 *
 * if (!fsm_model_validate(&door_fsm_model)) {
 *     log_error("Invalid FSM model detected");
 *     return -1;
 * }
 *
 * fsm_t *fsm = fsm_model_create_fsm(&door_fsm_model, &app_ctx, FSM_STATE_NONE);
 * if (!fsm) {
 *     log_error("Failed to create FSM instance");
 *     return -1;
 * }
 * @endcode
 *
 * @see fsm_model_validate, fsm_model_validate_ex — for model integrity
 * checking.
 * @see fsm_model_create_fsm — for instantiating a runtime FSM from this model.
 * @see fsm_model_find_state, fsm_model_find_event — for runtime introspection.
 */
struct fsm_model {
  const fsm_state_desc_t *states;  ///< Pointer to array of state descriptors;
                                   ///< may be NULL only if state_count == 0
  size_t state_count;              ///< Number of states in the array
  const fsm_event_desc_t *events;  ///< Pointer to array of event descriptors;
                                   ///< may be NULL only if event_count == 0
  size_t event_count;              ///< Number of events in the array
  const fsm_transition_desc_t
      *transitions;  ///< Pointer to array of transition descriptors; may be
                     ///< NULL only if transition_count == 0
  size_t transition_count;  ///< Number of transitions in the array
};

bool fsm_model_validate(const fsm_model_t *model);
bool fsm_model_validate_ex(const fsm_model_t *model, uint32_t *error_code,
                           const char **error_msg);

bool fsm_model_has_state(const fsm_model_t *m, fsm_state_t id);
const fsm_state_desc_t *fsm_model_find_state(const fsm_model_t *model,
                                             fsm_state_t id);
bool fsm_model_has_event(const fsm_model_t *model, fsm_event_t id);
const fsm_event_desc_t *fsm_model_find_event(const fsm_model_t *model,
                                             fsm_event_t id);
const fsm_transition_desc_t *fsm_model_find_transition(const fsm_model_t *model,
                                                       fsm_state_t src,
                                                       fsm_event_t event);

fsm_state_t fsm_model_get_parent_state(const fsm_model_t *model,
                                       fsm_state_t state);
bool fsm_model_is_ancestor_state(const fsm_model_t *model, fsm_state_t ancestor,
                                 fsm_state_t state);

fsm_state_t fsm_model_get_initial_state(const fsm_model_t *model);
bool fsm_model_has_final_state(const fsm_model_t *model);

bool fsm_model_build_core_transitions(const fsm_model_t *model,
                                      fsm_transition_t *out_transitions,
                                      size_t max_count, size_t *out_count);
fsm_t *fsm_model_create_fsm(const fsm_model_t *model, fsm_context_t ctx,
                            fsm_state_t start);

size_t fsm_model_get_transitions_from(const fsm_model_t *model, fsm_state_t src,
                                      const fsm_transition_desc_t **out_array);

#ifdef __cplusplus
}
#endif

#endif  // FSM_MODEL_H