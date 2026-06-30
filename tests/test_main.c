/* Main файл для запуска всех тестов */

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "test_fsm.h"

/* Объявление suites */
Suite *suite_fsm_creation_destruction(void);
Suite *suite_fsm_states(void);
Suite *suite_fsm_transitions(void);
Suite *suite_fsm_dispatch(void);
Suite *suite_fsm_update(void);
Suite *suite_fsm_initialize_reset(void);
Suite *suite_fsm_edge_cases(void);

/* Реализация callback-функций */
int entry_call_count = 0;
int do_call_count = 0;
int exit_call_count = 0;
int effect_call_count = 0;
int guard_call_count = 0;

void reset_call_counts(void) {
    entry_call_count = 0;
    do_call_count = 0;
    exit_call_count = 0;
    effect_call_count = 0;
    guard_call_count = 0;
}

void on_entry_default(void *ctx) { entry_call_count++; (void)ctx; }
void on_do_default(void *ctx) { do_call_count++; (void)ctx; }
void on_exit_default(void *ctx) { exit_call_count++; (void)ctx; }
void on_effect_default(void *ctx) { effect_call_count++; (void)ctx; }
bool on_guard_true(void *ctx) { guard_call_count++; (void)ctx; return true; }
bool on_guard_false(void *ctx) { guard_call_count++; (void)ctx; return false; }

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[]) {
    int number_failed;
    
    /* Создаем общий список suites */
    Suite * suites[] = {
        suite_fsm_creation_destruction(),
        suite_fsm_states(),
        suite_fsm_transitions(),
        suite_fsm_dispatch(),
        suite_fsm_update(),
        suite_fsm_initialize_reset(),
        suite_fsm_edge_cases()
    };
    
    /* Создаем итератор для запуска всех suites */
    SRunner *sr = srunner_create(suites[0]);
    for (int i = 1; i < 7; i++) {
        srunner_add_suite(sr, suites[i]);
    }
    
    /* Настройки вывода */
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_set_log(sr, "test_results.xml");
    srunner_set_xml(sr, "test_results.xml");
    
    /* Запуск всех тестов */
    srunner_run_all(sr, CK_NORMAL);
    
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
