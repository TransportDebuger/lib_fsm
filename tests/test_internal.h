#ifndef TEST_INTERNAL_H
#define TEST_INTERNAL_H

/* Этот файл предоставляет доступ к внутренним полям FSM только для тестирования.
 * Объявления функций, которые реализованы в s21_fsm.c.
 *
 * ВНИМАНИЕ: Этот файл должен включаться только в файлы тестов.
 * Для этого в Makefile определите макрос TEST_INTERNAL при сборке тестов.
 * Тестовые функции будут включены только при определении TEST_INTERNAL.
 */

/* Определение макроса TEST_INTERNAL должно быть сделано в Makefile */
#ifndef TEST_INTERNAL

/* Если кто-то попытается включить этот файл без определения TEST_INTERNAL,
 * это приведет к ошибке компиляции (чтобы предотвратить случайное использование) */
    #error "test_internal.h must be included into testing files only with defined macros TEST_INTERNAL"
#endif

/* Внутренние константы для тестов (дублируют определения из s21_fsm.c) */
#define S21_FSM_DEFAULT_STATE_CAPACITY     (8)
#define S21_FSM_DEFAULT_STATE_ALLOC_STEP   (4)
#define S21_FSM_DEFAULT_TRANSITION_CAPACITY (0)
#define S21_FSM_DEFAULT_TRANSITION_ALLOC_STEP (4)

/* Объявление типов структур */

struct s21_fsm_transition;
struct s21_fsm_state;

/* Объявление getter-функций для доступа к внутренним полям */
size_t s21_fsm_get_state_count(const s21_fsm_t *fsm);
size_t s21_fsm_get_state_capacity(const s21_fsm_t *fsm);
size_t s21_fsm_get_transition_count(const struct s21_fsm_state *state);
size_t s21_fsm_get_transition_capacity(const struct s21_fsm_state *state);
struct s21_fsm_state *s21_fsm_get_state_by_index(s21_fsm_t *fsm, size_t index);

void s21_fsm_test_set_current_state(s21_fsm_t *fsm, s21_state_id_t state_id);
void s21_fsm_test_set_initial_state_raw(s21_fsm_t *fsm, s21_state_id_t state_id);
void s21_fsm_test_set_in_dispatch(s21_fsm_t *fsm, bool value);
void s21_fsm_test_set_in_update(s21_fsm_t *fsm, bool value);

void s21_fsm_test_fail_next_malloc(void);
void s21_fsm_test_fail_second_malloc(void);
void s21_fsm_test_fail_next_realloc(void);
void s21_fsm_test_fail_second_realloc(void);
int s21_fsm_test_find_transition_index_null(void);
void s21_fsm_test_remove_transition_at_invalid(void);
void s21_fsm_test_remove_transitions_to_state_invalid(s21_fsm_t *fsm);
int s21_fsm_test_compact_states_invalid(s21_fsm_t *fsm, s21_state_id_t id);
int s21_fsm_test_grow_states_null(void);
int s21_fsm_test_grow_transitions_null(void);

#endif /* TEST_INTERNAL_H */
