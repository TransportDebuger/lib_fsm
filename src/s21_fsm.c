/**
 * @file s21_fsm.c
 * @brief Внутренняя реализация библиотеки конечного автомата (FSM).
 * @internal
 * @author provemet (s21)
 * @date 27-06-2026
 */

#include "s21_fsm.h"
#include <stdlib.h>
#include <string.h>

#ifdef TEST_INTERNAL
static int g_test_malloc_fail_countdown = -1;
static int g_test_realloc_fail_countdown = -1;

static void *s21_fsm_test_malloc(size_t size) {
    if (g_test_malloc_fail_countdown == 0) {
        g_test_malloc_fail_countdown = -1;
        return NULL;
    }
    if (g_test_malloc_fail_countdown > 0) {
        g_test_malloc_fail_countdown--;
    }
    return malloc(size);
}

static void *s21_fsm_test_realloc(void *ptr, size_t size) {
    if (g_test_realloc_fail_countdown == 0) {
        g_test_realloc_fail_countdown = -1;
        return NULL;
    }
    if (g_test_realloc_fail_countdown > 0) {
        g_test_realloc_fail_countdown--;
    }
    return realloc(ptr, size);
}

#define malloc s21_fsm_test_malloc
#define realloc s21_fsm_test_realloc
#endif /* TEST_INTERNAL */

#ifdef TEST_INTERNAL
    #include "test_fsm.h"
#endif /* TEST_INTERNAL */

/**
 * @internal
 * @brief Начальная ёмкость массива состояний.
 *
 * Используется при создании FSM для первой аллокации массива состояний.
 * Не является частью публичного API и может изменяться без влияния на ABI.
 */
#define S21_FSM_DEFAULT_STATE_CAPACITY     (8)

/**
 * @internal
 * @brief Шаг увеличения ёмкости массива состояний.
 *
 * Используется при росте массива состояний в s21_fsm_grow_states.
 * Определяет, на сколько элементов увеличивается capacity при realloc.
 */
#define S21_FSM_DEFAULT_STATE_ALLOC_STEP   (4)

/**
 * @internal
 * @brief Начальная ёмкость массива переходов для каждого состояния.
 *
 * Используется при первой аллокации массива переходов у состояния.
 * Значение 0 означает ленивую аллокацию (выделение памяти при добавлении
 * первого перехода).
 */
#define S21_FSM_DEFAULT_TRANSITION_CAPACITY (0)

/**
 * @internal
 * @brief Шаг увеличения ёмкости массива переходов.
 *
 * Используется при росте массива переходов в s21_fsm_grow_transitions.
 * Определяет, на сколько элементов увеличивается capacity при realloc.
 */
#define S21_FSM_DEFAULT_TRANSITION_ALLOC_STEP (4)

/**
 * @internal
 * @struct s21_fsm_transition
 * @brief Внутренняя структура, описывающая переход между состояниями FSM.
 *
 * Каждый переход принадлежит конкретному состоянию-источнику и задаётся
 * триггером события, целевым состоянием и опциональными callback-функциями
 * guard/effect, а также собственным контекстом перехода.
 *
 * Структура не экспортируется в публичный API и может изменяться без
 * влияния на внешний контракт библиотеки.
 */
struct s21_fsm_transition {
    s21_event_id_t trigger; ///< Идентификатор события, вызывающего переход (>= 0).
    s21_state_id_t target_state_id;      ///< Идентификатор целевого состояния.
    s21_fsm_guard_callback_t on_guard;   ///< Callback guard-условия.
    s21_fsm_effect_callback_t on_effect; ///< Callback effect-действия.
    void *ctx;                           ///< Пользовательский контекст перехода.
};

/**
 * @internal
 * @struct s21_fsm_state
 * @brief Внутреннее представление автомата.
 *
 * Хранит идентификатор состояния, массив исходящих переходов,
 * callbacks entry/do/exit и контекст состояния.
 */
struct s21_fsm_state {
    s21_state_id_t id;  ///< Идентификатор состояния (>= 0).
    struct s21_fsm_transition *transitions; ///< Массив исходящих переходов из этого состояния.
    size_t transition_count;   ///< Количество зарегистрированных переходов.
    size_t transition_capacity; ///< Текущая ёмкость массива переходов.
    size_t transition_alloc_step; ///< Шаг увеличения ёмкости массива переходов.
    s21_fsm_state_callback_t on_entry; ///< Callback при входе в состояние; может быть NULL.
    s21_fsm_state_callback_t on_do; ///< Callback при обновлении состояния; может быть NULL.
    s21_fsm_state_callback_t on_exit; ///< Callback при выходе из состояния; может быть NULL.
    void *ctx; ///< Пользовательский контекст состояния.
};

/**
 * @internal
 * @struct s21_fsm
 * @brief Внутренняя структура, описывающая экземпляр конечного автомата FSM.
 *
 * Хранит массив состояний, глобальный контекст, идентификаторы начального и текущего
 * состояния, а также флаги и служебные поля для управления жизненным циклом и
 * обработки событий.
 */
struct s21_fsm {
    struct s21_fsm_state *states; ///< Массив зарегистрированных состояний.
    size_t state_count; ///< Количество зарегистрированных состояний.
    size_t state_capacity; ///< Текущая ёмкость массива состояний.
    size_t state_alloc_step; ///< Шаг увеличения ёмкости массива состояний.
    void *user_ctx; ///< Глобальный пользовательский контекст FSM.
    s21_state_id_t initial_state; ///< Идентификатор начального состояния.
    s21_state_id_t current_state; ///< Идентификатор текущего состояния.
    bool initialized; ///< Флаг инициализации FSM.
    bool in_dispatch; ///< Флаг обработки события.
    bool in_update;   ///< Флаг обновления состояния.
};

/* --- Helper functions --- */

/**
 * @brief Находит индекс состояния по его идентификатору.
 *
 * @param[in] fsm      Указатель на FSM, внутри которой выполняется поиск.
 * @param[in] state_id Идентификатор состояния (должен быть ≥ 0).
 *
 * @return Индекс состояния
 * @retval ≥ 0 индекс найденного состояния в массиве @c states.
 * @retval -1 если @p fsm равен NULL, @p state_id < 0 или состояние не найдено.
 */
static int s21_fsm_find_state_index(s21_fsm_t *fsm, s21_state_id_t state_id) {
    if (!fsm || state_id < 0) {
        return -1;
    }
    for (size_t i = 0; i < fsm->state_count; i++) {
        if (fsm->states[i].id == state_id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Находит состояние по его идентификатору.
 *
 * @param[in] fsm      Указатель на FSM, внутри которой выполняется поиск.
 * @param[in] state_id Идентификатор состояния (должен быть ≥ 0).
 *
 * @return Указатель на состояние
 * @retval ненулевой указатель если состояние найдено.
 * @retval NULL если @p fsm равен NULL, @p state_id < 0 или состояние не найдено.
 */
static struct s21_fsm_state *s21_fsm_find_state(s21_fsm_t *fsm, s21_state_id_t state_id) {
    int idx = s21_fsm_find_state_index(fsm, state_id);
    if (idx >= 0) {
        return &fsm->states[idx];
    }
    return NULL;
}

/**
 * @brief Находит индекс перехода в массиве переходов состояния.
 *
 * @param[in] state        Указатель на состояние, в котором выполняется поиск.
 * @param[in] trigger      Идентификатор события‑триггера перехода.
 * @param[in] dest_state_id Идентификатор целевого состояния перехода.
 * @param[in] on_guard     Указатель на guard‑callback перехода (может быть NULL).
 * @param[in] on_effect    Указатель на effect‑callback перехода (может быть NULL).
 *
 * @return Индекс перехода
 * @retval ≥ 0 индекс найденного перехода в массиве @c transitions.
 * @retval -1 если @p state равен NULL или переход с указанными параметрами не найден.
 */
static int s21_fsm_find_transition_index(struct s21_fsm_state *state,
                                         s21_event_id_t trigger,
                                         s21_state_id_t dest_state_id,
                                         s21_fsm_guard_callback_t on_guard,
                                         s21_fsm_effect_callback_t on_effect) {
    if (!state) {
        return -1;
    }
    for (size_t i = 0; i < state->transition_count; i++) {
        if (state->transitions[i].trigger == trigger &&
            state->transitions[i].target_state_id == dest_state_id &&
            state->transitions[i].on_guard == on_guard &&
            state->transitions[i].on_effect == on_effect) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Удаляет переход из массива переходов состояния по индексу.
 *
 * @param[in,out] state Указатель на состояние, из которого удаляется переход.
 * @param[in]     idx   Индекс удаляемого перехода (должен быть в диапазоне [0, transition_count)).
 *
 * @note При некорректных аргументах (NULL, индекс вне диапазона) функция ничего не делает.
 */
static void s21_fsm_remove_transition_at(struct s21_fsm_state *state, int idx) {
    if (!state || idx < 0 || (size_t)idx >= state->transition_count) {
        return;
    }

    memmove(&state->transitions[idx], &state->transitions[idx + 1],
            (state->transition_count - idx - 1) * sizeof(struct s21_fsm_transition));
    state->transition_count--;
}

/**
 * @brief Удаляет все переходы из всех состояний FSM, ведущие в указанное состояние.
 *
 * @param[in,out] fsm           Указатель на FSM, в которой выполняется удаление переходов.
 * @param[in]     dest_state_id Идентификатор целевого состояния, в которое ведут удаляемые переходы (должен быть ≥ 0).
 *
 * @note При некорректных аргументах (NULL, отрицательный идентификатор) функция ничего не делает.
 */
static void s21_fsm_remove_transitions_to_state(s21_fsm_t *fsm, s21_state_id_t dest_state_id) {
    if (!fsm || dest_state_id < 0) {
        return;
    }
    for (size_t i = 0; i < fsm->state_count; i++) {
        struct s21_fsm_state *state = &fsm->states[i];
        for (size_t j = 0; j < state->transition_count; ) {
            if (state->transitions[j].target_state_id == dest_state_id) {
                s21_fsm_remove_transition_at(state, (int)j);
            } else {
                j++;
            }
        }
    }
}

/**
 * @brief Уплотняет массив состояний FSM после удаления состояния.
 *
 * @param[in,out] fsm        Указатель на FSM, для которой выполняется уплотнение.
 * @param[in]     removed_id Идентификатор удалённого состояния.
 *
 * @return Код результата
 * @retval S21_FSM_OK              при успешном уплотнении массива состояний.
 * @retval S21_FSM_ERROR           если @p fsm равен NULL или @p removed_id < 0.
 * @retval S21_FSM_STATE_NOT_FOUND если состояние с идентификатором @p removed_id не найдено.
 */
static int s21_fsm_compact_states(s21_fsm_t *fsm, s21_state_id_t removed_id) {
    if (!fsm || removed_id < 0) {
        return S21_FSM_ERROR;
    }
    
    int removed_idx = s21_fsm_find_state_index(fsm, removed_id);
    if (removed_idx < 0) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    if ((size_t)removed_idx < fsm->state_count - 1) {
        memmove(&fsm->states[removed_idx], &fsm->states[removed_idx + 1],
                (fsm->state_count - removed_idx - 1) * sizeof(struct s21_fsm_state));
    }
    
    fsm->state_count--;
    
    /* Если есть место для уменьшения выделенной памяти, можно сделать realloc */
    /* Но для простоты оставляем как есть */
    
    return S21_FSM_OK;
}

/**
 * @brief Увеличивает ёмкость массива состояний FSM.
 *
 * @param[in,out] fsm Указатель на FSM, для которой увеличивается массив состояний.
 *
 * @return Код результата
 * @retval S21_FSM_OK             при успешном увеличении ёмкости.
 * @retval S21_FSM_NULL_POINTER   если @p fsm равен NULL.
 * @retval S21_FSM_ALLOCATION_ERROR если @c realloc для массива состояний завершился неудачно.
 */
static int s21_fsm_grow_states(s21_fsm_t *fsm) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    size_t new_capacity = fsm->state_capacity + fsm->state_alloc_step;
    struct s21_fsm_state *new_states = realloc(fsm->states, new_capacity * sizeof(struct s21_fsm_state));
    if (!new_states) {
        return S21_FSM_ALLOCATION_ERROR;
    }
    
    fsm->states = new_states;
    fsm->state_capacity = new_capacity;
    
    return S21_FSM_OK;
}

/**
 * @brief Увеличивает ёмкость массива переходов состояния.
 *
 * @param[in,out] state Указатель на состояние, для которого увеличивается массив переходов.
 *
 * @return Код результата
 * @retval S21_FSM_OK             при успешном увеличении ёмкости.
 * @retval S21_FSM_NULL_POINTER   если @p state равен NULL.
 * @retval S21_FSM_ALLOCATION_ERROR если @c realloc для массива переходов завершился неудачно.
 */
static int s21_fsm_grow_transitions(struct s21_fsm_state *state) {
    if (!state) {
        return S21_FSM_NULL_POINTER;
    }
    
    size_t new_capacity = state->transition_capacity + state->transition_alloc_step;
    struct s21_fsm_transition *new_transitions = realloc(state->transitions,
                                                          new_capacity * sizeof(struct s21_fsm_transition));
    if (!new_transitions) {
        return S21_FSM_ALLOCATION_ERROR;
    }
    
    state->transitions = new_transitions;
    state->transition_capacity = new_capacity;
    
    return S21_FSM_OK;
}

/* --- 4.1. Создание, уничтожение и глобальный контекст --- */

s21_fsm_t *s21_fsm_create(void *user_ctx) {
    s21_fsm_t *fsm = malloc(sizeof(s21_fsm_t));
    if (!fsm) {
        return NULL;
    }
    
    fsm->states = malloc(S21_FSM_DEFAULT_STATE_CAPACITY * sizeof(struct s21_fsm_state));
    if (!fsm->states) {
        free(fsm);
        return NULL;
    }
    
    fsm->state_count = 0;
    fsm->state_capacity = S21_FSM_DEFAULT_STATE_CAPACITY;
    fsm->state_alloc_step = S21_FSM_DEFAULT_STATE_ALLOC_STEP;
    fsm->user_ctx = user_ctx;
    fsm->initial_state = S21_FSM_INVALID_STATE_ID;
    fsm->current_state = S21_FSM_INVALID_STATE_ID;
    fsm->initialized = false;
    fsm->in_dispatch = false;
    fsm->in_update = false;
    
    return fsm;
}

void s21_fsm_destroy(s21_fsm_t *fsm) {
    if (!fsm) {
        return;
    }
    
    for (size_t i = 0; i < fsm->state_count; i++) {
        if (fsm->states[i].transitions) {
            free(fsm->states[i].transitions);
            fsm->states[i].transitions = NULL;
        }
    }
    
    if (fsm->states) {
        free(fsm->states);
        fsm->states = NULL;
    }
    
    free(fsm);
}

int s21_fsm_set_context(s21_fsm_t *fsm, void *user_ctx) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    fsm->user_ctx = user_ctx;
    return S21_FSM_OK;
}

void *s21_fsm_get_context(s21_fsm_t *fsm) {
    if (!fsm) {
        return NULL;
    }
    return fsm->user_ctx;
}

/* --- 4.2. Состояния: регистрация, управление, контексты --- */

int s21_fsm_register_state(s21_fsm_t *fsm,
                           s21_state_id_t state_id,
                           s21_fsm_state_callback_t on_entry,
                           s21_fsm_state_callback_t on_do,
                           s21_fsm_state_callback_t on_exit,
                           void *state_ctx) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    /* Проверка на дубликат */
    if (s21_fsm_find_state(fsm, state_id) != NULL) {
        return S21_FSM_STATE_EXISTS;
    }
    
    /* Проверка, нужно ли расширить массив */
    if (fsm->state_count >= fsm->state_capacity) {
        int ret = s21_fsm_grow_states(fsm);
        if (ret != S21_FSM_OK) {
            return ret;
        }
    }
    
    /* Регистрация нового состояния */
    struct s21_fsm_state *state = &fsm->states[fsm->state_count];
    state->id = state_id;
    state->transitions = NULL;
    state->transition_count = 0;
    state->transition_capacity = S21_FSM_DEFAULT_TRANSITION_CAPACITY;
    state->transition_alloc_step = S21_FSM_DEFAULT_TRANSITION_ALLOC_STEP;
    state->on_entry = on_entry;
    state->on_do = on_do;
    state->on_exit = on_exit;
    state->ctx = state_ctx;
    
    fsm->state_count++;
    
    return S21_FSM_OK;
}

int s21_fsm_update_state(s21_fsm_t *fsm,
                         s21_state_id_t state_id,
                         s21_fsm_state_callback_t on_entry,
                         s21_fsm_state_callback_t on_do,
                         s21_fsm_state_callback_t on_exit,
                         void *state_ctx) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    struct s21_fsm_state *state = s21_fsm_find_state(fsm, state_id);
    if (!state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    state->on_entry = on_entry;
    state->on_do = on_do;
    state->on_exit = on_exit;
    state->ctx = state_ctx;
    
    return S21_FSM_OK;
}

int s21_fsm_unregister_state(s21_fsm_t *fsm, s21_state_id_t state_id) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    if (!s21_fsm_find_state(fsm, state_id)) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    /* Удаление всех переходов, ведущих в это состояние */
    s21_fsm_remove_transitions_to_state(fsm, state_id);
    
    /* Обновление current_state и initial_state, если нужно */
    if (fsm->current_state == state_id) {
        fsm->current_state = S21_FSM_INVALID_STATE_ID;
    }
    if (fsm->initial_state == state_id) {
        fsm->initial_state = S21_FSM_INVALID_STATE_ID;
    }
    
    /* Освобождение переходов удаляемого состояния */
    struct s21_fsm_state *state = s21_fsm_find_state(fsm, state_id);
    if (state && state->transitions) {
        free(state->transitions);
        state->transitions = NULL;
    }
    
    /* Уплотнение массива состояний */
    return s21_fsm_compact_states(fsm, state_id);
}

int s21_fsm_set_initial_state(s21_fsm_t *fsm, s21_state_id_t state_id) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    if (!s21_fsm_find_state(fsm, state_id)) {
        fsm->initial_state = S21_FSM_INVALID_STATE_ID;
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    fsm->initial_state = state_id;
    return S21_FSM_OK;
}

s21_state_id_t s21_fsm_get_initial_state(const s21_fsm_t *fsm) {
    if (!fsm) {
        return S21_FSM_INVALID_STATE_ID;
    }
    return fsm->initial_state;
}

s21_state_id_t s21_fsm_get_current_state(const s21_fsm_t *fsm) {
    if (!fsm) {
        return S21_FSM_INVALID_STATE_ID;
    }
    return fsm->current_state;
}

int s21_fsm_set_state_context(s21_fsm_t *fsm,
                              s21_state_id_t state_id,
                              void *state_ctx) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    struct s21_fsm_state *state = s21_fsm_find_state(fsm, state_id);
    if (!state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    state->ctx = state_ctx;
    return S21_FSM_OK;
}

int s21_fsm_get_state_context(s21_fsm_t *fsm,
                              s21_state_id_t state_id,
                              void **out_state_ctx) {
    if (!fsm || !out_state_ctx) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    struct s21_fsm_state *state = s21_fsm_find_state(fsm, state_id);
    if (!state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    *out_state_ctx = state->ctx;
    return S21_FSM_OK;
}

/* --- 4.3. Переходы: регистрация, управление, контексты --- */

int s21_fsm_register_transition(s21_fsm_t *fsm,
                                s21_state_id_t src_state_id,
                                s21_state_id_t dest_state_id,
                                s21_event_id_t trigger,
                                s21_fsm_guard_callback_t on_guard,
                                s21_fsm_effect_callback_t on_effect,
                                void *transition_ctx) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (src_state_id < 0 || dest_state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    if (trigger < 0) {
        return S21_FSM_EVENT_INCORRECT_ID;
    }
    
    /* Проверка существования состояний */
    struct s21_fsm_state *src_state = s21_fsm_find_state(fsm, src_state_id);
    if (!src_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    struct s21_fsm_state *dest_state = s21_fsm_find_state(fsm, dest_state_id);
    if (!dest_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    /* Проверка, нужно ли расширить массив переходов */
    if (src_state->transition_count >= src_state->transition_capacity) {
        int ret = s21_fsm_grow_transitions(src_state);
        if (ret != S21_FSM_OK) {
            return ret;
        }
    }
    
    /* Регистрация нового перехода */
    struct s21_fsm_transition *transition = &src_state->transitions[src_state->transition_count];
    transition->trigger = trigger;
    transition->target_state_id = dest_state_id;
    transition->on_guard = on_guard;
    transition->on_effect = on_effect;
    transition->ctx = transition_ctx;
    
    src_state->transition_count++;
    
    return S21_FSM_OK;
}

int s21_fsm_unregister_transition_exact(s21_fsm_t *fsm,
                                        s21_state_id_t src_state_id,
                                        s21_event_id_t trigger,
                                        s21_state_id_t dest_state_id,
                                        s21_fsm_guard_callback_t on_guard,
                                        s21_fsm_effect_callback_t on_effect) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (src_state_id < 0 || dest_state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    if (trigger < 0) {
        return S21_FSM_EVENT_INCORRECT_ID;
    }
    
    struct s21_fsm_state *src_state = s21_fsm_find_state(fsm, src_state_id);
    if (!src_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    int idx = s21_fsm_find_transition_index(src_state, trigger, dest_state_id, on_guard, on_effect);
    if (idx < 0) {
        return S21_FSM_TRANSITION_NOT_FOUND;
    }
    
    s21_fsm_remove_transition_at(src_state, idx);
    
    return S21_FSM_OK;
}

int s21_fsm_clear_transitions_by_trigger(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id,
                                         s21_event_id_t trigger) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (src_state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    if (trigger < 0) {
        return S21_FSM_EVENT_INCORRECT_ID;
    }
    
    struct s21_fsm_state *src_state = s21_fsm_find_state(fsm, src_state_id);
    if (!src_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    /* Удаляем все переходы с данным trigger */
    for (size_t i = 0; i < src_state->transition_count; ) {
        if (src_state->transitions[i].trigger == trigger) {
            s21_fsm_remove_transition_at(src_state, (int)i);
        } else {
            i++;
        }
    }
    
    return S21_FSM_OK;
}

int s21_fsm_clear_transitions_from_state(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (src_state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    struct s21_fsm_state *src_state = s21_fsm_find_state(fsm, src_state_id);
    if (!src_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    /* Освобождаем память массива переходов */
    if (src_state->transitions) {
        free(src_state->transitions);
        src_state->transitions = NULL;
    }
    src_state->transition_count = 0;
    src_state->transition_capacity = 0;
    
    return S21_FSM_OK;
}

int s21_fsm_set_transition_context_exact(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id,
                                         s21_event_id_t trigger,
                                         s21_state_id_t dest_state_id,
                                         s21_fsm_guard_callback_t on_guard,
                                         s21_fsm_effect_callback_t on_effect,
                                         void *transition_ctx) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (src_state_id < 0 || dest_state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    if (trigger < 0) {
        return S21_FSM_EVENT_INCORRECT_ID;
    }
    
    struct s21_fsm_state *src_state = s21_fsm_find_state(fsm, src_state_id);
    if (!src_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    int idx = s21_fsm_find_transition_index(src_state, trigger, dest_state_id, on_guard, on_effect);
    if (idx < 0) {
        return S21_FSM_TRANSITION_NOT_FOUND;
    }
    
    src_state->transitions[idx].ctx = transition_ctx;
    return S21_FSM_OK;
}

int s21_fsm_get_transition_context_exact(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id,
                                         s21_event_id_t trigger,
                                         s21_state_id_t dest_state_id,
                                         s21_fsm_guard_callback_t on_guard,
                                         s21_fsm_effect_callback_t on_effect,
                                         void **out_transition_ctx) {
    if (!fsm || !out_transition_ctx) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (src_state_id < 0 || dest_state_id < 0) {
        return S21_FSM_STATE_INCORRECT_ID;
    }
    
    if (trigger < 0) {
        return S21_FSM_EVENT_INCORRECT_ID;
    }
    
    struct s21_fsm_state *src_state = s21_fsm_find_state(fsm, src_state_id);
    if (!src_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    int idx = s21_fsm_find_transition_index(src_state, trigger, dest_state_id, on_guard, on_effect);
    if (idx < 0) {
        return S21_FSM_TRANSITION_NOT_FOUND;
    }
    
    *out_transition_ctx = src_state->transitions[idx].ctx;
    return S21_FSM_OK;
}

/* --- 4.4. Обработка событий --- */

int s21_fsm_dispatch(s21_fsm_t *fsm, s21_event_id_t event) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (event < 0) {
        return S21_FSM_EVENT_INCORRECT_ID;
    }
    
    if (!fsm->initialized) {
        return S21_FSM_NOT_INITIALIZED;
    }
    
    if (fsm->in_dispatch || fsm->in_update) {
        return S21_FSM_REENTRANT_CALL;
    }
    
    fsm->in_dispatch = true;
    
    /* Поиск текущего состояния */
    struct s21_fsm_state *current_state = s21_fsm_find_state(fsm, fsm->current_state);
    if (!current_state) {
        fsm->in_dispatch = false;
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    /* Поиск подходящего перехода */
    int transition_idx = -1;
    for (size_t i = 0; i < current_state->transition_count; i++) {
        if (current_state->transitions[i].trigger == event) {
            /* Guard проверка: если on_guard == NULL или возвращает true */
            if (!current_state->transitions[i].on_guard ||
                current_state->transitions[i].on_guard(current_state->transitions[i].ctx)) {
                transition_idx = (int)i;
                break;
            }
        }
    }
    
    if (transition_idx < 0) {
        fsm->in_dispatch = false;
        return S21_FSM_NO_TRANSITION;
    }
    
    struct s21_fsm_transition *transition = &current_state->transitions[transition_idx];
    
    /* Выполнение последовательности действий */
    /* 1. on_exit текущего состояния */
    if (current_state->on_exit) {
        current_state->on_exit(current_state->ctx);
    }
    
    /* 2. on_effect перехода */
    if (transition->on_effect) {
        transition->on_effect(transition->ctx);
    }
    
    /* 3. Обновление current_state */
    fsm->current_state = transition->target_state_id;
    
    /* 4. on_entry нового состояния */
    struct s21_fsm_state *new_state = s21_fsm_find_state(fsm, fsm->current_state);
    if (new_state && new_state->on_entry) {
        new_state->on_entry(new_state->ctx);
    }
    
    fsm->in_dispatch = false;
    
    return S21_FSM_OK;
}

/* --- 4.5. Обновление состояния --- */

int s21_fsm_update(s21_fsm_t *fsm) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (!fsm->initialized) {
        return S21_FSM_NOT_INITIALIZED;
    }
    
    if (fsm->in_update || fsm->in_dispatch) {
        return S21_FSM_REENTRANT_CALL;
    }
    
    /* Поиск текущего состояния */
    struct s21_fsm_state *current_state = s21_fsm_find_state(fsm, fsm->current_state);
    if (!current_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    /* Вызов on_do, если не NULL */
    if (current_state->on_do) {
        fsm->in_update = true;
        current_state->on_do(current_state->ctx);
        fsm->in_update = false;
    }
    
    return S21_FSM_OK;
}

/* --- 4.6. Инициализация и сброс --- */

int s21_fsm_initialize(s21_fsm_t *fsm) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (fsm->state_count == 0) {
        return S21_FSM_ERROR;
    }
    
    if (fsm->initial_state == S21_FSM_INVALID_STATE_ID) {
        return S21_FSM_ERROR;
    }
    
    /* Поиск начального состояния */
    struct s21_fsm_state *initial_state = s21_fsm_find_state(fsm, fsm->initial_state);
    if (!initial_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    fsm->current_state = fsm->initial_state;
    fsm->initialized = true;
    
    fsm->in_dispatch = true;
    /* Вызов on_entry начального состояния */
    if (initial_state->on_entry) {
        initial_state->on_entry(initial_state->ctx);
    }
    fsm->in_dispatch = false;
    
    return S21_FSM_OK;
}

bool s21_fsm_is_initialized(const s21_fsm_t *fsm) {
    if (!fsm) {
        return false;
    }
    return fsm->initialized;
}

int s21_fsm_reset(s21_fsm_t *fsm) {
    if (!fsm) {
        return S21_FSM_NULL_POINTER;
    }
    
    if (fsm->initial_state == S21_FSM_INVALID_STATE_ID) {
        return S21_FSM_ERROR;
    }
    
    /* Поиск начального состояния */
    struct s21_fsm_state *initial_state = s21_fsm_find_state(fsm, fsm->initial_state);
    if (!initial_state) {
        return S21_FSM_STATE_NOT_FOUND;
    }
    
    fsm->current_state = fsm->initial_state;
    fsm->initialized = true;
    fsm->in_dispatch = false;
    fsm->in_update = false;

    /* Вызов on_entry начального состояния */
    if (initial_state->on_entry) {
        initial_state->on_entry(initial_state->ctx);
    }
    
    return S21_FSM_OK;
}

/* --- Тестовые функции для доступа к внутренним полям --- */
/* Эти функции не объявлены в публичном заголовке, доступны только для тестов. */
/* Включаются только при определении TEST_INTERNAL */

#ifdef TEST_INTERNAL

size_t s21_fsm_get_state_count(const s21_fsm_t *fsm) {
    if (!fsm) {
        return 0;
    }
    return fsm->state_count;
}

size_t s21_fsm_get_state_capacity(const s21_fsm_t *fsm) {
    if (!fsm) {
        return 0;
    }
    return fsm->state_capacity;
}

size_t s21_fsm_get_transition_count(const struct s21_fsm_state *state) {
    if (!state) {
        return 0;
    }
    return state->transition_count;
}

size_t s21_fsm_get_transition_capacity(const struct s21_fsm_state *state) {
    if (!state) {
        return 0;
    }
    return state->transition_capacity;
}

struct s21_fsm_state *s21_fsm_get_state_by_index(s21_fsm_t *fsm, size_t index) {
    if (!fsm || index >= fsm->state_count) {
        return NULL;
    }
    return &fsm->states[index];
}

void s21_fsm_test_set_current_state(s21_fsm_t *fsm, s21_state_id_t state_id) {
    if (fsm) {
        fsm->current_state = state_id;
    }
}

void s21_fsm_test_set_initial_state_raw(s21_fsm_t *fsm, s21_state_id_t state_id) {
    if (fsm) {
        fsm->initial_state = state_id;
    }
}

void s21_fsm_test_set_in_dispatch(s21_fsm_t *fsm, bool value) {
    if (fsm) {
        fsm->in_dispatch = value;
    }
}

void s21_fsm_test_set_in_update(s21_fsm_t *fsm, bool value) {
    if (fsm) {
        fsm->in_update = value;
    }
}

void s21_fsm_test_fail_next_malloc(void) { g_test_malloc_fail_countdown = 0; }

void s21_fsm_test_fail_second_malloc(void) { g_test_malloc_fail_countdown = 1; }

void s21_fsm_test_fail_next_realloc(void) { g_test_realloc_fail_countdown = 0; }

void s21_fsm_test_fail_second_realloc(void) { g_test_realloc_fail_countdown = 1; }

int s21_fsm_test_find_transition_index_null(void) {
    return s21_fsm_find_transition_index(NULL, 0, 0, NULL, NULL);
}

void s21_fsm_test_remove_transition_at_invalid(void) {
    s21_fsm_remove_transition_at(NULL, 0);
}

void s21_fsm_test_remove_transitions_to_state_invalid(s21_fsm_t *fsm) {
    s21_fsm_remove_transitions_to_state(NULL, 0);
    s21_fsm_remove_transitions_to_state(fsm, -1);
}

int s21_fsm_test_compact_states_invalid(s21_fsm_t *fsm, s21_state_id_t id) {
    return s21_fsm_compact_states(fsm, id);
}

int s21_fsm_test_grow_states_null(void) { return s21_fsm_grow_states(NULL); }

int s21_fsm_test_grow_transitions_null(void) {
    return s21_fsm_grow_transitions(NULL);
}

#endif /* TEST_INTERNAL */