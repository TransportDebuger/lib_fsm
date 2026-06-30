/* Тесты для suite dispatch */

#include "test_fsm.h"

/* TC1: dispatch_successful_transition */
START_TEST(test_dispatch_successful_transition) {
    reset_call_counts();
    
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, on_entry_default, NULL, on_exit_default, NULL);
    s21_fsm_register_state(fsm, 1, on_entry_default, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    s21_fsm_register_transition(fsm, 0, 1, 1, NULL, on_effect_default, NULL);
    
    s21_fsm_initialize(fsm);
    
    int rc = s21_fsm_dispatch(fsm, 1);
    ck_assert_int_eq(rc, S21_FSM_OK);
    ck_assert_int_eq(s21_fsm_get_current_state(fsm), 1);
    
    ck_assert_int_eq(exit_call_count, 1);
    ck_assert_int_eq(effect_call_count, 1);
    ck_assert_int_eq(entry_call_count, 2);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC2: dispatch_null_fsm */
START_TEST(test_dispatch_null_fsm) {
    int rc = s21_fsm_dispatch(NULL, 0);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
}
END_TEST

/* TC3: dispatch_not_initialized */
START_TEST(test_dispatch_not_initialized) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    int rc = s21_fsm_dispatch(fsm, 1);
    ck_assert_int_eq(rc, S21_FSM_NOT_INITIALIZED);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC4: dispatch_state_not_found */
START_TEST(test_dispatch_state_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    /* Удаляем текущее состояние */
    s21_fsm_unregister_state(fsm, 0);
    
    int rc = s21_fsm_dispatch(fsm, 1);
    ck_assert_int_eq(rc, S21_FSM_STATE_NOT_FOUND);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC5: dispatch_no_transition */
START_TEST(test_dispatch_no_transition) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    int rc = s21_fsm_dispatch(fsm, 999);
    ck_assert_int_eq(rc, S21_FSM_NO_TRANSITION);
    ck_assert_int_eq(s21_fsm_get_current_state(fsm), 0);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC6: dispatch_guard_false */
START_TEST(test_dispatch_guard_false) {
    reset_call_counts();
    
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    /* Guard всегда возвращает false */
    s21_fsm_register_transition(fsm, 0, 1, 1, on_guard_false, on_effect_default, NULL);
    
    s21_fsm_initialize(fsm);
    
    int rc = s21_fsm_dispatch(fsm, 1);
    ck_assert_int_eq(rc, S21_FSM_NO_TRANSITION);
    ck_assert_int_eq(s21_fsm_get_current_state(fsm), 0);
    ck_assert_int_eq(guard_call_count, 1);
    ck_assert_int_eq(effect_call_count, 0);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC7: dispatch_reentrant_protection */
START_TEST(test_dispatch_reentrant_protection) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    int rc1 = s21_fsm_dispatch(fsm, 1);
    ck_assert_int_eq(rc1, S21_FSM_NO_TRANSITION);
    
    /* Повторный вызов не должен вызывать проблемы */
    int rc2 = s21_fsm_dispatch(fsm, 1);
    ck_assert_int_eq(rc2, S21_FSM_NO_TRANSITION);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC8: dispatch_long_sequence */
START_TEST(test_dispatch_long_sequence) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    /* Создаем цикл A->B->C->A */
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 2, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    s21_fsm_register_transition(fsm, 0, 1, 1, NULL, NULL, NULL);
    s21_fsm_register_transition(fsm, 1, 2, 1, NULL, NULL, NULL);
    s21_fsm_register_transition(fsm, 2, 0, 1, NULL, NULL, NULL);
    
    s21_fsm_initialize(fsm);
    
    /* 1000 итераций */
    for (int i = 0; i < 1000; i++) {
        int rc = s21_fsm_dispatch(fsm, 1);
        ck_assert_int_eq(rc, S21_FSM_OK);
    }
    
    /* Проверяем, что FSM работает корректно */
    ck_assert_int_eq(s21_fsm_is_initialized(fsm), 1);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* Создание suite */
Suite *suite_fsm_dispatch(void) {
    Suite *s = suite_create("fsm_dispatch");
    
    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_dispatch_successful_transition);
    tcase_add_test(tc, test_dispatch_null_fsm);
    tcase_add_test(tc, test_dispatch_not_initialized);
    tcase_add_test(tc, test_dispatch_state_not_found);
    tcase_add_test(tc, test_dispatch_no_transition);
    tcase_add_test(tc, test_dispatch_guard_false);
    tcase_add_test(tc, test_dispatch_reentrant_protection);
    tcase_add_test(tc, test_dispatch_long_sequence);
    suite_add_tcase(s, tc);
    
    return s;
}
