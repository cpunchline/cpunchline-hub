#include <stdio.h>
#include "utility/fsm.h"

// 定义数据域
typedef struct
{
    int cnt;
} SM_DATA;

/* ------------------- Functions -----------------------------*/
/* 状态机 函数指针 区域 */
void *FSM_FUNCT(init)(void *this_fsm);  // 初始化状态
void *FSM_FUNCT(count)(void *this_fsm); // 计数状态
void *FSM_FUNCT(done)(void *this_fsm);  // 执行完成状态
void *FSM_FUNCT(err)(void *this_fsm);   // 错误状态

/* 状态ID , 顺序要求与 procedure_list[] 对应 */
enum procedure_id
{
    FSM_STATE(init),
    FSM_STATE(count),
    FSM_STATE(done),
    FSM_STATE(err),
};

/* 状态机跳转列表 */
static Procedure procedure_list[] = {
    FSM_FUNCT(init),
    FSM_FUNCT(count),
    FSM_FUNCT(done),
    FSM_FUNCT(err),
};

void *FSM_FUNCT(init)(void *this_fsm)
{
    SM_DATA *pd = get_data_entry(this_fsm);

    // 下次step时的状态
    set_next_state(this_fsm, FSM_STATE(count));

    // 数据处理
    pd->cnt = 0;
    printf("INIT\n");

    return NULL;
}

void *FSM_FUNCT(count)(void *this_fsm) // 计数
{
    SM_DATA *pd = get_data_entry(this_fsm);

    pd->cnt++;

    printf("COUNT, %d\n", pd->cnt);
    if ((pd->cnt) >= 3)
    {
        set_next_state(this_fsm, FSM_STATE(done));
    }

    return NULL;
}

void *FSM_FUNCT(done)(void *this_fsm) // 计数完成
{
    // 驻留
    set_next_state(this_fsm, FSM_STATE(done));
    printf("DONE\n");
    return NULL;
}

void *FSM_FUNCT(err)(void *this_fsm) // 错误状态
{
    int *err_var;
    // 驻留(实际上, 若不修改状态, 则一直重复当前状态)
    set_next_state(this_fsm, FSM_STATE(err));

    // 通知 调用者 有错误发生
    set_fsm_error_flag(this_fsm);

    // 把 错误值 设进 容器中(如果容器存在)
    err_var = get_err_var(this_fsm);
    if (err_var)
        *err_var = 0xff;

    return NULL;
}

////////////////////////// 事件处理 //////////////////////////
// 在FSM_FUNCT执行前会调用此函数, 以告知即将进入的状态
static void fsm_state_entered(void *this_fsm, state to)
{
    (void)this_fsm;
    printf("[Note]Enter to %d\n", to);

    if (to == FSM_STATE(init))
    {
        printf("[Note]Enter to [init]\n");
    }
}

// 在FSM_FUNCT执行后会调用此函数, 以告知已经退出的状态
static void fsm_state_exited(void *this_fsm, state from)
{
    (void)this_fsm;
    printf("[Note]Exit from state %d\n", from);
    if (from == FSM_STATE(count))
    {
        printf("[Note]Exit from [count]\n");
    }
}

// 在FSM_FUNCT执行后会调用此函数, 以告知已经从from状态退出并即将进入to状态
static void fsm_state_changed_from_to(void *this_fsm, state from, state to)
{
    (void)this_fsm;
    printf("[Note]Exit from state %d, goting to %d\n", from, to);
}

int main(void)
{
    int i = 0;

    state pre_state, cur_state, nxt_state;
    FSM fsm_1 = {};    // 状态机实例
    SM_DATA data = {}; // 状态机需要的数据实例
    int err_code;      // 用于出错处理
    int *got_err;

    set_procedures(&fsm_1, procedure_list);                                    // 设置 状态机
    set_data_entry(&fsm_1, &data);                                             // 指定自定义数据
    set_default_state(&fsm_1, FSM_STATE(init));                                // 设置默认状态
    set_err_var(&fsm_1, &err_code);                                            // 设置错误处理时所使用的变量空间
    set_callback_when_state_entered(&fsm_1, fsm_state_entered);                // 设置通知行为:状态进入
    set_callback_when_state_exited(&fsm_1, fsm_state_exited);                  // 设置通知行为:状态退出
    set_callback_when_state_change_from_to(&fsm_1, fsm_state_changed_from_to); // 设置通知行为:状态切换(相当于entered和exited)

    while (1)
    {
        ++i;
        printf("========[%d]========\n", i);

        // 获取状态机执行之前的状态
        pre_state = get_curr_state(&fsm_1);
        printf("Before runing: %d\n", pre_state);

        // 执行状态机, 使其步进一次
        cur_state = run_state_machine_once(&fsm_1);
        printf("Ran :%d\n", cur_state);

        // 获取状态机下次执行的状态
        nxt_state = get_next_state(&fsm_1);
        printf("Next :%d\n", nxt_state);

        // 判断是否出错
        if (is_fsm_error(&fsm_1))
        {
            printf("Error when Stepping !\n");
            got_err = get_err_var(&fsm_1);
            printf("Get Error Code :0x%x\n", *got_err);
            clr_fsm_error_flag(&fsm_1);
            break;
        }

        /* 下列2种情况下的 状态机结束时机 不同 */
#if 1
        // 1,内部执行完成done状态过程, 下次才会退出
        // (如果执行过done, 则不再执行)
        if (is_curr_state(&fsm_1, FSM_STATE(done)))
        {
            printf("Done after fsm.done\n");
            reset_state_machine(&fsm_1);
            break;
        }
#else
        // 2,在执行done过程之前, 就退出
        // 状态机的done状态过程不会被执行
        if (is_next_state(&fsm_1, FSM_STATE(done)))
        {
            printf("Done before fsm.done\n");
            reset_state_machine(&fsm_1);
            break;
        }
#endif
    }

    return 0;
}
