/* Тесты для suite initialize/reset */

#include "test_fsm.h"

/* TC1: initialize_valid */
START_TEST(test_initialize_valid) {
    reset_call_counts();
    
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, on_entry_default, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    int rc = s21_fsm_initialize(fsm);
    ck_assert_int_eq(rc, S21_FSM_OK);
    ck_assert_int_eq(s21_fsm_is_initialized(fsm), 1);
    ck_assert_int_eq(s21_fsm_get_current_state(fsm), 0);
    
    /* on_entry должен быть вызван один раз */
    ck_assert_int_eq(entry_call_count, 1);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC2: initialize_null_fsm */
START_TEST(test_initialize_null_fsm) {
    int rc = s21_fsm_initialize(NULL);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
}
END_TEST

/* TC3: initialize_no_states */
START_TEST(test_initialize_no_states) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_initialize(fsm);
    ck_assert_int_eq(rc, S21_FSM_ERROR);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC4: initialize_invalid_initial_state */
START_TEST(test_initialize_invalid_initial_state) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    /* initial_state не задан, останется S21_FSM_INVALID_STATE_ID */
    
    int rc = s21_fsm_initialize(fsm);
    ck_assert_int_eq(rc, S21_FSM_ERROR);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC5: initialize_initial_state_not_found */
START_TEST(test_initialize_initial_state_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    /* Удаляем состояние после set_initial_state */
    s21_fsm_unregister_state(fsm, 0);
    
    int rc = s21_fsm_initialize(fsm);
    ck_assert_int_eq(rc, S21_FSM_ERROR);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC6: reset_valid */
START_TEST(test_reset_valid) {
    reset_call_counts();
    
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, on_entry_default, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, on_entry_default, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    s21_fsm_initialize(fsm);
    
    /* Делаем переход */
    s21_fsm_register_transition(fsm, 0, 1, 1, NULL, NULL, NULL);
    s21_fsm_dispatch(fsm, 1);
    ck_assert_int_eq(s21_fsm_get_current_state(fsm), 1);
    
    /* Сбрасываем */
    int rc = s21_fsm_reset(fsm);
    ck_assert_int_eq(rc, S21_FSM_OK);
    ck_assert_int_eq(s21_fsm_get_current_state(fsm), 0);
    ck_assert_int_eq(s21_fsm_is_initialized(fsm), 1);
    
    /* on_entry должен быть вызван при reset (init + dispatch + reset) */
    ck_assert_int_eq(entry_call_count, 3);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC7: reset_null_fsm */
START_TEST(test_reset_null_fsm) {
    int rc = s21_fsm_reset(NULL);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
}
END_TEST

/* TC8: reset_invalid_initial_state */
START_TEST(test_reset_invalid_initial_state) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    /* initial_state не задан */
    
    int rc = s21_fsm_reset(fsm);
    ck_assert_int_eq(rc, S21_FSM_ERROR);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* Создание suite */
Suite *suite_fsm_initialize_reset(void) {
    Suite *s = suite_create("fsm_initialize_reset");
    
    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_initialize_valid);
    tcase_add_test(tc, test_initialize_null_fsm);
    tcase_add_test(tc, test_initialize_no_states);
    tcase_add_test(tc, test_initialize_invalid_initial_state);
    tcase_add_test(tc, test_initialize_initial_state_not_found);
    tcase_add_test(tc, test_reset_valid);
    tcase_add_test(tc, test_reset_null_fsm);
    tcase_add_test(tc, test_reset_invalid_initial_state);
    suite_add_tcase(s, tc);
    
    return s;
}
