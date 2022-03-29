/******************************************************************************
 * Author: Roy Honigman                                                       *
 * Reviewer: HAIM                                                             *
 * Date: 26.1.21                                                              *
 * Description: Implementation of watchdog                                    *
 *                                                                            * 
 * Infinity Labs OL113                                                        *
 ******************************************************************************/

#define _GNU_SOURCE

#include <pthread.h> /* create(), pthread_sigmask() */
#include <stdio.h>   /* printf() */
#include <signal.h>  /* sigaction, kill, SIGUSR1, SIGUSR2, sigemptyset(), 
                                            sigaddset(), sigprocmask()  */
#include <unistd.h>  /* fork, STDOUT_FILENO ,write */
#include <assert.h>  /* assert */
#include <stdlib.h>  /* setenv(), getenv() */
#include <string.h>  /*strcpy() strcmp()*/

#include "wd.h"                /* WDStart */
#include "st_scheduler.h"      /* scheduler funtions */
#include "process_semaphore.h" /* semaphore functions */

#define MAX_APP_NAME_LEN (100)
#define TIME_OUT (5)
#define SEM_MAX_NAME (100)
#define SEM_IS_READY "SEM_IS_READY"
#define SEM_IS_WD_EXIST "SEM_IS_WD"
#define SEM_INIT_VAL (0)

/********************************** enums ************************************/
typedef enum exit_status
{
    SUCCESS,
    SYSTEM_ERROR
} exit_status_t;

typedef enum wait_or_post
{
    WAIT,
    POST
} wait_or_post_t;

/***************************** global variables ******************************/

int g_i_am_app = 1;
static pid_t g_pid_partner = 0;
static wait_or_post_t g_sem_wait_or_post = WAIT;
static size_t g_sigusr1_counter = 0;
static sem_id_t *sem_is_ready = NULL;
static sem_id_t *sem_is_wd_exist = NULL;
static int g_keep_running_task1 = 1;
static int g_keep_running_task2 = 1;
static int g_app_got_sigusr2_from_wd = 0;
static pthread_t g_thread = {0};
static sigset_t g_sig_set = {0};

/*********************** inner functions declarations ************************/
static void SIG1Handler();
static void SIG2Handler();
static exit_status_t InitResources(st_scheduler_t **scheduler_p, char **argv);
static exit_status_t InitHandlers(void);
static exit_status_t InitSemaphors(void);
static exit_status_t GenerateSemName(char *dest);
static exit_status_t InitSpecificSemaphore(char *env_sem_name);
static exit_status_t InitScheduler(st_scheduler_t **scheduler_p, char **argv);
static int NeedToCreateWD(void);
static int CreatePartnerProcess(char **argv);
static void CleanUpResources(st_scheduler_t *scheduler);
static void *AppWD(void *scheduler);
static void *WD(void *scheduler);
static int Task1(void *param);
static int Task2(void *param);
static void StopScheduler(void);
static int IsTimeOut(size_t time_out);
static void CpyArgs(char **dest, char *const *src);
static void BlockSIGUSR(void);
static void UnBlockSIGUSR(void);

/********************************************************************************
 *                                 Main Functions                               *
 ********************************************************************************/

int WDStart(char **argv)
{
    st_scheduler_t *scheduler = NULL;
    exit_status_t status = SUCCESS;

    BlockSIGUSR(); /* blocks SIGUSR signal's catch from user application */

    if ((status = InitResources(&scheduler, argv)))
    {
        return status;
    }

    printf("am_i_app = %d\n", g_i_am_app);

    if (g_i_am_app)
    {
        if (NeedToCreateWD())
        {
            g_sem_wait_or_post = WAIT;
            status = CreatePartnerProcess(argv);
            if (status)
            {
                CleanUpResources(scheduler);
                return status;
            }
        }
        else
        {
            SemWait(sem_is_wd_exist);
            g_sem_wait_or_post = POST;
            g_pid_partner = getppid();
        }

        if (pthread_create(&g_thread, NULL, AppWD, (void *)scheduler))
        {
            CleanUpResources(scheduler);
            return SYSTEM_ERROR;
        }

        pthread_detach(g_thread);
    }
    else
    {
        g_pid_partner = getppid();
        WD((void *)scheduler);
    }

    return status;
}

void WDStop(void)
{
    size_t time_out = time(NULL) + TIME_OUT;

    g_keep_running_task2 = 0;

    while (!IsTimeOut(time_out) && !g_app_got_sigusr2_from_wd)
    {
        kill(g_pid_partner, SIGUSR2);
        sleep(1);
    }

    g_keep_running_task1 = 0;
}

/********************************************************************************
 *                                Inner Functions                               *
 ********************************************************************************/

/*************************** Init functions ******************************/

static exit_status_t InitResources(st_scheduler_t **scheduler_p, char **argv)
{
    if (InitHandlers())
    {
        return SYSTEM_ERROR;
    }

    if (InitSemaphors())
    {
        return SYSTEM_ERROR;
    }

    if (InitScheduler(scheduler_p, argv))
    {
        SemDestroy(sem_is_ready);
        SemDestroy(sem_is_wd_exist);

        return SYSTEM_ERROR;
    }

    return SUCCESS;
}

static exit_status_t InitHandlers(void)
{
    struct sigaction sig1 = {0};
    struct sigaction sig2 = {0};

    sig1.sa_handler = &SIG1Handler;
    sig2.sa_handler = &SIG2Handler;

    if (-1 == sigaction(SIGUSR1, &sig1, NULL) ||
        -1 == sigaction(SIGUSR2, &sig2, NULL))
    {
        return SYSTEM_ERROR;
    }

    return SUCCESS;
}

static exit_status_t InitSemaphors(void)
{
    if (InitSpecificSemaphore(SEM_IS_READY))
    {
        return SYSTEM_ERROR;
    }

    if (InitSpecificSemaphore(SEM_IS_WD_EXIST))
    {
        SemDestroy(sem_is_wd_exist);
        return SYSTEM_ERROR;
    }

    return SUCCESS;
}

static exit_status_t InitSpecificSemaphore(char *env_sem_name)
{
    char *env_sem = getenv(env_sem_name);
    sem_id_t *check_valid = NULL;
    char sem_name[SEM_MAX_NAME] = {0};

    if (!env_sem)
    {
        if (GenerateSemName(sem_name))
        {
            return SYSTEM_ERROR;
        }

        if (-1 == setenv(env_sem_name, sem_name, 0))
        {
            return SYSTEM_ERROR;
        }
    }
    else
    {
        strcpy(sem_name, env_sem);
    }

    if (!strcmp(SEM_IS_WD_EXIST, env_sem_name))
    {
        sem_is_wd_exist = SemCreate(sem_name, SEM_INIT_VAL, POSIX);
        check_valid = sem_is_wd_exist;
    }
    else
    {
        sem_is_ready = SemCreate(sem_name, SEM_INIT_VAL, POSIX);
        check_valid = sem_is_ready;
    }

    if (!check_valid)
    {
        return SYSTEM_ERROR;
    }

    return SUCCESS;
}

static size_t CreateNewUid(void)
{
    unique_id_t uid = UidCreate();

    return uid.time + uid.pid + uid.count;
}

static exit_status_t GenerateSemName(char *dest)
{
    size_t uid = CreateNewUid();

    if (0 > sprintf(dest, "/%lu", uid))
    {
        return SYSTEM_ERROR;
    }

    return SUCCESS;
}

static exit_status_t InitScheduler(st_scheduler_t **scheduler_p, char **argv)
{
    *scheduler_p = STSchedulerCreate();
    if (!*scheduler_p)
    {
        return SYSTEM_ERROR;
    }

    if (UidIsMatch(BadUID, STSchedulerAdd(*scheduler_p, Task1, 1, NULL)))
    {
        STSchedulerDestroy(*scheduler_p);
        return SYSTEM_ERROR;
    }

    if (UidIsMatch(BadUID, STSchedulerAdd(*scheduler_p, Task2, 5, argv)))
    {
        STSchedulerDestroy(*scheduler_p);
        return SYSTEM_ERROR;
    }

    return SUCCESS;
}

static void CleanUpResources(st_scheduler_t *scheduler)
{
    SemDestroy(sem_is_ready);
    SemDestroy(sem_is_wd_exist);
    STSchedulerDestroy(scheduler);
}

/*************************** Create WD and Restart app ******************************/

static int CreatePartnerProcess(char **argv)
{
    pid_t child_pid = fork();

    if (-1 == child_pid)
    {
        printf("Can't create child process\n");
        return SYSTEM_ERROR;
    }

    if (0 == child_pid)
    {
        if (g_i_am_app)
        {
            char *new_argv[MAX_APP_NAME_LEN] = {NULL};

            new_argv[0] = "./wd_bg_p";
            CpyArgs(&new_argv[1], argv);

            execvp(new_argv[0], new_argv);
        }
        else
        {
            execvp(argv[1], &argv[1]);
        }

        return SYSTEM_ERROR;
    }
    else
    {
        g_pid_partner = child_pid;
    }

    return SUCCESS;
}

/*************************** Run Schedulers ******************************/

static void *AppWD(void *scheduler)
{
    printf("Run AppWD, my pid is: %d, my partner (WD) pid is: %d\n", getpid(), g_pid_partner);

    UnBlockSIGUSR();

    if (POST == g_sem_wait_or_post)
    {
        SemPost(sem_is_ready);
    }
    else
    {
        SemWait(sem_is_ready);
    }

    STSchedulerRun((st_scheduler_t *)scheduler);

    CleanUpResources((st_scheduler_t *)scheduler);

    pthread_exit(NULL);
}

static void *WD(void *scheduler)
{
    printf("Run WD, my pid is: %d, my partner (AppWD) pid is: %d\n", getpid(), g_pid_partner);

    UnBlockSIGUSR();

    SemPost(sem_is_ready);
    STSchedulerRun((st_scheduler_t *)scheduler);

    CleanUpResources((st_scheduler_t *)scheduler);

    return NULL;
}

/*************************** Signal Handlers ******************************/

static void SIG1Handler()
{
    g_sigusr1_counter++;
    fprintf(stderr, "g_sigusr1_counter = %ld\n", g_sigusr1_counter);
}

static void SIG2Handler()
{
    if (g_i_am_app)
    {
        fprintf(stderr, "STOP app got SIGUSR2\n");
        g_app_got_sigusr2_from_wd = 1;
    }
    else
    {
        fprintf(stderr, "STOP wd got SIGUSR2\n");
        StopScheduler();
        kill(g_pid_partner, SIGUSR2);
    }
}

/******************************** Tasks ***********************************/

static int Task1(void *param)
{
    (void)param;

    if (g_keep_running_task1)
    {
        kill(g_pid_partner, SIGUSR1);

        return 0;
    }

    return 1;
}

static int Task2(void *argv)
{
    char **argv_ch = (char **)argv;
    if (g_keep_running_task2)
    {
        if (g_sigusr1_counter > 0)
        {
            g_sigusr1_counter = 0;
            fprintf(stderr, "Task2 - g_sigusr1_counter = %ld\n", g_sigusr1_counter);
        }
        else
        {
            if (!g_i_am_app)
            {
                SemPost(sem_is_wd_exist);
            }

            if (CreatePartnerProcess(argv_ch))
            {
                return 1;
            }

            SemWait(sem_is_ready);
        }

        return 0;
    }

    return 1;
}

/************************ Additional minor functions ****************************/

static void StopScheduler(void)
{
    g_keep_running_task1 = 0;
    g_keep_running_task2 = 0;
}

static int IsTimeOut(size_t time_out)
{
    return (size_t)time(NULL) >= time_out;
}

static int NeedToCreateWD(void)
{
    return !SemGetVal(sem_is_wd_exist);
}

static void CpyArgs(char **dest, char *const *src)
{
    while (*src)
    {
        *dest = *src;

        dest++;
        src++;
    }
}

static void BlockSIGUSR(void)
{
    sigemptyset(&g_sig_set);
    sigaddset(&g_sig_set, SIGUSR1);
    sigaddset(&g_sig_set, SIGUSR2);

    sigprocmask(SIG_BLOCK, &g_sig_set, NULL);
}

static void UnBlockSIGUSR(void)
{
    pthread_sigmask(SIG_UNBLOCK, &g_sig_set, NULL);
}
