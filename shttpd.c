/******************************************************************************
 * Filename: shttpd.c      
 *
 * %Description:  shttp服务器的主文件    
 * %date_created:  2019.6.19 
 * %version:       1 
 * %authors:       Theo_hui
*******************************************************************************/

# include "shttpd.h" 


/******************************************************************
 用户ctrl-c 之后进行处理退出 
*******************************************************************/

static int signal_int(int num){
	
	printf("停止操作"); 
} 

	
int main(int argc,char *argv[]){
	
	/*挂接信号 用户ctrl-c之后进行退出*/
	while(1){
		signal(SIGINT,signal_int);	
	}
	
	
	printf("hello\n"); 
}