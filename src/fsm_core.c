/**
 * @file fsm_core.c
 * @brief Realisation of FSM core
 * 
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2024-01-16
 */

#include "fsm_core.h"

struct fsm {
  const fsm_transition_t *transitions;
  size_t count;
  fsm_state_t current;
  fsm_context_t ctx;
  bool processing;
};