/* Тесты для suite transitions */

#include "test_fsm.h"

/* TC1: register_transition_valid */
START_TEST(test_register_transition_valid) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    int rc = s21_fsm_register_transition(fsm, 0, 1, 0, NULL, NULL, NULL);
    ck_assert(rc == S21_FSM_OK);
    
    s21_fsm_initialize(fsm);
    
    rc = s21_fsm_dispatch(fsm, 0);
    ck_assert(rc == S21_FSM_OK);
    ck_assert_msg(s21_fsm_get_current_state(fsm) == 1, "current should be 1");
    
    s21_fsm_destroy(fsm);
}
END_TEST

START_TEST(test_fsm_grow_transitions) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_set_initial_state(fsm, 0));
    ck_assert_int_eq(S21_FSM_OK, s21_fsm_initialize(fsm));

    for (int i = 0; i < S21_FSM_DEFAULT_STATE_ALLOC_STEP * 2; i++) {
        int ret = s21_fsm_register_transition(fsm, 0, 1, i, NULL, NULL, NULL);
        ck_assert_int_eq(S21_FSM_OK, ret);
    }

    // Проверяем capacity
    s21_state_id_t initial_id = s21_fsm_get_initial_state(fsm);
    struct s21_fsm_state *s = s21_fsm_get_state_by_index(fsm, (size_t)initial_id);
    ck_assert_int_eq(S21_FSM_DEFAULT_TRANSITION_ALLOC_STEP * 2, s21_fsm_get_transition_count(s));
    ck_assert_int_eq(S21_FSM_DEFAULT_TRANSITION_ALLOC_STEP * 2, s21_fsm_get_transition_capacity(s));

    s21_fsm_destroy(fsm);
}
END_TEST

/* TC2: register_transition_invalid_ids */
START_TEST(test_register_transition_invalid_ids) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    
    int rc = s21_fsm_register_transition(fsm, -1, 0, 0, NULL, NULL, NULL);
    ck_assert(rc == S21_FSM_STATE_INCORRECT_ID);
    
    rc = s21_fsm_register_transition(fsm, 0, -1, 0, NULL, NULL, NULL);
    ck_assert(rc == S21_FSM_STATE_INCORRECT_ID);
    
    rc = s21_fsm_register_transition(fsm, 0, 1, -1, NULL, NULL, NULL);
    ck_assert(rc == S21_FSM_EVENT_INCORRECT_ID);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC3: register_transition_state_not_found */
START_TEST(test_register_transition_state_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    
    int rc = s21_fsm_register_transition(fsm, 100, 0, 0, NULL, NULL, NULL);
    ck_assert(rc == S21_FSM_STATE_NOT_FOUND);
    
    rc = s21_fsm_register_transition(fsm, 0, 100, 0, NULL, NULL, NULL);
    ck_assert(rc == S21_FSM_STATE_NOT_FOUND);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC4: unregister_transition_exact_valid */
START_TEST(test_unregister_transition_exact_valid) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    s21_fsm_register_transition(fsm, 0, 1, 1, NULL, NULL, NULL);
    
    int rc = s21_fsm_unregister_transition_exact(fsm, 0, 1, 1, NULL, NULL);
    ck_assert(rc == S21_FSM_OK);
    
    s21_fsm_initialize(fsm);
    
    rc = s21_fsm_dispatch(fsm, 1);
    ck_assert(rc == S21_FSM_NO_TRANSITION);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC5: unregister_transition_exact_not_found */
START_TEST(test_unregister_transition_exact_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    
    int rc = s21_fsm_unregister_transition_exact(fsm, 0, 999, 1, NULL, NULL);
    ck_assert(rc == S21_FSM_TRANSITION_NOT_FOUND);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC6: clear_transitions_by_trigger */
START_TEST(test_clear_transitions_by_trigger) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 2, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    s21_fsm_register_transition(fsm, 0, 1, 1, NULL, NULL, NULL);
    s21_fsm_register_transition(fsm, 0, 2, 1, NULL, NULL, NULL);
    s21_fsm_register_transition(fsm, 0, 1, 2, NULL, NULL, NULL);
    
    int rc = s21_fsm_clear_transitions_by_trigger(fsm, 0, 1);
    ck_assert(rc == S21_FSM_OK);
    
    s21_fsm_initialize(fsm);
    
    rc = s21_fsm_dispatch(fsm, 1);
    ck_assert(rc == S21_FSM_NO_TRANSITION);
    
    rc = s21_fsm_dispatch(fsm, 2);
    ck_assert(rc == S21_FSM_OK);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC7: clear_transitions_from_state */
START_TEST(test_clear_transitions_from_state) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 2, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    s21_fsm_register_transition(fsm, 0, 1, 1, NULL, NULL, NULL);
    s21_fsm_register_transition(fsm, 0, 2, 2, NULL, NULL, NULL);
    
    int rc = s21_fsm_clear_transitions_from_state(fsm, 0);
    ck_assert(rc == S21_FSM_OK);
    
    s21_fsm_initialize(fsm);
    
    rc = s21_fsm_dispatch(fsm, 1);
    ck_assert(rc == S21_FSM_NO_TRANSITION);
    
    rc = s21_fsm_dispatch(fsm, 2);
    ck_assert(rc == S21_FSM_NO_TRANSITION);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC8: set_get_transition_context_exact */
START_TEST(test_set_get_transition_context_exact) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    
    s21_fsm_register_transition(fsm, 0, 1, 0, NULL, NULL, NULL);
    
    int rc = s21_fsm_set_transition_context_exact(fsm, 0, 0, 1, NULL, NULL, (void*)0xCAFEBABE);
    ck_assert(rc == S21_FSM_OK);
    
    void *out_ctx;
    rc = s21_fsm_get_transition_context_exact(fsm, 0, 0, 1, NULL, NULL, &out_ctx);
    ck_assert(rc == S21_FSM_OK);
    ck_assert_msg(out_ctx == (void*)0xCAFEBABE, "context should match");
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC9: set_get_transition_context_null_fsm */
START_TEST(test_set_get_transition_context_null_fsm) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    
    s21_fsm_register_transition(fsm, 0, 1, 0, NULL, NULL, NULL);
    
    int rc = s21_fsm_set_transition_context_exact(NULL, 0, 0, 1, NULL, NULL, (void*)0x1);
    ck_assert(rc == S21_FSM_NULL_POINTER);
    
    void *out_ctx;
    rc = s21_fsm_get_transition_context_exact(NULL, 0, 0, 1, NULL, NULL, &out_ctx);
    ck_assert(rc == S21_FSM_NULL_POINTER);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* Создание suite */
Suite *suite_fsm_transitions(void) {
    Suite *s = suite_create("fsm_transitions");
    
    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_register_transition_valid);
    tcase_add_test(tc, test_fsm_grow_transitions);
    tcase_add_test(tc, test_register_transition_invalid_ids);
    tcase_add_test(tc, test_register_transition_state_not_found);
    tcase_add_test(tc, test_unregister_transition_exact_valid);
    tcase_add_test(tc, test_unregister_transition_exact_not_found);
    tcase_add_test(tc, test_clear_transitions_by_trigger);
    tcase_add_test(tc, test_clear_transitions_from_state);
    tcase_add_test(tc, test_set_get_transition_context_exact);
    tcase_add_test(tc, test_set_get_transition_context_null_fsm);
    suite_add_tcase(s, tc);
    
    return s;
}
