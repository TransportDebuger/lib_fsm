/* Тесты для suite creation/destruction */

#include "test_fsm.h"

/* TC1: create_non_null */
START_TEST(test_create_non_null) {
    s21_fsm_t *fsm = s21_fsm_create((void*)0x1234);
    ck_assert_ptr_nonnull(fsm);
    ck_assert_ptr_eq(s21_fsm_get_context(fsm), (void*)0x1234);
    ck_assert_int_eq(s21_fsm_is_initialized(fsm), false);
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC2: create_with_null_ctx */
START_TEST(test_create_with_null_ctx) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    ck_assert_ptr_eq(s21_fsm_get_context(fsm), NULL);
    ck_assert_int_eq(s21_fsm_is_initialized(fsm), false);
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC3: destroy_valid_fsm */
START_TEST(test_destroy_valid_fsm) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    /* Регистрируем несколько состояний и переходов */
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_register_transition(fsm, 0, 1, 0, NULL, NULL, NULL);
    
    /* Деструктор не должен падать */
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC4: destroy_null */
START_TEST(test_destroy_null) {
    /* Не должно падать */
    s21_fsm_destroy(NULL);
}
END_TEST

/* TC5: set_get_context_valid */
START_TEST(test_set_get_context_valid) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_set_context(fsm, (void*)0xDEADBEEF);
    ck_assert_int_eq(rc, S21_FSM_OK);
    
    void *ctx = s21_fsm_get_context(fsm);
    ck_assert_ptr_eq(ctx, (void*)0xDEADBEEF);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC6: set_context_null_fsm */
START_TEST(test_set_context_null_fsm) {
    int rc = s21_fsm_set_context(NULL, (void*)0x1);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
}
END_TEST

/* TC7: get_context_null_fsm */
START_TEST(test_get_context_null_fsm) {
    void *ctx = s21_fsm_get_context(NULL);
    ck_assert_ptr_eq(ctx, NULL);
}
END_TEST

/* Создание suite */
Suite *suite_fsm_creation_destruction(void) {
    Suite *s = suite_create("fsm_creation_destruction");
    
    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_create_non_null);
    tcase_add_test(tc_core, test_create_with_null_ctx);
    tcase_add_test(tc_core, test_destroy_valid_fsm);
    tcase_add_test(tc_core, test_destroy_null);
    tcase_add_test(tc_core, test_set_get_context_valid);
    tcase_add_test(tc_core, test_set_context_null_fsm);
    tcase_add_test(tc_core, test_get_context_null_fsm);
    suite_add_tcase(s, tc_core);
    
    return s;
}
