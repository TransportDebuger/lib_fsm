/**
 * @file fsm_core.h
 * @brief Core functionality for the universal Finite State Machine (FSM)
 * 
 * Core API is a base layer for FSM implementation.
 * It designed for easy integration with any project, and realizes the following:
 * - event driven state machine
 * - transition oriented callbacks on enter/exit for handling state changes.
 * - recursion protection for event handling
 * 
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2024-01-16
 */

#ifndef FSM_CORE_H
#define FSM_CORE_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h> // size_t
#include <stdbool.h> // bool

#define FSM_EVENT_NONE (-1)

#define FSM_STATE_NONE (-1)

/** @typedef fsm_state_t
 * @brief State identifier type
 */
typedef int fsm_state_t;

/** @typedef fsm_event_t
 * @brief Event identifier type
 */
typedef int fsm_event_t;

/** @typedef fsm_t 
 * @brief Opaque FSM type
*/
typedef struct fsm fsm_t;

/** @typedef fsm_context_t 
 * @brief Opaque FSM context type
*/
typedef void *fsm_context_t;

/** @typedef fsm_cb_t 
 * @brief Callback type
 */
typedef void (*fsm_cb_t)(fsm_context_t ctx);

/** @typedef fsm_transition_t
 * @brief Transition rule type
 */
typedef struct fsm_transition fsm_transition_t;

/** @struct fsm_transition
 * @brief Transition rule
 */
struct fsm_transition {
  fsm_state_t src;
  fsm_event_t event;
  fsm_state_t dst;
  fsm_cb_t on_exit;
  fsm_cb_t on_enter;
};

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // FSM_CORE_H