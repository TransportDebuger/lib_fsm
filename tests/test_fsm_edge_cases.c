#include "test_fsm.h"

static s21_fsm_t *make_two_state_fsm(void) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL));
    return fsm;
}

START_TEST(test_state_api_additional_invalid_arguments) {
    void *out = (void *)0x1;
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);

    ck_assert_int_eq(S21_FSM_NULL_POINTER,
                     s21_fsm_update_state(NULL, 0, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_update_state(fsm, -1, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_NULL_POINTER, s21_fsm_unregister_state(NULL, 0));
    ck_assert_int_eq(S21_FSM_NULL_POINTER, s21_fsm_set_initial_state(NULL, 0));
    ck_assert_int_eq(S21_FSM_NULL_POINTER, s21_fsm_set_state_context(NULL, 0, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID, s21_fsm_set_state_context(fsm, -1, NULL));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND, s21_fsm_set_state_context(fsm, 999, NULL));
    ck_assert_int_eq(S21_FSM_NULL_POINTER, s21_fsm_get_state_context(fsm, 0, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID, s21_fsm_get_state_context(fsm, -1, &out));
    ck_assert_ptr_null(s21_fsm_get_state_by_index(NULL, 0));
    ck_assert_int_eq(0, s21_fsm_get_state_count(NULL));
    ck_assert_int_eq(0, s21_fsm_get_state_capacity(NULL));

    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_transition_api_additional_invalid_arguments) {
    s21_fsm_t *fsm = make_two_state_fsm();
    void *out = NULL;

    ck_assert_int_eq(S21_FSM_NULL_POINTER,
                     s21_fsm_register_transition(NULL, 0, 1, 0, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_NULL_POINTER,
                     s21_fsm_unregister_transition_exact(NULL, 0, 0, 1, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_unregister_transition_exact(fsm, -1, 0, 1, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_unregister_transition_exact(fsm, 0, 0, -1, NULL, NULL));
    ck_assert_int_eq(S21_FSM_EVENT_INCORRECT_ID,
                     s21_fsm_unregister_transition_exact(fsm, 0, -1, 1, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND,
                     s21_fsm_unregister_transition_exact(fsm, 99, 0, 1, NULL, NULL));

    ck_assert_int_eq(S21_FSM_NULL_POINTER,
                     s21_fsm_clear_transitions_by_trigger(NULL, 0, 0));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_clear_transitions_by_trigger(fsm, -1, 0));
    ck_assert_int_eq(S21_FSM_EVENT_INCORRECT_ID,
                     s21_fsm_clear_transitions_by_trigger(fsm, 0, -1));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND,
                     s21_fsm_clear_transitions_by_trigger(fsm, 99, 0));

    ck_assert_int_eq(S21_FSM_NULL_POINTER,
                     s21_fsm_clear_transitions_from_state(NULL, 0));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_clear_transitions_from_state(fsm, -1));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND,
                     s21_fsm_clear_transitions_from_state(fsm, 99));

    ck_assert_int_eq(S21_FSM_NULL_POINTER,
                     s21_fsm_set_transition_context_exact(NULL, 0, 0, 1, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_set_transition_context_exact(fsm, -1, 0, 1, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_set_transition_context_exact(fsm, 0, 0, -1, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_EVENT_INCORRECT_ID,
                     s21_fsm_set_transition_context_exact(fsm, 0, -1, 1, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND,
                     s21_fsm_set_transition_context_exact(fsm, 99, 0, 1, NULL, NULL, NULL));

    ck_assert_int_eq(S21_FSM_NULL_POINTER,
                     s21_fsm_get_transition_context_exact(fsm, 0, 0, 1, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_get_transition_context_exact(fsm, -1, 0, 1, NULL, NULL, &out));
    ck_assert_int_eq(S21_FSM_STATE_INCORRECT_ID,
                     s21_fsm_get_transition_context_exact(fsm, 0, 0, -1, NULL, NULL, &out));
    ck_assert_int_eq(S21_FSM_EVENT_INCORRECT_ID,
                     s21_fsm_get_transition_context_exact(fsm, 0, -1, 1, NULL, NULL, &out));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND,
                     s21_fsm_get_transition_context_exact(fsm, 99, 0, 1, NULL, NULL, &out));

    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_transition_context_exact_requires_exact_callbacks) {
    s21_fsm_t *fsm = make_two_state_fsm();
    void *out = NULL;
    ck_assert_int_eq(S21_FSM_OK,
                     s21_fsm_register_transition(fsm, 0, 1, 7, on_guard_true,
                                                 on_effect_default, NULL));

    ck_assert_int_eq(S21_FSM_TRANSITION_NOT_FOUND,
                     s21_fsm_set_transition_context_exact(fsm, 0, 7, 1, NULL,
                                                          on_effect_default, NULL));
    ck_assert_int_eq(S21_FSM_TRANSITION_NOT_FOUND,
                     s21_fsm_get_transition_context_exact(fsm, 0, 7, 1,
                                                          on_guard_true, NULL, &out));
    ck_assert_int_eq(S21_FSM_OK,
                     s21_fsm_set_transition_context_exact(fsm, 0, 7, 1,
                                                          on_guard_true,
                                                          on_effect_default,
                                                          (void *)0x55));
    ck_assert_int_eq(S21_FSM_OK,
                     s21_fsm_get_transition_context_exact(fsm, 0, 7, 1,
                                                          on_guard_true,
                                                          on_effect_default, &out));
    ck_assert_ptr_eq(out, (void *)0x55);
    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_dispatch_additional_error_branches) {
    s21_fsm_t *fsm = make_two_state_fsm();
    ck_assert_int_eq(S21_FSM_EVENT_INCORRECT_ID, s21_fsm_dispatch(fsm, -1));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_set_initial_state(fsm, 0));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_initialize(fsm));

    s21_fsm_test_set_in_dispatch(fsm, true);
    ck_assert_int_eq(S21_FSM_REENTRANT_CALL, s21_fsm_dispatch(fsm, 0));
    s21_fsm_test_set_in_dispatch(fsm, false);

    s21_fsm_test_set_in_update(fsm, true);
    ck_assert_int_eq(S21_FSM_REENTRANT_CALL, s21_fsm_dispatch(fsm, 0));
    s21_fsm_test_set_in_update(fsm, false);

    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_update_additional_reentrant_branches) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_set_initial_state(fsm, 0));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_initialize(fsm));

    s21_fsm_test_set_in_update(fsm, true);
    ck_assert_int_eq(S21_FSM_REENTRANT_CALL, s21_fsm_update(fsm));
    s21_fsm_test_set_in_update(fsm, false);

    s21_fsm_test_set_in_dispatch(fsm, true);
    ck_assert_int_eq(S21_FSM_REENTRANT_CALL, s21_fsm_update(fsm));
    s21_fsm_test_set_in_dispatch(fsm, false);

    ck_assert_int_eq(S21_FSM_OK, s21_fsm_update(fsm));
    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_initialize_and_reset_state_not_found_branches) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL));

    s21_fsm_test_set_initial_state_raw(fsm, 99);
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND, s21_fsm_initialize(fsm));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND, s21_fsm_reset(fsm));

    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_unregister_state_removes_incoming_transitions_and_current) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 2, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_transition(fsm, 0, 2, 9, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_transition(fsm, 0, 1, 1, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_transition(fsm, 1, 2, 3, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_transition(fsm, 2, 1, 1, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_set_initial_state(fsm, 0));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_initialize(fsm));
    s21_fsm_test_set_current_state(fsm, 1);

    ck_assert_int_eq(S21_FSM_OK, s21_fsm_unregister_state(fsm, 1));
    ck_assert_int_eq(S21_FSM_INVALID_STATE_ID, s21_fsm_get_current_state(fsm));

    struct s21_fsm_state *state0 = s21_fsm_get_state_by_index(fsm, 0);
    struct s21_fsm_state *state1 = s21_fsm_get_state_by_index(fsm, 1);
    ck_assert_int_eq(1, s21_fsm_get_transition_count(state0));
    ck_assert_int_eq(0, s21_fsm_get_transition_count(state1));
    ck_assert_ptr_null(s21_fsm_get_state_by_index(fsm, 99));
    ck_assert_int_eq(0, s21_fsm_get_transition_count(NULL));
    ck_assert_int_eq(0, s21_fsm_get_transition_capacity(NULL));

    s21_fsm_destroy(fsm);
}
END_TEST


START_TEST(test_internal_static_helper_error_branches) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);

    ck_assert_int_eq(-1, s21_fsm_test_find_transition_index_null());
    s21_fsm_test_remove_transition_at_invalid();
    s21_fsm_test_remove_transitions_to_state_invalid(fsm);
    ck_assert_int_eq(S21_FSM_ERROR, s21_fsm_test_compact_states_invalid(NULL, 0));
    ck_assert_int_eq(S21_FSM_ERROR, s21_fsm_test_compact_states_invalid(fsm, -1));
    ck_assert_int_eq(S21_FSM_STATE_NOT_FOUND,
                     s21_fsm_test_compact_states_invalid(fsm, 42));
    ck_assert_int_eq(S21_FSM_NULL_POINTER, s21_fsm_test_grow_states_null());
    ck_assert_int_eq(S21_FSM_NULL_POINTER, s21_fsm_test_grow_transitions_null());
    ck_assert_int_eq(false, s21_fsm_is_initialized(NULL));

    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_create_allocation_failures) {
    s21_fsm_test_fail_next_malloc();
    ck_assert_ptr_null(s21_fsm_create(NULL));

    s21_fsm_test_fail_second_malloc();
    ck_assert_ptr_null(s21_fsm_create(NULL));
}
END_TEST

START_TEST(test_grow_state_allocation_failure_is_returned) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);

    for (int i = 0; i < S21_FSM_DEFAULT_STATE_CAPACITY; ++i) {
        ck_assert_int_eq(S21_FSM_OK,
                         s21_fsm_register_state(fsm, i, NULL, NULL, NULL, NULL));
    }
    s21_fsm_test_fail_next_realloc();
    ck_assert_int_eq(S21_FSM_ALLOCATION_ERROR,
                     s21_fsm_register_state(fsm, 99, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_DEFAULT_STATE_CAPACITY, s21_fsm_get_state_count(fsm));

    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_grow_transition_allocation_failure_is_returned) {
    s21_fsm_t *fsm = make_two_state_fsm();
    s21_fsm_test_fail_next_realloc();
    ck_assert_int_eq(S21_FSM_ALLOCATION_ERROR,
                     s21_fsm_register_transition(fsm, 0, 1, 1, NULL, NULL, NULL));

    s21_fsm_destroy(fsm);
}
END_TEST


START_TEST(test_second_realloc_failure_countdown_branch) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);

    for (int i = 0; i < S21_FSM_DEFAULT_STATE_CAPACITY; ++i) {
        ck_assert_int_eq(S21_FSM_OK,
                         s21_fsm_register_state(fsm, i, NULL, NULL, NULL, NULL));
    }
    s21_fsm_test_fail_second_realloc();
    ck_assert_int_eq(S21_FSM_OK,
                     s21_fsm_register_state(fsm, 100, NULL, NULL, NULL, NULL));

    for (int i = 0; i < S21_FSM_DEFAULT_STATE_ALLOC_STEP - 1; ++i) {
        ck_assert_int_eq(S21_FSM_OK,
                         s21_fsm_register_state(fsm, 101 + i, NULL, NULL, NULL, NULL));
    }
    ck_assert_int_eq(S21_FSM_ALLOCATION_ERROR,
                     s21_fsm_register_state(fsm, 200, NULL, NULL, NULL, NULL));
    s21_fsm_destroy(fsm);
}
END_TEST

Suite *suite_fsm_edge_cases(void) {
    Suite *s = suite_create("fsm_edge_cases");
    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_state_api_additional_invalid_arguments);
    tcase_add_test(tc, test_transition_api_additional_invalid_arguments);
    tcase_add_test(tc, test_transition_context_exact_requires_exact_callbacks);
    tcase_add_test(tc, test_dispatch_additional_error_branches);
    tcase_add_test(tc, test_update_additional_reentrant_branches);
    tcase_add_test(tc, test_initialize_and_reset_state_not_found_branches);
    tcase_add_test(tc, test_unregister_state_removes_incoming_transitions_and_current);
    tcase_add_test(tc, test_internal_static_helper_error_branches);
    tcase_add_test(tc, test_create_allocation_failures);
    tcase_add_test(tc, test_grow_state_allocation_failure_is_returned);
    tcase_add_test(tc, test_grow_transition_allocation_failure_is_returned);
    tcase_add_test(tc, test_second_realloc_failure_countdown_branch);
    suite_add_tcase(s, tc);
    return s;
}
