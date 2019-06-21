/******************************************************************************
 * Filename: shttpd_work.c      
 *
 * %Description:   作业调度    
 * %date_created:  2019.6.19 
 * %version:       1 
 * %authors:       Theo_hui
*******************************************************************************/

# include "shttpd.h"
# define DEBUG

////////////////////////////////////宏定义////////////////////////////////////
#define STATUS_RUNNING 1
#define STATSU_STOP 0

////////////////////////////////////全局变量///////////////////////////////////

static int SCHEDULESTATUS = STATUS_RUNNING;


static int workersnum = 0;/*工作线程的数量*/
static struct worker_ctl *wctls = NULL;/*线程选项*/
extern struct conf_opts conf_para;
pthread_mutex_t thread_init = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////函数声明//////////////////////////////////

static int WORKER_ISSTATUS(int status);
static void Worker_Init();
static int Worker_Add(int i);
static void Worker_Delete(int i);
static void Worker_Destory();
///////////////////////////////////函数实现///////////////////////////////////


/**
 * 线程的状态
 * @param  status 状态值
 * @return        线程的序号
 */
static int WORKER_ISSTATUS(int status)
{
    int i = 0;
    for(i = 0; i<conf_para.MaxClient;i++)
    {
        if(wctls[i].opts.flags == status)
            return i;
    }

    return -1;
}


/**
 * worker接收到任务之后，开始处理任务
 * @param wctl 线程控制信息
 */
static void do_work(struct worker_ctl *wctl)
{

#ifdef DEBUG
    printf("正在处理工作 do_work\n");
#endif

    struct timeval tv;      /*超时时间*/
    fd_set rfds;            /*读文件集*/
    int fd = wctl->conn.cs;/*客户端的套接字描述符*/
    struct vec *req = &wctl->conn.con_req.req;/*请求缓冲区向量*/
    
    int retval = 1;

    for(;retval > 0;)
    {
        /*清读文件集,将客户端连接
            描述符放入读文件集*/
        FD_ZERO(&rfds); 
        FD_SET(fd, &rfds);  

        /*设置超时*/
        tv.tv_sec = 300;//conf_para.TimeOut;
        tv.tv_usec = 0;

        /*超时读数据*/
        retval = select(fd + 1, &rfds, NULL, NULL, &tv);
        switch(retval)
        {
            case -1:/*错误*/
                close(fd);
                break;
            case 0:/*超时*/
                close(fd);
                break;
            default:
                printf("select retval:%d\n",retval);

                if(FD_ISSET(fd, &rfds))/*检测文件*/
                {
                    memset(wctl->conn.dreq, 0, sizeof(wctl->conn.dreq));
                    /*读取客户端数据*/
                    req->len = 
                        read(wctl->conn.cs, wctl->conn.dreq, sizeof(wctl->conn.dreq));
                    req->ptr = wctl->conn.dreq;
# ifdef DEBUG
                    printf("读取到请求的数据 %d bytes,'%s'\n",req->len,req->ptr);
# endif

                    /*有数据*/
                    if(req->len > 0)
                    {   
                        /*分析客户端的数据,主要是http的解析等*/
                        wctl->conn.con_req.err =Request_Parse(wctl);

                        /*响应客户端请求*/
                        Request_Handle(wctl);               
                    }
                    /*没有数据则退出*/
                    else
                    {
                        close(fd);
                        retval = -1;
                    }
                }
        }
    }
}




/**
 * 线程处理函数
 * @param  arg 参数
 * @return     [description]
 */
static void *worker(void *arg){

#ifdef DEBUG
    printf("进入线程处理函数 worker\n");
#endif

    struct worker_ctl *ctl = (struct worker_ctl *)arg; //控制信息
    struct worker_opts *self_opts = &ctl->opts;        //选项结构

    pthread_mutex_unlock(&thread_init); //解锁互斥

    /*初始化线程为空闲，等待任务*/
    self_opts->flags = WORKER_IDEL;

    /*如果主控线程没有让此线程退出，则循环处理任务*/
    for(;self_opts->flags != WORKER_DETACHING;)
    {
        /*查看是否有任务分配*/
        int err = pthread_mutex_trylock(&self_opts->mutex);
        if(err)
        {

#ifdef DEBUG
    printf("[waiting......]\n");
#endif
      
            sleep(1);
            continue;
        }
        else
        {
            /*有任务，do it*/
#ifdef DEBUG
    printf("接收到任务，处理任务\n");
#endif
            self_opts->flags = WORKER_RUNNING; //设置为正在运行
            do_work(ctl);
            close(ctl->conn.cs);
            ctl->conn.cs = -1;
            if(self_opts->flags == WORKER_DETACHING)
                break;
            else
                self_opts->flags = WORKER_IDEL;
        }
    }

    /*主控发送退出命令*/
    /*设置状态为已卸载*/
    self_opts->flags = WORKER_DETACHED;
    workersnum--;/*工作线程-1*/

    return NULL;
}

/**
 * 线程池初始化
 */
static void Worker_Init(){

#ifdef DEBUG
    printf("进入线程初始化\n");
#endif
    int i = 0;
    /*初始化总控参数*/
    wctls = (struct worker_ctl*)malloc(sizeof(struct worker_ctl)*conf_para.MaxClient);
    memset(wctls, 0, sizeof(*wctls)*conf_para.MaxClient);/*清零*/

    /*初始化一些参数:worker_ctl那些*/
    for(i = 0; i<conf_para.MaxClient;i++)
    {
        /*opts&conn结构与worker_ctl结构形成回指针*/
        wctls[i].opts.work = &wctls[i];
        wctls[i].conn.work = &wctls[i];

        /*opts结构部分的初始化*/
        wctls[i].opts.flags = WORKER_DETACHED;
        //wctls[i].opts.mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&wctls[i].opts.mutex,NULL);
        pthread_mutex_lock(&wctls[i].opts.mutex);

        /*conn部分的初始化*/
        /*con_req&con_res与conn结构形成回指*/
        wctls[i].conn.con_req.conn = &wctls[i].conn;
        wctls[i].conn.con_res.conn = &wctls[i].conn;
        wctls[i].conn.cs = -1;/*客户端socket连接为空*/
        /*con_req部分初始化*/
        wctls[i].conn.con_req.req.ptr = wctls[i].conn.dreq;
        wctls[i].conn.con_req.head = wctls[i].conn.dreq;
        wctls[i].conn.con_req.uri = wctls[i].conn.dreq;
        /*con_res部分初始化*/
        wctls[i].conn.con_res.fd = -1; 
        wctls[i].conn.con_res.res.ptr = wctls[i].conn.dres;
    }
    
     // 初始化完后，建立多个业务线程，个数由配置文件定
    for(i = 0; i<conf_para.InitClient;i++)
    {
        /*增加规定个数工作线程*/
        Worker_Add(i);
    }

#ifdef DEBUG
    printf("调出线程初始化\n");
#endif

}

/**
 * 减少线程
 * @param i [线程的ID]
 */
 static void Worker_Delete(int i)
 {
# ifdef DEBUG
    printf("正在卸载进程%d\n",i);
# endif

    wctls[i].opts.flags = WORKER_DETACHING;/*线程状态改为正在卸载*/
 }

/*******
 * 增加线程
 * @param  i 序号索引ID
 * @return   0
 */
static int Worker_Add(int i){

#ifdef DEBUG
    printf("正在增加线程 %d\n",i);
#endif

    pthread_t th;
    int err = -1;

    /*如果正在运行 判断出错 退出*/
    if( wctls[i].opts.flags == WORKER_RUNNING)
        return 1;

    /*正式创建的设置新线程*/
    pthread_mutex_lock(&thread_init);
    wctls[i].opts.flags = WORKER_INITED;/*状态为已初始化*/
    err = pthread_create(&th, NULL, worker, (void*)&wctls[i]);/*建立线程*/
    pthread_mutex_unlock(&thread_init);

    /*更新线程选项*/  
    wctls[i].opts.th = th;/*线程ID*/
    workersnum++;/*线程数量增加1*/

#ifdef DEBUG
    printf("线程%d增加成功\n",i);
#endif

    return 0;
}

/*****
 * 销毁线程
 */
static void Worker_Destory()
{

    int i = 0;
    int clean = 0;
    
    for(i=0;i<conf_para.MaxClient;i++)
    {
#ifdef DEBUG
        printf("thread %d,status %d\n",i,wctls[i].opts.flags );
#endif
        if(wctls[i].opts.flags != WORKER_DETACHED)
            Worker_Delete(i);
    }

    while(!clean)
    {
        clean = 1;
        for(i = 0; i<conf_para.MaxClient;i++){

#ifdef DEBUG
            printf("thread %d,status %d\n",i,wctls[i].opts.flags );
#endif
            if(wctls[i].opts.flags==WORKER_RUNNING 
                || wctls[i].opts.flags == WORKER_DETACHING)
                clean = 0;
        }
        if(!clean)
            sleep(1);
    }
#ifdef DEBUG
    printf("线程销毁完毕\n");
#endif
}


/**********************************************************************************
 * 主调度过程 当有客户端连接
 * @param  ss [description]
 * @return    [description]
 */
int Worker_ScheduleRun(int ss){

#ifdef DEBUG
    printf("进入工作调度\n");
#endif

    struct sockaddr_in client;         /*客户端地址*/
    socklen_t len = sizeof(client);

    Worker_Init();                     /*线程初始化*/

    int i=0;

    /* 当调度状态SCHEDULESTATUS为STATUS_RUNNING时候，select()等待客户端连接。
     当有客户端连接到的时候，查找空闲的业务进程，
     如果没有则增加一个业务进程，然后将此任务分配给找到的空闲业务进程
    */
    for (;SCHEDULESTATUS== STATUS_RUNNING;)
    {
        struct timeval tv;      /*超时时间*/
        fd_set rfds;            /*读文件集*/

        int retval = -1;

        /*清读文件集,将客户端连接
            描述符放入读文件集*/
        FD_ZERO(&rfds); 
        FD_SET(ss, &rfds);

        /*设置超时*/
        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        /*超时读数据*/
        retval = select(ss + 1, &rfds, NULL, NULL, &tv);
        switch(retval)
        {
            case -1:/*错误*/
            case 0:/*超时*/
                continue;
                break;
            default:
                if(FD_ISSET(ss, &rfds))/*检测文件*/
                {
                    int sc = accept(ss, (struct sockaddr*)&client, &len);
                    printf("client comming\n");
                    i = WORKER_ISSTATUS(WORKER_IDEL); //查找空闲的状态线程

                    /*未找到*/
                    if(i == -1)
                    {
                        i =  WORKER_ISSTATUS(WORKER_DETACHED); //找分离的线程
                        if(i != -1)
#ifdef DEBUG
                            printf("添加一个新线程\n");
#endif
                            Worker_Add( i);
                    }
                    if(i != -1)
                    {
                        wctls[i].conn.cs = sc;
                        pthread_mutex_unlock(&wctls[i].opts.mutex);
                    }
#ifdef DEBUG
                    printf("客户端挂接到%d\n",i);
#endif             
                }
        }       
    }
    
#ifdef DEBUG
    printf("退出工作调度\n");
#endif
    return 0;
}

/***********************************************************************************************
 * 停止调度过程
 * @return [description]
 */
int Worker_ScheduleStop()
{

#ifdef DEBUG
    printf("正在停止线程......\n");
#endif

    SCHEDULESTATUS = STATSU_STOP;
    int i =0;

    Worker_Destory();
    int allfired = 0;
    for(;!allfired;)
    {
        allfired = 1;
        for(i = 0; i<conf_para.MaxClient;i++)
        {
            int flags = wctls[i].opts.flags;
            if(flags == WORKER_DETACHING || flags == WORKER_IDEL)
                allfired = 0;
        }
    }
    
    pthread_mutex_destroy(&thread_init);
    for(i = 0; i<conf_para.MaxClient;i++)
        pthread_mutex_destroy(&wctls[i].opts.mutex);
    free(wctls);
    
#ifdef DEBUG
    printf("线程停止完毕\n");
#endif
    return 0;
}
