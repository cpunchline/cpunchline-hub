#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <limits.h>

/* ----------------------- Defines ------------------------------------------*/
// 用于快速识别出 STATE与STEP
#define FSM_STATE(name) state_##name
#define FSM_FUNCT(name) funct_##name

// 数据类型定义区
typedef unsigned char state;
typedef long long step_ret;
typedef void *AS_STEP_RETVAL;

#define FSM_INVALID_STATE ((state) - 1)

/*!
 *  @brief  状态机过程实现原型函数
 *          设计状态机时需要按照这个模式写
 *
 *  @param[in]  fsm 当前的fsm实例
 *
 *  @return 返回值 最近一个执行状态的返回值(自定义的)
 */
typedef void *(*Procedure)(void *);

/*!
 *  @brief  状态机切换通知, 从当前状态跳转到下一个状态之前调用
 *
 *  @param[in]  fsm 当前的fsm实例
 *  @param[in]  from 从from状态离开
 *  @param[in]  to   切换为to状态
 *
 *  @return 无
 */
typedef void (*WhenStateChangedFromTo)(void *fsm, state from, state to);

/*!
 *  @brief  状态机切换通知, 当前状态变化时(进入新状态, 退出旧状态)
 *
 *  @param[in]  fsm 当前的fsm实例
 *  @param[in]  st  状态
 *
 *  @return 无
 */
typedef void (*WhenStateChanged)(void *fsm, state st);

typedef struct
{
    state ds; // 默认状态
    state cs; // 当前状态
    state ns; // 下个状态
} SM_STATE;

// 状态机 属性 定义
typedef struct
{
    // 状态管理
    SM_STATE st;

    // 状态机跳转表(每一个状态都需要)
    Procedure *procedures;
    // 通知行为(所有的状态切换通知的统一入口)
    WhenStateChanged when_state_changed_entered;       // 即将进入状态(执行自定义状态程序前)
    WhenStateChanged when_state_changed_exited;        // 退出状态(执行自定义状态以后并在下一轮会切换新的状态前)
    WhenStateChangedFromTo when_state_changed_from_to; // entered和exited的整合, 应用场景比较少, 不如前2个好用

    // 自定义数据区域
    void *data;

    // 错误处理(用于存放 状态 执行 的结果)
    step_ret ret_ptr; // 状态 执行结果
    void *err_ptr;
    state err_flag;
} FSM;

/* ----------------------- Start function declaration -----------------------------*/

/*!
 *  @brief  设置状态机的错误容器
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @param[in]  err_var 容器
 *
 *  @return 是/否
 */
static inline void set_err_var(FSM *fsm, void *err_var)
{
    if (!fsm)
        return;
    fsm->err_ptr = err_var;
}

/*!
 *  @brief  获取错误值容器(用于读取其中的内容)
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 是/否
 */
static inline void *get_err_var(FSM *fsm)
{
    return fsm->err_ptr;
}

/*!
 *  @brief  获取状态机 在 步进中是否遇到了错误
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 是(非0)/否(0)
 */
static inline state is_fsm_error(FSM *fsm)
{
    return fsm->err_flag;
}

/*!
 *  @brief  置 状态机 错误位
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 设置成功时返回0
 */
static inline state set_fsm_error_flag(FSM *fsm)
{
    if (!fsm)
        return UCHAR_MAX;
    fsm->err_flag = 1;
    return 0;
}

/*!
 *  @brief  清 状态机 错误位
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 设置成功时返回0
 */
static inline state clr_fsm_error_flag(FSM *fsm)
{
    if (!fsm)
        return UCHAR_MAX;
    fsm->err_flag = 0;
    return 0;
}

/*!
 *  @brief  为状态机添加 过程方法 序列
 *
 *  @param[in] fsm 状态机实例
 *
 *  @param[in] procedures 状态机的所有过程方法
 *
 */
static inline void set_procedures(FSM *fsm, Procedure *procedures)
{
    if (fsm)
    {
        fsm->procedures = procedures;
        fsm->st.cs = UCHAR_MAX; // 执行run之前, 当前状态是未定的
    }
}

/*!
 *  @brief  配置状态机的数据域
 *
 *  @param[in] fsm 状态机实例
 *
 *  @param[in] data 状态机需要的数据域
 *
 */
static inline void set_data_entry(FSM *fsm, void *data)
{
    if (fsm)
        fsm->data = data;
}

/*!
 *  @brief  配置状态机的数据域
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 返回 状态机 数据域
 *
 */
static inline void *get_data_entry(FSM *fsm)
{
    return fsm->data;
}

/*!
 *  @brief  让 状态机 步进一次
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 非负数 :代表 所成功执行的状态
 *          -1 : 失败
 */
static inline state run_state_machine_once(FSM *fsm)
{
    state last;    // 执行前状态
    state will_be; // 即将执行的状态
    if (!fsm)
        return UCHAR_MAX;

    // 记录新一次的动作变化
    last = fsm->st.cs;
    will_be = fsm->st.ns;
    // 切换到新状态
    fsm->st.cs = fsm->st.ns;

    // 检测到状态变化时, 通知进入新状态
    if ((last != will_be) && (will_be != FSM_INVALID_STATE) && fsm->when_state_changed_entered != NULL)
    {
        fsm->when_state_changed_entered(fsm, will_be);
    }

    // 跳转到下一个状态(状态 执行 结果 保存在 ret_ptr 中 )
    fsm->ret_ptr = (step_ret)fsm->procedures[fsm->st.cs](fsm);

    // 检测到状态变化时, 通知退出当前状态
    if ((fsm->st.cs != fsm->st.ns) && (fsm->st.cs != FSM_INVALID_STATE) && fsm->when_state_changed_exited != NULL)
    {
        fsm->when_state_changed_exited(fsm, fsm->st.cs);
    }

    // 检测到状态变化时, 通知退出当前状态(相当于enter 和 exit合并的功能)
    if ((fsm->st.cs != fsm->st.ns) && (fsm->st.cs != FSM_INVALID_STATE) && fsm->when_state_changed_from_to != NULL)
    {
        fsm->when_state_changed_from_to(fsm, fsm->st.cs, fsm->st.ns);
    }

    return fsm->st.cs;
}

/*!
 *  @brief  获取步进执行结果
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 最后一次状态过程中return的结果
 */
static inline void *get_step_retval(FSM *fsm)
{
    return (void *)fsm->ret_ptr;
}

/*!
 *  @brief  获取状态机的当前状态
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 当前状态
 */
static inline state get_curr_state(FSM *fsm)
{
    return fsm->st.cs;
}

/*!
 *  @brief  设置状态机默认状态
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @param[in]  st  状态值
 *
 */
static inline void set_default_state(FSM *fsm, state st)
{
    if (!fsm)
        return;
    fsm->st.ds = st;
    fsm->st.ns = st;
}

/*!
 *  @brief  设置状态机的下次状态
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @param[in]  st  状态值
 *
 */
static inline void set_next_state(FSM *fsm, state st)
{
    if (fsm)
        fsm->st.ns = st;
}

/*!
 *  @brief  获取状态机的下次状态
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @return 下一个状态
 */
static inline state get_next_state(FSM *fsm)
{
    return fsm->st.ns;
}

/*!
 *  @brief  将状态机设为默认状态
 *
 *  @param[in]  fsm 状态机实例
 *
 */
static inline void init_state_machine(FSM *p)
{
    set_next_state(p, p->st.ds);
    p->st.cs = UCHAR_MAX; // 执行run之前, 当前状态是未定的
}

/*!
 *  @brief  将状态机设为默认状态, 同时清除错误状态
 *
 *  @param[in]  fsm 状态机实例
 *
 */
static inline void reset_state_machine(FSM *p)
{
    if (!p)
        return;
    clr_fsm_error_flag(p);
    init_state_machine(p);
}

/*!
 *  @brief  判断状态机是否在某个状态
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @param[in]  st  状态值
 *
 *  @return 是/否
 */
static inline state is_curr_state(FSM *fsm, state st)
{
    return fsm->st.cs == st;
}

/*!
 *  @brief  判断状态机是否即将进行某个状态
 *
 *  @param[in]  fsm 状态机实例
 *
 *  @param[in]  st  状态值
 *
 *  @return 是/否
 */
static inline state is_next_state(FSM *fsm, state st)
{
    return fsm->st.ns == st;
}

/*!
 *  @brief  登记状态机状态切换通知:进入状态
 *
 *  @param[in] fsm 状态机实例
 *
 *  @param[in] func_call_back 希望在进入状态时执行的函数
 *
 */
static inline void set_callback_when_state_entered(FSM *fsm, WhenStateChanged func_call_back)
{
    if (fsm)
    {
        fsm->when_state_changed_entered = func_call_back;
    }
}
/*!
 *  @brief  登记状态机状态切换通知:退出状态
 *
 *  @param[in] fsm 状态机实例
 *
 *  @param[in] func_call_back 希望在退出状态时执行的函数
 *
 */
static inline void set_callback_when_state_exited(FSM *fsm, WhenStateChanged func_call_back)
{
    if (fsm)
    {
        fsm->when_state_changed_exited = func_call_back;
    }
}

/*!
 *  @brief  登记状态机状态切换通知:退出状态a, 进入状态b
 *
 *  @param[in] fsm 状态机实例
 *
 *  @param[in] func_call_back 希望在状态切换时执行的函数(同时指出退出的状态和新进入的状态)
 *
 */
static inline void set_callback_when_state_change_from_to(FSM *fsm, WhenStateChangedFromTo func_call_back)
{
    if (fsm)
    {
        fsm->when_state_changed_from_to = func_call_back;
    }
}

#ifdef __cplusplus
}
#endif
