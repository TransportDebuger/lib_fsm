#ifndef TEST_FSM_H
#define TEST_FSM_H

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "s21_fsm.h"
#include "test_internal.h"

/* Macros for checking return codes */
#define ASSERT_OK(rc) ck_assert_msg(rc == S21_FSM_OK, "Expected S21_FSM_OK, got %d", rc)
#define ASSERT_NULL(rc) ck_assert_msg(rc == S21_FSM_NULL_POINTER, "Expected S21_FSM_NULL_POINTER, got %d", rc)
#define ASSERT_NOT_FOUND(rc) ck_assert_msg(rc == S21_FSM_STATE_NOT_FOUND, "Expected S21_FSM_STATE_NOT_FOUND, got %d", rc)
#define ASSERT_INCORRECT_ID(rc) ck_assert_msg(rc == S21_FSM_STATE_INCORRECT_ID, "Expected S21_FSM_STATE_INCORRECT_ID, got %d", rc)
#define ASSERT_EXISTS(rc) ck_assert_msg(rc == S21_FSM_STATE_EXISTS, "Expected S21_FSM_STATE_EXISTS, got %d", rc)
#define ASSERT_TRANSITION_NOT_FOUND(rc) ck_assert_msg(rc == S21_FSM_TRANSITION_NOT_FOUND, "Expected S21_FSM_TRANSITION_NOT_FOUND, got %d", rc)
#define ASSERT_NO_TRANSITION(rc) ck_assert_msg(rc == S21_FSM_NO_TRANSITION, "Expected S21_FSM_NO_TRANSITION, got %d", rc)
#define ASSERT_NOT_INITIALIZED(rc) ck_assert_msg(rc == S21_FSM_NOT_INITIALIZED, "Expected S21_FSM_NOT_INITIALIZED, got %d", rc)
#define ASSERT_REENTRANT(rc) ck_assert_msg(rc == S21_FSM_REENTRANT_CALL, "Expected S21_FSM_REENTRANT_CALL, got %d", rc)
#define ASSERT_ERROR(rc) ck_assert_msg(rc == S21_FSM_ERROR, "Expected S21_FSM_ERROR, got %d", rc)

/* Standard callback functions for tests */
extern int entry_call_count;
extern int do_call_count;
extern int exit_call_count;
extern int effect_call_count;
extern int guard_call_count;

void reset_call_counts(void);
void on_entry_default(void *ctx);
void on_do_default(void *ctx);
void on_exit_default(void *ctx);
void on_effect_default(void *ctx);
bool on_guard_true(void *ctx);
bool on_guard_false(void *ctx);

#endif /* TEST_FSM_H */
