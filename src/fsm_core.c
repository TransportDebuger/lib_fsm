/**
 * @file fsm_core.c
 * @brief FSM Core API realisation
 * 
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2024-01-16
 */

#include "fsm_core.h"

struct fsm {
  const fsm_transition_t *transitions;
  size_t count;
  fsm_state_t initial;
  fsm_state_t current;
  fsm_context_t ctx;
  bool processing;
};

fsm_t *fsm_new(const fsm_transition_t *transitions, size_t transitions_count, fsm_context_t ctx, fsm_state_t start) {
  // input params checking
  if (transitions == NULL || transitions_count == 0 || start == FSM_EVENT_NONE) { return NULL; }
  bool state_presents = false;
  for (size_t i = 0; i < transitions_count; i++) {
    if (transitions[i].src == start) { 
      state_presents = true; 
      break; 
    } 
  }
  if (!state_presents) { return NULL; }

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
  if (fsm != NULL) { free(fsm); }
}

void fsm_reset(fsm_t *fsm) {
  if (fsm != NULL) { 
    fsm->current = fsm->initial;
    fsm->processing = false;
  }
}

bool fsm_process_event(fsm_t *fsm, fsm_event_t event) {
  if (fsm == NULL || fsm->processing) { return false; }

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

void fsm_update(fsm_t *fsm) {
  if (fsm == NULL || fsm->processing) { return; }

  fsm->processing = true;
  for (size_t i = 0; i < fsm->count; ++i) {
    const fsm_transition_t *t = &fsm->transitions[i];
    if (t->src == fsm->current && t->event == FSM_EVENT_NONE) {
      if (t->on_exit) t->on_exit(fsm->ctx);
      fsm->current = t->dst;
      if (t->on_enter) t->on_enter(fsm->ctx);
      break;
    }
  }
  fsm->processing = false;
}

fsm_state_t fsm_get_state(const fsm_t *fsm) {
  if (fsm == NULL) return FSM_STATE_NONE;

  return fsm->current;
}

fsm_context_t fsm_get_context(fsm_t *fsm) {
  if (fsm == NULL) return NULL;

  return fsm->ctx;
}

bool fsm_set_context(fsm_t *fsm, fsm_context_t ctx){
  if (fsm == NULL) return false;

  fsm->ctx = ctx;
  return true;
}