/* Тесты для suite states */

#include "test_fsm.h"

/* TC1: register_single_state */
START_TEST(test_register_single_state) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    ck_assert_int_eq(rc, S21_FSM_OK);
    
    ck_assert_int_eq(s21_fsm_get_initial_state(fsm), S21_FSM_INVALID_STATE_ID);
    ck_assert_int_eq(s21_fsm_get_current_state(fsm), S21_FSM_INVALID_STATE_ID);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC2: register_multiple states */
START_TEST(test_register_multiple_states) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);

    // Регистрируем 12 состояний (должен сработать grow)
    for (int i = 0; i < 12; i++) {
        int ret = s21_fsm_register_state(fsm, i, NULL, NULL, NULL, NULL);
        ck_assert_int_eq(S21_FSM_OK, ret);
    }

    // Проверяем, что все прошли
    ck_assert_int_eq(12, s21_fsm_get_state_count(fsm));
    ck_assert_int_eq(S21_FSM_DEFAULT_STATE_ALLOC_STEP + S21_FSM_DEFAULT_STATE_CAPACITY, s21_fsm_get_state_capacity(fsm)); // 8 + 4 + 4 = 16

    s21_fsm_destroy(fsm);
}
END_TEST

/* TC2: register_state_negative_id */
START_TEST(test_register_state_negative_id) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_register_state(fsm, -1, NULL, NULL, NULL, NULL);
    ck_assert_int_eq(rc, S21_FSM_STATE_INCORRECT_ID);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC3: register_state_duplicate_id */
START_TEST(test_register_state_duplicate_id) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    int rc = s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    ck_assert_int_eq(rc, S21_FSM_STATE_EXISTS);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC4: register_state_null_fsm */
START_TEST(test_register_state_null_fsm) {
    int rc = s21_fsm_register_state(NULL, 0, NULL, NULL, NULL, NULL);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
}
END_TEST

/* TC5: update_existing_state */
START_TEST(test_update_existing_state) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    
    int rc = s21_fsm_update_state(fsm, 0, on_entry_default, on_do_default, on_exit_default, (void*)0x1234);
    ck_assert_int_eq(rc, S21_FSM_OK);
    
    void *ctx;
    rc = s21_fsm_get_state_context(fsm, 0, &ctx);
    ck_assert_int_eq(rc, S21_FSM_OK);
    ck_assert_ptr_eq(ctx, (void*)0x1234);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC6: update_state_not_found */
START_TEST(test_update_state_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_update_state(fsm, 42, NULL, NULL, NULL, NULL);
    ck_assert_int_eq(rc, S21_FSM_STATE_NOT_FOUND);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC7: unregister_existing_state */
START_TEST(test_unregister_existing_state) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    s21_fsm_set_initial_state(fsm, 0);
    
    int rc = s21_fsm_unregister_state(fsm, 0);
    ck_assert_int_eq(rc, S21_FSM_OK);
    
    ck_assert_int_eq(s21_fsm_get_initial_state(fsm), S21_FSM_INVALID_STATE_ID);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC8: unregister_state_negative_id */
START_TEST(test_unregister_state_negative_id) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_unregister_state(fsm, -1);
    ck_assert_int_eq(rc, S21_FSM_STATE_INCORRECT_ID);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC9: unregister_state_not_found */
START_TEST(test_unregister_state_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_unregister_state(fsm, 100);
    ck_assert_int_eq(rc, S21_FSM_STATE_NOT_FOUND);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC10: set_initial_state_valid */
START_TEST(test_set_initial_state_valid) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 0, NULL, NULL, NULL, NULL);
    
    int rc = s21_fsm_set_initial_state(fsm, 0);
    ck_assert_int_eq(rc, S21_FSM_OK);
    
    ck_assert_int_eq(s21_fsm_get_initial_state(fsm), 0);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC11: set_initial_state_negative */
START_TEST(test_set_initial_state_negative) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_set_initial_state(fsm, -1);
    ck_assert_int_eq(rc, S21_FSM_STATE_INCORRECT_ID);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC12: set_initial_state_not_found */
START_TEST(test_set_initial_state_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    int rc = s21_fsm_set_initial_state(fsm, 999);
    ck_assert_int_eq(rc, S21_FSM_STATE_NOT_FOUND);
    ck_assert_int_eq(s21_fsm_get_initial_state(fsm), S21_FSM_INVALID_STATE_ID);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC13: get_initial_state_null_fsm */
START_TEST(test_get_initial_state_null_fsm) {
    s21_state_id_t id = s21_fsm_get_initial_state(NULL);
    ck_assert_int_eq(id, S21_FSM_INVALID_STATE_ID);
}
END_TEST

/* TC14: get_current_state_null_fsm */
START_TEST(test_get_current_state_null_fsm) {
    s21_state_id_t id = s21_fsm_get_current_state(NULL);
    ck_assert_int_eq(id, S21_FSM_INVALID_STATE_ID);
}
END_TEST

/* TC15: set_get_state_context_valid */
START_TEST(test_set_get_state_context_valid) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    s21_fsm_register_state(fsm, 1, NULL, NULL, NULL, NULL);
    
    int rc = s21_fsm_set_state_context(fsm, 1, (void*)0xABCDEF);
    ck_assert_int_eq(rc, S21_FSM_OK);
    
    void *out_ctx;
    rc = s21_fsm_get_state_context(fsm, 1, &out_ctx);
    ck_assert_int_eq(rc, S21_FSM_OK);
    ck_assert_ptr_eq(out_ctx, (void*)0xABCDEF);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* TC16: set_get_state_context_null_fsm */
START_TEST(test_set_get_state_context_null_fsm) {
    int rc = s21_fsm_set_state_context(NULL, 0, (void*)0x1);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
    
    void *out_ctx;
    rc = s21_fsm_get_state_context(NULL, 0, &out_ctx);
    ck_assert_int_eq(rc, S21_FSM_NULL_POINTER);
}
END_TEST

/* TC17: get_state_context_not_found */
START_TEST(test_get_state_context_not_found) {
    s21_fsm_t *fsm = s21_fsm_create(NULL);
    ck_assert_ptr_nonnull(fsm);
    
    void *out_ctx;
    int rc = s21_fsm_get_state_context(fsm, 999, &out_ctx);
    ck_assert_int_eq(rc, S21_FSM_STATE_NOT_FOUND);
    
    s21_fsm_destroy(fsm);
}
END_TEST

/* Создание suite */
Suite *suite_fsm_states(void) {
    Suite *s = suite_create("fsm_states");
    
    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_register_single_state);
    tcase_add_test(tc, test_register_multiple_states);
    tcase_add_test(tc, test_register_state_negative_id);
    tcase_add_test(tc, test_register_state_duplicate_id);
    tcase_add_test(tc, test_register_state_null_fsm);
    tcase_add_test(tc, test_update_existing_state);
    tcase_add_test(tc, test_update_state_not_found);
    tcase_add_test(tc, test_unregister_existing_state);
    tcase_add_test(tc, test_unregister_state_negative_id);
    tcase_add_test(tc, test_unregister_state_not_found);
    tcase_add_test(tc, test_set_initial_state_valid);
    tcase_add_test(tc, test_set_initial_state_negative);
    tcase_add_test(tc, test_set_initial_state_not_found);
    tcase_add_test(tc, test_get_initial_state_null_fsm);
    tcase_add_test(tc, test_get_current_state_null_fsm);
    tcase_add_test(tc, test_set_get_state_context_valid);
    tcase_add_test(tc, test_set_get_state_context_null_fsm);
    tcase_add_test(tc, test_get_state_context_not_found);
    suite_add_tcase(s, tc);
    
    return s;
}
