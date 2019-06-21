/******************************************************************************
 * Filename:   shttpd_parameter.c    
 *
 * %Description:  参数解析部分    
 * %date_created:  2019.6.20 
 * %version:       1 
 * %authors:       Theo_hui
*******************************************************************************/

# include "shttpd.h"

///////////////////////////////////全局定义/////////////////////////////////

#define LINELENGTH 256  /*配置文件一行读取长度*/

///////////////////////////////////全局变量/////////////////////////////////


static char *l_opt_arg;             /*解析的参数*/
extern struct conf_opts conf_para;  /*解析的命令参数 外置变量*/


static char*shortopts="c:d:f:ho:l:m:t:";; /*短选项 getopt_long使用*/

static struct option longopts[] = {
    {"CGIRoot",         required_argument, NULL, 'c'},
    {"ConfigFile",      required_argument, NULL, 'f'},  
    {"DefaultFile",     required_argument, NULL, 'd'},  
    {"DocumentRoot",    required_argument, NULL, 'o'},
    {"ListenPort",      required_argument, NULL, 'l'},
    {"MaxClient",       required_argument, NULL, 'm'},  
    {"TimeOut",     required_argument, NULL, 't'},
    {"Help",            no_argument, NULL, 'h'},
    {0, 0, 0, 0},
};                                        /*长选项 getopt_long使用*/





////////////////////////////////////函数部分////////////////////////////////


/**
 * 显示用户输入参数信息
 */
static void display_usage(void)
{
  printf("sHTTPD -l number  -m number      -o path    -c  path   -d  filename   -t seconds  -o filename\n");
  printf("sHTTPD --ListenPort number\n");
  printf("       --MaxClient number\n");
  printf("       --DocumentRoot) path\n");
  printf("       --DefaultFile) filename\n");
  printf("       --CGIRoot path \n");
  printf("       --DefaultFile filename\n");
  printf("       --TimeOut seconds\n");
  printf("       --ConfigFile filename\n");
}

/**
 * 显示已经解析完成之后的参数
 */

static void display_para()
{
  printf("sHTTPD ListenPort: %d\n",conf_para.ListenPort);
  printf("       MaxClient: %d\n", conf_para.MaxClient);
  printf("       DocumentRoot: %s\n",conf_para.DocumentRoot);
  printf("       DefaultFile:%s\n",conf_para.DefaultFile);
  printf("       CGIRoot:%s \n",conf_para.CGIRoot);
  printf("       DefaultFile:%s\n",conf_para.DefaultFile);
  printf("       TimeOut:%d\n",conf_para.TimeOut);
  printf("       ConfigFile:%s\n",conf_para.ConfigFile);
}

/**
 * 读取配置文件的一行参数 用于下面的函数
 */

static int conf_readline(int fd, char *buff, int len)
{
  int n = -1;
  int i = 0;
  int begin = 0;

  /*清缓冲区*/
  memset(buff, 0, len);
  for(i =0; i<len;begin?i++:i)/*当开头部分不为'\r'或者'\n'时i计数*/
  {
    n = read(fd, buff+i, 1);/*读一个字符*/
    if(n == 0)/*文件末尾*/{
      *(buff+i) = '\0';
      break;
    }else if(*(buff+i) == '\r' ||*(buff+i) == '\n'){/*回车换行*/
      if(begin){/*为一行*/
        *(buff+i) = '\0'; 
        break;
      }
    }else{
      begin = 1;
    }
  }

  return i;
}




/**
 * 命令行参数的解析
 */

static int Parameter_ParseCmd(int argc,char* argv[]){
	
    printf("正在解析参数\n");

    int c;  //获得参数
    int len;//参数的长度
    int value;

    /*遍历获得参数*/
    while((c=getopt_long(argc,argv,shortopts,longopts,NULL))!=-1){
      switch(c){
        case 'c':
          l_opt_arg=optarg;
          if(l_opt_arg && l_opt_arg[0]!=':'){
              len = strlen(l_opt_arg);
              memcpy(conf_para.CGIRoot, l_opt_arg, len +1);
          }
          //printf("解析到CGI根目录: %s\n",conf_para.CGIRoot);
          break;
        case 'd':   /*目录下的默认文件名称*/
          l_opt_arg = optarg;
          if(l_opt_arg && l_opt_arg[0]!=':'){
            len = strlen(l_opt_arg);
            memcpy(conf_para.DefaultFile, l_opt_arg, len +1);
          }
          
          break;
        case 'f':   /*配置文件名称和路径*/
          l_opt_arg = optarg;
          if(l_opt_arg && l_opt_arg[0]!=':'){
            len = strlen(l_opt_arg);
            memcpy(conf_para.ConfigFile, l_opt_arg, len +1);
          }
          
          break;
        case 'o': /*网站的根文件路径*/
          l_opt_arg = optarg;
          if(l_opt_arg && l_opt_arg[0]!=':'){
            len = strlen(l_opt_arg);
            memcpy(conf_para.DocumentRoot, l_opt_arg, len +1);
          }
          
          break;
        case 'l':   /*侦听端口*/
          l_opt_arg = optarg;
          if(l_opt_arg && l_opt_arg[0]!=':'){
            len = strlen(l_opt_arg);
            value = strtol(l_opt_arg, NULL, 10);
            if(value != LONG_MAX && value != LONG_MIN)
              conf_para.ListenPort = value;
          }
          
          break;
        case 'm': /*最大客户端数量*/
          l_opt_arg = optarg;
          if(l_opt_arg && l_opt_arg[0]!=':'){
            len = strlen(l_opt_arg);
            value = strtol(l_opt_arg, NULL, 10);
            if(value != LONG_MAX && value != LONG_MIN)
              conf_para.MaxClient= value;
          }       
          
          break;
        case 't':   /*超时时间*/
          l_opt_arg = optarg;
          if(l_opt_arg && l_opt_arg[0]!=':'){
            printf("TIMEOUT\n");
            len = strlen(l_opt_arg);
            value = strtol(l_opt_arg, NULL, 10);
            if(value != LONG_MAX && value != LONG_MIN)
              conf_para.TimeOut = value;
          }
          
          break;
        case '?':/*错误参数*/
          printf("Invalid para\n");
        case 'h': /*帮助*/
          display_usage();
          break;
        }
    }
} 



 
/**
 * 解析配置文件
 * @param file 配置文件的路径
 */
void Parameter_ParseFile(char *file){

  printf("正在解析文件\n");

  char line[LINELENGTH];//读取一行
  char *name =NULL;     //配置名字
  char *value =NULL;    //配置值

  int fd=-1; //文件描述符
  int n=0;

  /*打开文件*/
  fd=open(file,O_RDONLY);
  if (fd==-1)
  {
    return;
  }


  /*
  *命令格式如下:
  *[#注释|[空格]关键字[空格]=[空格]value]
  */
  while( (n = conf_readline(fd, line, LINELENGTH)) !=0)
  { 
    //printf("%s\n",line);
    char *pos = line; 
    /* 跳过一行开头部分的空格 */
    while(isspace(*pos)){
      pos++;
    }
    /*注释?*/
    if(*pos == '#'){
      continue;
    }
    
    /*关键字开始部分*/
    name = pos;
    /*关键字的末尾*/
    while(!isspace(*pos) && *pos != '=')
    {
      pos++;
    }
    *pos = '\0';/*生成关键字字符串*/
    pos++;

    /*value部分前面空格*/
    while(isspace(*pos)|| *pos =='=')
    {
      pos++;
    }
    //printf("%c\n", *pos);
    /*value开始*/ 
    value = pos;
    /*到结束*/
    while(!isspace(*pos) && *pos != '\r' && *pos != '\n')
    {
      pos++;
    }
    *pos = '\0';/*生成值的字符串*/


    /*根据关键字部分，获得value部分的值*/
    int ivalue;
    //printf("%s %s\n",name,value);
    /*"CGIRoot","DefaultFile","DocumentRoot","ListenPort","MaxClient","TimeOut"*/
    if(!strncmp("CGIRoot", name, 7)) {/*CGIRoot部分*/
      memcpy(conf_para.CGIRoot, value, strlen(value)+1);
    }else if(!strncmp("DefaultFile", name, 11)){/*DefaultFile部分*/
      memcpy(conf_para.DefaultFile, value, strlen(value)+1);
    }else if(!strncmp("DocumentRoot", name, 12)){/*DocumentRoot部分*/
      memcpy(conf_para.DocumentRoot, value, strlen(value)+1);
    }else if(!strncmp("ListenPort", name, 10)){/*ListenPort部分*/
      ivalue = strtol(value, NULL, 10);
      conf_para.ListenPort = ivalue;
    }else if(!strncmp("MaxClient", name, 9)){/*MaxClient部分*/
      ivalue = strtol(value, NULL, 10);
      conf_para.MaxClient = ivalue;
    }else if(!strncmp("TimeOut", name, 7)){/*TimeOut部分*/
      ivalue = strtol(value, NULL, 10);
      conf_para.TimeOut = ivalue;
    }   
  }
  close(fd);

//printf("退出\n");
  return;
}



/***************************************************
*函数名:Parameter_Init
*函数功能：参数初始化 解析命令的参数 与配置文件的参数 
           主要是对上面参数的调用 
*输入参数:int argc:     输入的参数数目 
          char* argv[]: 输入的参数值 
*输出参数： 
****************************************************/
void Parameter_Init(int argc,char* argv[]){
	
	Parameter_ParseCmd(argc,argv); /*解析命令行参数*/

  if (strlen(conf_para.ConfigFile)) /*解析配置文件*/
  {
    Parameter_ParseFile(conf_para.ConfigFile);
  }
  
  display_para();/*显示已经解析好的参数*/

  return;
	
} 
