/* Тесты для suite update */

#include "test_fsm.h"

/* TC1: update_calls_on_do */
START_TEST(test_update_calls_on_do) {
    reset_call_counts();
    
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, on_do_default, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    /* Вызываем update 10 раз */
    for (int i = 0; i < 10; i++) {
        int rc = s21_fsm_update(fsm);
        ck_assert_int_eq(rc, S21_FSM_OK);
    }
    
    ck_assert_int_eq(do_call_count, 10);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC2: update_null_fsm */
START_TEST(test_update_null_fsm) {
    int rc = s21_fsm_update(NULL);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
}
END_TEST

/* TC3: update_not_initialized */
START_TEST(test_update_not_initialized) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, on_do_default, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    int rc = s21_fsm_update(fsm);
    ck_assert_int_eq(rc, S21_FSM_NOT_INITIALIZED);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC4: update_state_not_found */
START_TEST(test_update_state_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    /* Удаляем текущее состояние */
    s21_fsm_unregister_state(fsm, 0);
    
    int rc = s21_fsm_update(fsm);
    ck_assert_int_eq(rc, S21_FSM_STATE_NOT_FOUND);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC5: update_reentrant_protection */
START_TEST(test_update_reentrant_protection) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    int rc1 = s21_fsm_update(fsm);
    ck_assert_int_eq(rc1, S21_FSM_OK);
    
    /* Повторный вызов не должен вызывать проблемы */
    int rc2 = s21_fsm_update(fsm);
    ck_assert_int_eq(rc2, S21_FSM_OK);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC6: update_long_sequence */
START_TEST(test_update_long_sequence) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, on_do_default, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    /* 1000 итераций */
    for (int i = 0; i < 1000; i++) {
        int rc = s21_fsm_update(fsm);
        ck_assert_int_eq(rc, S21_FSM_OK);
    }
    
    ck_assert_int_eq(s21_fsm_is_initialized(fsm), 1);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* Создание suite */
Suite *suite_fsm_update(void) {
    Suite *s = suite_create("fsm_update");
    
    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_update_calls_on_do);
    tcase_add_test(tc, test_update_null_fsm);
    tcase_add_test(tc, test_update_not_initialized);
    tcase_add_test(tc, test_update_state_not_found);
    tcase_add_test(tc, test_update_reentrant_protection);
    tcase_add_test(tc, test_update_long_sequence);
    suite_add_tcase(s, tc);
    
    return s;
}
