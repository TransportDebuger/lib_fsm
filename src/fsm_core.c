/**
 * @file fsm_core.c
 * @brief Implementation of the FSM Core API
 *
 * This file contains the complete realization of the finite state machine
 * engine, including:
 * - FSM instance lifecycle management (`fsm_new`, `fsm_free`)
 * - Event processing with recursion protection
 * - State transitions with optional entry/exit callbacks
 * - Periodic update mechanism via `fsm_update`
 *
 * The implementation is designed to be:
 * - Minimal and embed-friendly (no dynamic allocations beyond user-facing
 * `fsm_new`)
 * - Reentrant (via `processing` flag)
 * - Deterministic (first-match transition semantics)
 *
 * @note This module does not depend on `fsm_model.h` or any extended metadata,
 *       ensuring zero overhead when advanced features are not used.
 *
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2024-01-16
 * @version 1.0.0
 */

#include "fsm_core.h"

/**
 * @internal
 * @struct fsm
 * @brief Finite State Machine (FSM) instance structure.
 *
 * This structure represents a single instance of a finite state machine.
 * It holds the transition table, current state, initial state, user context,
 * and execution state. The FSM processes events by matching them against
 * transitions from the current state.
 */
struct fsm {
  const fsm_transition_t *transitions; ///< Pointer to the array of transitions
                                       ///<   defining the FSM's behavior.
  size_t count;        ///< Number of transitions in the array.
  fsm_state_t initial; ///< Initial state of the FSM, set at initialization.
  fsm_state_t
      current;         ///< Current (active) state of the FSM during execution.
  fsm_context_t ctx;   ///< User-defined context passed to action functions during
                       ///<   transitions.
  bool processing;     ///< Flag indicating whether the FSM is currently processing
                       ///< an event (used to detect reentrancy).
};

fsm_t *fsm_new(const fsm_transition_t *transitions, size_t transitions_count,
               fsm_context_t ctx, fsm_state_t start) {
  // input params checking
  if (transitions == NULL || transitions_count == 0 ||
      start == FSM_STATE_NONE) {
    return NULL;
  }
  bool state_presents = false;
  for (size_t i = 0; i < transitions_count; i++) {
    if (transitions[i].src == start) {
      state_presents = true;
      break;
    }
  }
  if (!state_presents) {
    return NULL;
  }

  // creating FSM instance
  fsm_t *fsm = (fsm_t *)malloc(sizeof(fsm_t));
  if (fsm != NULL) {
    fsm->transitions = transitions;
    fsm->count = transitions_count;
    fsm->initial = start;
    fsm->current = start;
    fsm->ctx = ctx;
    fsm->processing = false;
  }
  return fsm;
}

void fsm_free(fsm_t *fsm) {
  if (fsm != NULL) {
    free(fsm);
  }
}

void fsm_reset(fsm_t *fsm) {
  if (fsm != NULL) {
    fsm->current = fsm->initial;
    fsm->processing = false;
  }
}

bool fsm_process_event(fsm_t *fsm, fsm_event_t event) {
  if (fsm == NULL || fsm->processing) {
    return false;
  }

  fsm->processing = true;
  for (size_t i = 0; i < fsm->count; ++i) {
    const fsm_transition_t *t = &fsm->transitions[i];
    if (t->src == fsm->current && t->event == event) {
      if (t->on_exit) t->on_exit(fsm->ctx);
      fsm->current = t->dst;
      if (t->on_enter) t->on_enter(fsm->ctx);
      fsm->processing = false;
      return true;
    }
  }
  fsm->processing = false;

  return false;
}

void fsm_update(fsm_t *fsm) { fsm_process_event(fsm, FSM_EVENT_NONE); }

fsm_state_t fsm_get_state(const fsm_t *fsm) {
  if (fsm == NULL) return FSM_STATE_NONE;

  return fsm->current;
}

fsm_context_t fsm_get_context(fsm_t *fsm) {
  if (fsm == NULL) return NULL;

  return fsm->ctx;
}

bool fsm_set_context(fsm_t *fsm, fsm_context_t ctx) {
  if (fsm == NULL) return false;

  fsm->ctx = ctx;
  return true;
}