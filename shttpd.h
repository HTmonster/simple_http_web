/******************************************************************************
 * Filename: shttpd.h       
 *
 * %Description: shttpd��ͷ�ļ�   
 * %date_created:  
 * %version:       1 
 * %authors:       Theo_hui
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>   /* for sockaddr_in */
#include <netdb.h>        /* for hostent */ 
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>        /* we want to catch some of these after all */
#include <unistd.h>       /* protos for read, write, close, etc */
#include <dirent.h>       /* for MAXNAMLEN */
#include <limits.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stddef.h>


////////////////////////////////////////�궨��////////////////////////////////

#define URI_MAX     16384       /* ������󳤶�*/

#define big_int_t   long        /* ������ ��������ͷֵ*/

#define DBGPRINT    printf

#define OFFSET(x)   offsetof(struct headers, x)  /*���ڸ�����������ͼ*/
#define DEBUG


/////////////////////////////////////////�ṹ�嶨��///////////////////////////

/*�̵߳�״ֵ̬*/
enum{WORKER_INITED, WORKER_RUNNING,WORKER_DETACHING, WORKER_DETACHED,WORKER_IDEL};



/*******************************************Э�鷽�����************************************/

/* HTTPЭ��ķ��� */
typedef enum SHTTPD_METHOD_TYPE{
    METHOD_GET,         /*GET     ����*/
    METHOD_POST,        /*POST   ����*/
    METHOD_PUT,         /*PUT     ����*/
    METHOD_DELETE,  /*DELETE����*/
    METHOD_HEAD,        /*HEAD   ����*/
    METHOD_CGI,     /**CGI����*/
    METHOD_NOTSUPPORT
}SHTTPD_METHOD_TYPE;


typedef struct shttpd_method{
    SHTTPD_METHOD_TYPE type;
    int name_index;
    
}shttpd_method;


/*******************************************���û���������**********************/

/* ����������*/ 
typedef struct vec 
{
    char    *ptr;                  // �ַ���
    int         len;
    SHTTPD_METHOD_TYPE type;       // �ַ�����ʾ������
}vec;

/********************************************�����ļ����**********************/
// �����ļ��Ľṹ
struct conf_opts{
    char CGIRoot[128];      /*CGI��·��*/
    char DefaultFile[128];      /*Ĭ���ļ�����*/
    char DocumentRoot[128]; /*���ļ�·��*/
    char ConfigFile[128];       /*�����ļ�·��������*/
    int ListenPort;         /*�����˿�*/
    int MaxClient;          /*���ͻ�������*/
    int TimeOut;                /*��ʱʱ��*/
    int InitClient;             /*��ʼ���߳�����*/
};

/*******************************************����ͷ���************************/

enum {HDR_DATE, HDR_INT, HDR_STRING};   /* HTTP ͷ������*/

/*���Ա���������͵�ֵ*/
union variant {
    char        *v_str;
    int     v_int;
    big_int_t   v_big_int;
    time_t      v_time;
    void        (*v_func)(void);
    void        *v_void;
    struct vec  v_vec;
};


/*����ͷ*/
struct http_header {
    int     len;        /*ͷ�����ֳ���*/
    int     type;       /*ͷ������*/
    size_t      offset; /*ֵλ��*/
    char    *name;      /*ͷ������*/
};


/*���ڽ�������ͷ*/
struct headers {
    union variant   cl;     /* Content-Length:  ���ݳ���    */
    union variant   ct;     /* Content-Type:    ��������    */
    union variant   connection; /* Connection:  ����״̬        */
    union variant   ims;        /* If-Modified-Since:   �����޸�ʱ�� */
    union variant   user;       /* Remote user name     �û����� */
    union variant   auth;       /* Authorization    Ȩ��  */
    union variant   useragent;  /* User-Agent:      �û�����    */
    union variant   referer;    /* Referer: �ο�      */
    union variant   cookie;     /* Cookie:      cookie  */
    union variant   location;   /* Location:    λ��      */
    union variant   range;      /* Range:       ��Χ  */
    union variant   status;     /* Status:      ״ֵ̬ */
    union variant   transenc;   /* Transfer-Encoding:   ��������    */
};


/*******************************************CGI*********************************/
struct cgi{
    int iscgi;
    struct vec bin;
    struct vec para;    
};

/********************************************��������Ӧ**************************/
struct worker_conn ;

/*����ṹ*/
struct conn_request{
    struct vec  req;        /*��������*/
    char *head;         /*����ͷ��\0'��β*/
    char *uri;          /*����URI,'\0'��β*/
    char rpath[URI_MAX];    /*�����ļ�����ʵ��ַ\0'��β*/

    int     method;         /*��������*/

    /*HTTP�İ汾��Ϣ*/
    unsigned long major;    /*���汾*/
    unsigned long minor;    /*���汾*/

    struct headers ch;  /*ͷ���ṹ*/

    struct worker_conn *conn;   /*���ӽṹָ��*/
    int err;                      // �������
};

/* ��Ӧ�ṹ */
struct conn_response{
    struct vec  res;        /*��Ӧ����*/
    time_t  birth_time; /*����ʱ��*/
    time_t  expire_time;/*��ʱʱ��*/

    int     status;     /*��Ӧ״ֵ̬*/
    int     cl;         /*��Ӧ���ݳ���*/

    int         fd;         /*�����ļ�������*/
    struct stat fsate;      /*�����ļ�״̬*/

    struct worker_conn *conn;   /*���ӽṹָ��*/  
};


/**************************************worker���**************************/

struct worker_ctl;

// �������̵߳����״̬��
struct worker_opts{
    pthread_t th;           /*�̵߳�ID��*/
    int flags;              /*�߳�״̬*/
    pthread_mutex_t mutex;/*�߳����񻥳�*/

    struct worker_ctl *work;    /*���̵߳��ܿؽṹ*/
};

//  ά���ͻ����������Ӧ����
struct worker_conn 
{
#define K 1024
    char        dreq[16*K]; /*���󻺳��������յ��Ŀͻ�����������*/
    char        dres[16*K]; /*��Ӧ�����������͸��ͻ��˵����� */

    int     cs;         /*�ͻ����׽����ļ�������*/
    int     to;         /*�ͻ�������Ӧʱ�䳬ʱ�˳�ʱ�䣬��ʱ��Ӧʱ��*/

    struct conn_response con_res;         //  ά����Ӧ�ṹ
    struct conn_request con_req;          // ά���ͷ��˵�����

    struct worker_ctl *work;    /*���̵߳��ܿؽṹ*/
};

//worker�Ŀ�����Ϣ
struct worker_ctl{
    struct worker_opts opts;               // ��ʾ�߳�״̬
    struct worker_conn conn;               // ��ʾ�ͻ��������״̬��ֵ
};


/****************************************ý������*********************************/
//ý������
struct mine_type{
    char    *extension;           // ��չ��
    int            type;          // ����
    int         ext_len;          // ��չ������
    char    *mime_type;           //��������
};