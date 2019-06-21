/******************************************************************************
 * Filename: shttpd.c      
 *
 * %Description:  shttp服务器的主文件    
 * %date_created:  2019.6.19 
 * %version:       1 
 * %authors:       Theo_hui
*******************************************************************************/

# include "shttpd.h" 


///////////////////////////////////////全局变量//////////////////////////////////

/*命令行的解析结构*/ 
struct conf_opts conf_para={
	/*CGIRoot*/		"/usr/local/var/www/cgi-bin/",
	/*DefaultFile*/	"index.html",
	/*DocumentRoot*/"/home/www",
	/*ConfigFile*/	"/etc/sHTTPd.conf",
	/*ListenPort*/	80,
	/*MaxClient*/	4,	
	/*TimeOut*/		3,
	/*InitClient*/		2
};

// 各种方法的请求头类型
struct vec _shttpd_methods[] = {
	{"GET",		3, METHOD_GET},
	{"POST",	4, METHOD_POST},
	{"PUT",		3, METHOD_PUT},
	{"DELETE",	6, METHOD_DELETE},
	{"HEAD",	4, METHOD_HEAD},
	{NULL,		0}                // 结尾
};


//////////////////////////////////////函数区////////////////////////////////////


/**
 *用户ctrl-c 之后进行处理退出 
*/

static int signal_int(int num){ 
	
	Worker_ScheduleStop();
	
	return;
} 

/**
 * SIGPIPE信号截取函数
 */
static void signal_pipe(int num)
{
	return;
}

/******************************************************************************/


/**
 * 套接字初始化
 * @return 套接字描述符
 */
int do_listen(){
	struct sockaddr_in server;
	int ss = -1;
	int err = -1;
	int reuse = 1;
	int ret = -1;

	/* 初始化服务器地址 */
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;                     /*IPV4*/
	server.sin_addr.s_addr=htonl(INADDR_ANY);        /*任意地址*/
	server.sin_port = htons(conf_para.ListenPort);   /*端口*/

	/* 信号截取函数 */
	signal(SIGINT,  signal_int);
	signal(SIGPIPE, signal_pipe);

	/* 生成套接字文件描述符 */
	ss = socket (AF_INET, SOCK_STREAM, 0);
	if (ss == -1)
	{
		printf("socket() error\n");
		ret = -1;
		goto EXITshttpd_listen;
	}

	/* 设置套接字地址和端口复用 */
	err = setsockopt (ss, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (err == -1)
	{
		printf("setsockopt SO_REUSEADDR failed\n");
	}

	/* 绑定IP和套接字描述符 */
	err = bind (ss, (struct sockaddr*)  &server, sizeof(server));
	if (err == -1)
	{
		printf("bind() error\n");
		ret = -2;
		goto EXITshttpd_listen;
	}

	/* 设置服务器侦听队列长度 */
	err = listen(ss, conf_para.MaxClient*2);
	if (err)
	{
		printf ("listen() error\n");
		ret = -3;
		goto EXITshttpd_listen;
	}

	ret = ss;
EXITshttpd_listen:
	return ret;
}


////////////////////////////////////////主函数//////////////////////////////

/**
 * 主函数  所有功能的入口
 * @param  argc [description]
 * @param  argv [description]
 * @return      [description]
 */
int main(int argc,char *argv[]){
	
	signal(SIGINT,signal_int);	/*挂接信号 用户ctrl-c之后进行退出*/
	
	Parameter_Init(argc,argv);  /*参数初始化*/

	int s=do_listen();          /*套接字初始化*/

	Worker_ScheduleRun(s);      /* 任务调度*/

	return 0;
}