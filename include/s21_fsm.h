#ifndef S21_FSM_H
#define S21_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Непрозрачный тип, представляющий экземпляр конечного автомата (FSM).
 * 
 * @note Внешний код работает только с указателями `s21_fsm_t *` через функции публичного API.
 */
typedef struct s21_fsm s21_fsm_t;

/**
 * @brief Тип идентификатора состояния FSM.
 *
 * @note Идентификатор должен быть неотрицательным (>= 0). Отрицательные значения
 * считаются ошибкой и приводят к кодам `S21_FSM_STATE_INCORRECT_ID` или
 * `S21_FSM_STATE_NOT_FOUND` в соответствующих функциях.
 */
typedef int32_t s21_state_id_t;

/**
 * @brief Тип идентификатора события (триггера) FSM.
 *
 * @note Идентификатор должен быть неотрицательным (>= 0). Отрицательные значения
 * считаются ошибкой и приводят к коду `S21_FSM_EVENT_INCORRECT_ID` при
 * регистрации переходов и обработке событий (`dispatch`).
 */
typedef int32_t s21_event_id_t;

/**
 * @brief Callback для действий состояния (entry, do, exit).
 *
 * Вызывается с контекстом состояния (`state_ctx`), если он задан.
 * Может быть `NULL`, в этом случае соответствующее действие не выполняется.
 */
typedef void (*s21_fsm_state_callback_t)(void *ctx);

/**
 * @brief Callback для проверки guard-условия перехода.
 *
 * Возвращает `true`, если переход разрешён, и `false` иначе. Если указатель
 * равен `NULL`, переход считается всегда разрешён.
 */
typedef bool (*s21_fsm_guard_callback_t)(void *ctx);

/**
 * @brief Callback для effect-действия перехода.
 *
 * Вызывается при успешном переходе после выхода из исходного состояния и
 * перед входом в целевое состояние. Может быть `NULL`, если действие
 * не требуется.
 */
typedef void (*s21_fsm_effect_callback_t)(void *ctx);

/**
 * @brief Специальное значение «невалидного» идентификатора состояния.
 *
 * Используется для обозначения отсутствующего начального или текущего
 * состояния. Функции API возвращают `S21_FSM_INVALID_STATE_ID`, если
 * указатель FSM равен `NULL` или состояние ещё не задано.
 */
#define S21_FSM_INVALID_STATE_ID      ((s21_state_id_t)-1)

/**
 * @brief Успешное выполнение операции без ошибок.
 *
 * Большинство функций API возвращают `S21_FSM_OK` при корректном выполнении
 * запроса (регистрация, обновление, удаление и т.д.).
 */
#define S21_FSM_OK                    (0)

/**
 * @brief Обобщённая ошибка выполнения операции.
 *
 * Используется в ситуациях, когда конкретный тип ошибки не детализирован,
 * например при некорректном состоянии автомата на этапе инициализации.
 */
#define S21_FSM_ERROR                 (-1)

/**
 * @brief Ошибка: передан нулевой указатель (`NULL`) вместо ожидаемого объекта.
 *
 * Возвращается, когда указатель на FSM, буфер вывода или другой обязательный
 * параметр равен `NULL`.
 */
#define S21_FSM_NULL_POINTER          (-2)

/**
 * @brief Ошибка: не удалось выделить или перераспределить память.
 *
 * Возвращается при ошибках `malloc`/`realloc` внутри библиотеки, например
 * при увеличении массивов состояний или переходов.
 */
#define S21_FSM_ALLOCATION_ERROR      (-3)

/**
 * @brief Ошибка: состояние с указанным идентификатором не найдено.
 *
 * Возвращается, когда операция требует существующего состояния
 * (обновление, удаление, установка контекста и т.п.), но оно отсутствует.
 */
#define S21_FSM_STATE_NOT_FOUND       (-4)

/**
 * @brief Ошибка: идентификатор состояния некорректен.
 *
 * Идентификатор состояния должен быть неотрицательным (>= 0).
 * Отрицательные значения приводят к `S21_FSM_STATE_INCORRECT_ID`.
 */
#define S21_FSM_STATE_INCORRECT_ID    (-5)

/**
 * @brief Ошибка: состояние с таким идентификатором уже существует.
 *
 * Возвращается при попытке зарегистрировать новое состояние с идентификатором,
 * который уже занят в текущем экземпляре FSM.
 */
#define S21_FSM_STATE_EXISTS          (-6)

/**
 * @brief Ошибка: указанный переход не найден.
 *
 * Возвращается функциями, которые ищут конкретный переход по набору
 * параметров (источник, событие, целевое состояние, callbacks).
 */
#define S21_FSM_TRANSITION_NOT_FOUND  (-7)

/**
 * @brief Ошибка: из текущего состояния нет подходящего перехода.
 *
 * Возвращается `s21_fsm_dispatch`, если для заданного события не найден
 * ни один разрешённый переход (по trigger и guard).
 */
#define S21_FSM_NO_TRANSITION         (-8)
/**
 * @brief Ошибка: идентификатор события некорректен.
 *
 * Идентификатор события должен быть неотрицательным (>= 0).
 * Отрицательные значения приводят к `S21_FSM_EVENT_INCORRECT_ID`.
 */
#define S21_FSM_EVENT_INCORRECT_ID    (-9)

/**
 * @brief Ошибка: автомат не инициализирован.
 *
 * Возвращается `s21_fsm_dispatch` и `s21_fsm_update`, если FSM ещё
 * не прошёл инициализацию (`initialized == false`).
 */
#define S21_FSM_NOT_INITIALIZED       (-10)

/**
 * @brief Ошибка: обнаружен реэнтрантный вызов FSM‑функции.
 *
 * Возвращается, если `dispatch` или `update` вызываются повторно
 * для того же экземпляра FSM изнутри текущего вызова.
 */
#define S21_FSM_REENTRANT_CALL        (-11)

/* --- Публичный API --- */

/**
 * @brief Создаёт новый экземпляр конечного автомата.
 *
 * @param[in] user_ctx Глобальный контекст автомата (может быть NULL),
 *        не интерпретируется библиотекой и не управляется ею.
 *
 * @return Указатель на созданный автомат при успехе,
 *         либо NULL, если не удалось выделить память.
 *
 * @note В случае ошибки аллокации внутренний автомат не создаётся,
 *       и вызывающему коду возвращается NULL без дополнительных
 *       кодов статуса.
 */
s21_fsm_t *s21_fsm_create(void *user_ctx);

/**
 * @brief Уничтожает экземпляр конечного автомата.
 *
 * @param[in,out] fsm Указатель на автомат, который нужно уничтожить;
 *                    если равен NULL, функция ничего не делает.
 *
 * @note Функция освобождает все внутренние ресурсы FSM, включая
 *       массив состояний и их переходов, и затем освобождает саму
 *       структуру автомата.
 */
void s21_fsm_destroy(s21_fsm_t *fsm);

/**
 * @brief Устанавливает глобальный пользовательский контекст FSM.
 *
 * @param[in,out] fsm Указатель на автомат, для которого изменяется контекст.
 * @param[in]     user_ctx Новый глобальный контекст (может быть NULL),
 *                         не интерпретируется библиотекой и не управляется ею.
 *
 * @return Код ошибки
 * @retval S21_FSM_OK при успешной установке контекста,
 * @retval S21_FSM_NULL_POINTER, если @p fsm равен NULL.
 */
int s21_fsm_set_context(s21_fsm_t *fsm, void *user_ctx);

/**
 * @brief Возвращает указатель на глобальный пользовательский контекст FSM.
 *
 * @param[in] fsm Указатель на автомат, из которого считывается контекст.
 *
 * @return Текущее значение глобального контекста автомата,
 *         либо NULL, если @p fsm равен NULL или контекст не установлен.
 */
void *s21_fsm_get_context(s21_fsm_t *fsm);

/**
 * @brief Регистрирует новое состояние в автомате.
 *
 * @param[in,out] fsm       Указатель на FSM, в которую добавляется состояние.
 * @param[in]     state_id  Идентификатор состояния (должен быть ≥ 0 и уникальным).
 * @param[in]     on_entry  Callback, вызываемый при входе в состояние (может быть NULL).
 * @param[in]     on_do     Callback, вызываемый при обновлении состояния (`s21_fsm_update`) (может быть NULL).
 * @param[in]     on_exit   Callback, вызываемый при выходе из состояния при переходе (может быть NULL).
 * @param[in]     state_ctx Пользовательский контекст состояния, передаваемый во все его callbacks (может быть NULL).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешной регистрации состояния,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p state_id < 0,
 * @retval S21_FSM_STATE_EXISTS       если состояние с таким идентификатором уже зарегистрировано,
 * @retval S21_FSM_ALLOCATION_ERROR   если не удалось расширить массив состояний.
 */
int s21_fsm_register_state(s21_fsm_t *fsm,
                           s21_state_id_t state_id,
                           s21_fsm_state_callback_t on_entry,
                           s21_fsm_state_callback_t on_do,
                           s21_fsm_state_callback_t on_exit,
                           void *state_ctx);

/**
 * @brief Обновляет callbacks и контекст уже зарегистрированного состояния.
 *
 * @param[in,out] fsm       Указатель на FSM, в которой обновляется состояние.
 * @param[in]     state_id  Идентификатор состояния (должен быть ≥ 0).
 * @param[in]     on_entry  Новый callback для входа в состояние (может быть NULL).
 * @param[in]     on_do     Новый callback для обновления состояния (`s21_fsm_update`) (может быть NULL).
 * @param[in]     on_exit   Новый callback для выхода из состояния при переходе (может быть NULL).
 * @param[in]     state_ctx Новый пользовательский контекст состояния, передаваемый во все его callbacks (может быть NULL).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном обновлении состояния,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p state_id < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если состояние с идентификатором @p state_id не зарегистрировано.
 */
int s21_fsm_update_state(s21_fsm_t *fsm,
                         s21_state_id_t state_id,
                         s21_fsm_state_callback_t on_entry,
                         s21_fsm_state_callback_t on_do,
                         s21_fsm_state_callback_t on_exit,
                         void *state_ctx);

/**
 * @brief Удаляет зарегистрированное состояние из автомата.
 *
 * @param[in,out] fsm      Указатель на FSM, из которой удаляется состояние.
 * @param[in]     state_id Идентификатор состояния (должен быть ≥ 0).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном удалении состояния,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p state_id < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если состояние с идентификатором @p state_id не зарегистрировано.
 *
 * @note При удалении состояния все переходы, ведущие в него,
 *       удаляются, а поля @c current_state и @c initial_state
 *       сбрасываются в S21_FSM_INVALID_STATE_ID, если указывали
 *       на удаляемое состояние.
 */
int s21_fsm_unregister_state(s21_fsm_t *fsm, s21_state_id_t state_id);

/**
 * @brief Устанавливает начальное состояние автомата.
 *
 * @param[in,out] fsm      Указатель на FSM, для которой задаётся начальное состояние.
 * @param[in]     state_id Идентификатор состояния (должен быть ≥ 0 и заранее зарегистрирован).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешной установке начального состояния,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p state_id < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если состояние с идентификатором @p state_id не зарегистрировано
 *                                    (в этом случае @c initial_state сбрасывается в S21_FSM_INVALID_STATE_ID).
 */
int s21_fsm_set_initial_state(s21_fsm_t *fsm, s21_state_id_t state_id);

/**
 * @brief Возвращает идентификатор начального состояния автомата.
 *
 * @param[in] fsm Указатель на FSM, из которой считывается начальное состояние.
 *
 * @return Текущее значение поля @c initial_state,
 *         либо S21_FSM_INVALID_STATE_ID, если @p fsm равен NULL
 *         или начальное состояние не задано.
 */
s21_state_id_t s21_fsm_get_initial_state(const s21_fsm_t *fsm);

/**
 * @brief Возвращает идентификатор текущего состояния автомата.
 *
 * @param[in] fsm Указатель на FSM, из которой считывается текущее состояние.
 *
 * @return Текущее значение поля @c current_state,
 *         либо S21_FSM_INVALID_STATE_ID, если @p fsm равен NULL
 *         или автомат ещё не инициализирован и текущее состояние не задано.
 */
s21_state_id_t s21_fsm_get_current_state(const s21_fsm_t *fsm);

/**
 * @brief Устанавливает пользовательский контекст для состояния.
 *
 * @param[in,out] fsm       Указатель на FSM, в которой обновляется контекст состояния.
 * @param[in]     state_id  Идентификатор состояния (должен быть ≥ 0).
 * @param[in]     state_ctx Новый пользовательский контекст состояния (может быть NULL).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном обновлении контекста,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p state_id < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если состояние с идентификатором @p state_id не зарегистрировано.
 */
int s21_fsm_set_state_context(s21_fsm_t *fsm,
                              s21_state_id_t state_id,
                              void *state_ctx);

/**
 * @brief Возвращает пользовательский контекст состояния.
 *
 * @param[in]  fsm          Указатель на FSM, из которой считывается контекст.
 * @param[in]  state_id     Идентификатор состояния (должен быть ≥ 0).
 * @param[out] out_state_ctx Адрес указателя, в который будет помещён контекст состояния.
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном получении контекста,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL или @p out_state_ctx равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p state_id < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если состояние с идентификатором @p state_id не зарегистрировано.
 */
int s21_fsm_get_state_context(s21_fsm_t *fsm,
                              s21_state_id_t state_id,
                              void **out_state_ctx);

/**
 * @brief Регистрирует новый переход из исходного состояния.
 *
 * @param[in,out] fsm            Указатель на FSM, в которой создаётся переход.
 * @param[in]     src_state_id   Идентификатор исходного состояния (должен быть ≥ 0 и существовать).
 * @param[in]     dest_state_id  Идентификатор целевого состояния (должен быть ≥ 0 и существовать).
 * @param[in]     trigger        Идентификатор события‑триггера (должен быть ≥ 0).
 * @param[in]     on_guard       Guard‑callback; если NULL, переход всегда разрешён.
 * @param[in]     on_effect      Effect‑callback, вызываемый при выполнении перехода (может быть NULL).
 * @param[in]     transition_ctx Пользовательский контекст перехода, передаваемый в его callbacks (может быть NULL).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешной регистрации перехода,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p src_state_id < 0 или @p dest_state_id < 0,
 * @retval S21_FSM_EVENT_INCORRECT_ID если @p trigger < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если исходное или целевое состояние не найдено,
 * @retval S21_FSM_ALLOCATION_ERROR   если не удалось расширить массив переходов исходного состояния.
 */
int s21_fsm_register_transition(s21_fsm_t *fsm,
                                s21_state_id_t src_state_id,
                                s21_state_id_t dest_state_id,
                                s21_event_id_t trigger,
                                s21_fsm_guard_callback_t on_guard,
                                s21_fsm_effect_callback_t on_effect,
                                void *transition_ctx);

/**
 * @brief Удаляет конкретный переход, совпадающий по всем параметрам.
 *
 * @param[in,out] fsm           Указатель на FSM, из которой удаляется переход.
 * @param[in]     src_state_id  Идентификатор исходного состояния (должен быть ≥ 0).
 * @param[in]     trigger       Идентификатор события‑триггера (должен быть ≥ 0).
 * @param[in]     dest_state_id Идентификатор целевого состояния (должен быть ≥ 0).
 * @param[in]     on_guard      Guard‑callback перехода (может быть NULL).
 * @param[in]     on_effect     Effect‑callback перехода (может быть NULL).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном удалении перехода,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p src_state_id < 0 или @p dest_state_id < 0,
 * @retval S21_FSM_EVENT_INCORRECT_ID если @p trigger < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если исходное состояние не найдено,
 * @retval S21_FSM_TRANSITION_NOT_FOUND если переход с указанными параметрами отсутствует.
 */
int s21_fsm_unregister_transition_exact(s21_fsm_t *fsm,
                                        s21_state_id_t src_state_id,
                                        s21_event_id_t trigger,
                                        s21_state_id_t dest_state_id,
                                        s21_fsm_guard_callback_t on_guard,
                                        s21_fsm_effect_callback_t on_effect);

/**
 * @brief Удаляет все переходы с указанным trigger из исходного состояния.
 *
 * @param[in,out] fsm          Указатель на FSM, в которой выполняется очистка переходов.
 * @param[in]     src_state_id Идентификатор исходного состояния (должен быть ≥ 0).
 * @param[in]     trigger      Идентификатор события‑триггера (должен быть ≥ 0).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном удалении переходов,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p src_state_id < 0,
 * @retval S21_FSM_EVENT_INCORRECT_ID если @p trigger < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если состояние с идентификатором @p src_state_id не зарегистрировано.
 */                                        
int s21_fsm_clear_transitions_by_trigger(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id,
                                         s21_event_id_t trigger);

/**
 * @brief Удаляет все переходы, исходящие из указанного состояния.
 *
 * @param[in,out] fsm          Указатель на FSM, в которой выполняется очистка переходов.
 * @param[in]     src_state_id Идентификатор исходного состояния (должен быть ≥ 0).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном удалении всех переходов,
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID если @p src_state_id < 0,
 * @retval S21_FSM_STATE_NOT_FOUND    если состояние с идентификатором @p src_state_id не зарегистрировано.
 */
int s21_fsm_clear_transitions_from_state(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id);

/**
 * @brief Устанавливает пользовательский контекст для конкретного перехода.
 *
 * @param[in,out] fsm           Указатель на FSM, в которой обновляется контекст перехода.
 * @param[in]     src_state_id  Идентификатор исходного состояния (должен быть ≥ 0).
 * @param[in]     trigger       Идентификатор события‑триггера (должен быть ≥ 0).
 * @param[in]     dest_state_id Идентификатор целевого состояния (должен быть ≥ 0).
 * @param[in]     on_guard      Guard‑callback перехода (может быть NULL).
 * @param[in]     on_effect     Effect‑callback перехода (может быть NULL).
 * @param[in]     transition_ctx Новый пользовательский контекст перехода (может быть NULL).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                   при успешном обновлении контекста,
 * @retval S21_FSM_NULL_POINTER         если @p fsm равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID   если @p src_state_id < 0 или @p dest_state_id < 0,
 * @retval S21_FSM_EVENT_INCORRECT_ID   если @p trigger < 0,
 * @retval S21_FSM_STATE_NOT_FOUND      если исходное состояние не найдено,
 * @retval S21_FSM_TRANSITION_NOT_FOUND если переход с указанными параметрами отсутствует.
 */
int s21_fsm_set_transition_context_exact(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id,
                                         s21_event_id_t trigger,
                                         s21_state_id_t dest_state_id,
                                         s21_fsm_guard_callback_t on_guard,
                                         s21_fsm_effect_callback_t on_effect,
                                         void *transition_ctx);

/**
 * @brief Возвращает пользовательский контекст для конкретного перехода.
 *
 * @param[in]  fsm           Указатель на FSM, из которой считывается контекст перехода.
 * @param[in]  src_state_id  Идентификатор исходного состояния (должен быть ≥ 0).
 * @param[in]  trigger       Идентификатор события‑триггера (должен быть ≥ 0).
 * @param[in]  dest_state_id Идентификатор целевого состояния (должен быть ≥ 0).
 * @param[in]  on_guard      Guard‑callback перехода (может быть NULL).
 * @param[in]  on_effect     Effect‑callback перехода (может быть NULL).
 * @param[out] out_transition_ctx Адрес указателя, в который будет помещён контекст перехода.
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                   при успешном получении контекста,
 * @retval S21_FSM_NULL_POINTER         если @p fsm равен NULL или @p out_transition_ctx равен NULL,
 * @retval S21_FSM_STATE_INCORRECT_ID   если @p src_state_id < 0 или @p dest_state_id < 0,
 * @retval S21_FSM_EVENT_INCORRECT_ID   если @p trigger < 0,
 * @retval S21_FSM_STATE_NOT_FOUND      если исходное состояние не найдено,
 * @retval S21_FSM_TRANSITION_NOT_FOUND если переход с указанными параметрами отсутствует.
 */
int s21_fsm_get_transition_context_exact(s21_fsm_t *fsm,
                                         s21_state_id_t src_state_id,
                                         s21_event_id_t trigger,
                                         s21_state_id_t dest_state_id,
                                         s21_fsm_guard_callback_t on_guard,
                                         s21_fsm_effect_callback_t on_effect,
                                         void **out_transition_ctx);

/**
 * @brief Обрабатывает событие и выполняет переход из текущего состояния FSM.
 *
 * @param[in,out] fsm   Указатель на FSM, для которой обрабатывается событие.
 * @param[in]     event Идентификатор события (должен быть ≥ 0).
 *
 * @return Код ошибки
 * @retval S21_FSM_OK                 при успешном выполнении перехода.
 * @retval S21_FSM_NULL_POINTER       если @p fsm равен NULL.
 * @retval S21_FSM_EVENT_INCORRECT_ID если @p event < 0.
 * @retval S21_FSM_NOT_INITIALIZED    если FSM не была инициализирована (см. s21_fsm_initialize()).
 * @retval S21_FSM_REENTRANT_CALL     если функция вызвана реэнтрантно (во время другого dispatch/update).
 * @retval S21_FSM_STATE_NOT_FOUND    если текущее состояние не найдено.
 * @retval S21_FSM_NO_TRANSITION      если для данного события нет подходящего перехода из текущего состояния.
 */
int s21_fsm_dispatch(s21_fsm_t *fsm, s21_event_id_t event);

/**
 * @brief Вызывает callback @c on_do текущего состояния FSM (обновление состояния).
 *
 * @param[in,out] fsm Указатель на FSM, состояние которой обновляется.
 *
 * @return Код ошибки
 * @retval S21_FSM_OK              при успешном выполнении обновления.
 * @retval S21_FSM_NULL_POINTER    если @p fsm равен NULL.
 * @retval S21_FSM_NOT_INITIALIZED если FSM не была инициализирована (см. s21_fsm_initialize()).
 * @retval S21_FSM_REENTRANT_CALL  если функция вызвана реэнтрантно (во время другого dispatch/update).
 * @retval S21_FSM_STATE_NOT_FOUND если текущее состояние не найдено.
 */
int s21_fsm_update(s21_fsm_t *fsm);

/**
 * @brief Инициализирует FSM: устанавливает текущее состояние в начальное и вызывает его @c on_entry.
 *
 * @param[in,out] fsm Указатель на FSM, которую требуется инициализировать.
 *
 * @return Код ошибки
 * @retval S21_FSM_OK              при успешной инициализации FSM.
 * @retval S21_FSM_NULL_POINTER    если @p fsm равен NULL.
 * @retval S21_FSM_ERROR           если не зарегистрировано ни одного состояния
 *                                 или не установлен корректный начальный идентификатор состояния.
 * @retval S21_FSM_STATE_NOT_FOUND если начальное состояние не найдено.
 */
int s21_fsm_initialize(s21_fsm_t *fsm);

/**
 * @brief Проверяет, была ли FSM инициализирована.
 *
 * @param[in] fsm Указатель на FSM, для которой выполняется проверка.
 *
 * @return Флаг инициализации
 * @retval true  если FSM инициализирована (после успешного вызова s21_fsm_initialize() или s21_fsm_reset()).
 * @retval false если @p fsm равен NULL или FSM ещё не инициализирована.
 */
bool s21_fsm_is_initialized(const s21_fsm_t *fsm);

/**
 * @brief Сбрасывает FSM в начальное состояние и повторно вызывает его @c on_entry.
 *
 * @param[in,out] fsm Указатель на FSM, которую требуется сбросить.
 *
 * @return Код ошибки
 * @retval S21_FSM_OK              при успешном сбросе FSM.
 * @retval S21_FSM_NULL_POINTER    если @p fsm равен NULL.
 * @retval S21_FSM_ERROR           если начальное состояние не установлено
 *                                 (поле @c initial_state равно S21_FSM_INVALID_STATE_ID).
 * @retval S21_FSM_STATE_NOT_FOUND если начальное состояние не найдено.
 */
int s21_fsm_reset(s21_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* S21_FSM_H */
