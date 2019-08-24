/*************************************************************************
  > File Name: telc.c
# File Name: telc.c
# Author :彭敬
# QQ : 784915651
# Email:784915651@qq.com
# Blog：http://www.cnblogs.com/shenlanqianlan/
# Created Time: 2017年12月06日 星期三 17时00分22秒
 ************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include "common.h"
//file op
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

//socket config
#include <sys/types.h>
#include <sys/socket.h>

//socket init
int my_socket_init(const char *ip, int port);
void sig_handle_sigusr1(int arg1);

//helper
int my_get_request(const char *str_req, char *type, char arg[40]);

//business handle
int handle_list(int socket);
int handle_get(int socket, const char *filename);
int handle_put(int socket, const char *filename);

int main(int argc, char *argv[])
{
	/*
	printf("req %d\n", sizeof(RequestBody));
	printf("resp %d\n", sizeof(ResponeBody));
	return 0;
	*/
	
	//recv user arg
	/*
	   if(argc != 3){
	   printf("./ftpc <ip> <port>");
	   return -1;
	   }*/
	//other config
	signal(SIGUSR1, sig_handle_sigusr1);

	//create socket
	int confd = my_socket_init("127.0.0.1", 11223);
	if(confd < 0)
	{
		perror("socket init error");
		return -1;
	}
	printf("weclome to parkin ftp\n");

	//user login
	//my_login(confd);

	//send user op
	printf("****please input your op****\n");
	char buff[100];
	int num = -1, res = -1;
	char arg[40] = {0};
	char type = none;
	while(1)
	{
		memset(buff, 0, 100);
		num = read(0, buff, 100);
		if(num <= 0){
			continue;
		}
		res = my_get_request(buff, &type, arg);
		if(res < 0){
			printf("request error\n");
		}
		else if(get == type){
			handle_get(confd, arg);
		}
		else if(put == type){
			handle_put(confd, arg);
		}
		else if(list == type){
			handle_list(confd);
		}
		else if(quit == type){
			exit(0);
		}
		else if(clear == type){
			system("clear");
		}
	}
	close(confd);
}

int my_socket_init(const char *ip, int port){
	int confd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == confd)
	{
		perror("client socket error");
		return -1;
	}
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	//addr.sin_port = htons(atoi(argv[2]));
	//addr.sin_addr.s_addr = inet_addr(argv[1]);
	//addr.sin_port = htons(atoi("11223"));
	//addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);
	int res = connect(confd, (struct sockaddr *)&addr, sizeof(addr));
	if(res < 0){
		perror("connect fail\n");
		return -1;
	}
	return confd;
}

int my_login(int socket){
	int ret = -1;
	RequestBody req;
	memset(&req, 0, SIZE_REQ);
	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);
	while(1){
		//auth user login
		printf("-----------------------------\n");
		printf("--1.login  2.regist  3.quit--\n");
		printf("-----------------------------\n");
		char op = 0;
		//setbuf(stdin, NULL);
		scanf("%c",&op);
		while(( getchar())!='\n');//clear stdio '\n' cache
		switch(op){
			case '1':{
						 req.type = login;
						 printf("please input user name:");
						 scanf("%s", req.s_arg);
						 printf("please input user pwd:");
						 scanf("%s", req.s_arg2);
						 my_write(socket, &req, SIZE_REQ);
						 my_read(socket, (char *)&resp, SIZE_RESP);
						 if(login == resp.type && 1 == resp.status){
							 printf("login success\n");
							 return 0;
						 }
						 printf("login fail:%s\n", resp.data);
						 break;
					 }
			case '2':{
						 req.type = regist;
						 char userpwd_re[40] = {0};
						 printf("please input user name:");
						 scanf("%s", req.s_arg);
						 printf("please input user pwd:");
						 scanf("%s", req.s_arg2);
						 printf("please re-input user pwd:");
						 scanf("%s", userpwd_re);
						 if(strcmp(req.s_arg2, userpwd_re) == 0){
							 my_write(socket, &req, SIZE_REQ);
							 my_read(socket, (char *)&resp, SIZE_RESP);
							 if(regist == resp.type && 1 == resp.status){
								 printf("regist success\n");
								 continue;
							 }else{
								 printf("regist fail:%s\n", resp.data);
							 }
						 }else{
							 printf("password is different twice\n");
						 }
						 break;
					 }
			case '3':{
						 exit(0);
					 }
			default:{
						printf("input error\n");
						break;
					}
		}
		memset(&req, 0, SIZE_REQ);
		memset(&resp, 0, SIZE_RESP);
		printf("please wait 3 sec...\n");
		sleep(3);
		system("clear");
	}
	return ret;
}

void sig_handle_sigusr1(int arg1){
	//printf("sigusr\n");
}

int my_get_request(const char *str_req, char *type, char arg[40])
{
	if(NULL == str_req || 0 == *str_req)
	{
		return -1;
	}
	char seg[] = " ";
	char charlist[2][50] = {0};
	int i = 0;
	char *substr = strtok((char *)str_req, seg);
	while (substr != NULL)
	{
		strcpy(charlist[i],substr);
		i++;
		substr = strtok(NULL,seg);
	}
	if(i<= 0 || i > 2)
	{
		return -1;
	}
	//\n replace \0
	int len_1 = strlen(charlist[0]);
	if(len_1 > 0 && '\n' == charlist[0][len_1-1])
	{
		charlist[0][len_1-1] = '\0';
	}
	int len_2 = strlen(charlist[1]);
	if(len_2 > 0 && '\n' == charlist[1][len_2-1])
	{
		charlist[1][len_2-1] = '\0';
	}
	
	if(strcmp(charlist[0], "get") == 0)
	{
		*type = get;
	}
	else if(strcmp(charlist[0], "put") == 0)
	{
		*type = put;
	}
	else if(strcmp(charlist[0], "list") == 0)
	{
		*type = list;
	}
	else if(strcmp(charlist[0], "quit") == 0)
	{
		*type = quit;
	}
	else if(strcmp(charlist[0], "clear") == 0)
	{
		*type = clear;
	}
	else if(strcmp(charlist[0], "say") == 0)
	{
		*type = say;
	}
	else{
		*type = none;
	}
	strcpy(arg, charlist[1]);
	return 0;
}

int handle_list(int socket){
	if(socket == 0){
		printf("list socket is empty\n");
		return 0;
	}
	RequestBody req;
	memset(&req, 0, SIZE_REQ);
	req.type = list;
	my_write(socket, &req, SIZE_REQ);

	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);
	my_read(socket, (char *)&resp, SIZE_RESP);
	printf("%s\n", resp.data);
	return 0;
}

int handle_get(int socket,const char *filename)
{
	if(NULL == filename || 0 == *filename){
		printf("get -> filename error");
		return -1;
	}

	char path[50] = {0};
	sprintf(path, "%s%s", PATH, filename);

	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0){
		perror("open error");
		return -1;
	}
	RequestBody req;
	memset(&req, 0, SIZE_REQ);
	req.type = get;
	strcpy(req.s_arg, filename);
	my_write(socket, &req, SIZE_REQ);

	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);
	int datatotal = 0;
	my_read(socket, (char *)&resp, SIZE_RESP);
	if(getData == resp.type){
		if(1 == resp.status){
			datatotal = resp.datatotal;
		}else{
			printf("getData -> %s\n", resp.data);
		}
	}
	memset(&resp, 0, SIZE_RESP);

	int num = 0,len_recv = 0;
	//read data
	int ret = -1;
	while(1){
		my_read(socket, (char *)&resp, SIZE_RESP);
		if(0 == resp.status || resp.type != getData){
			printf("getData -> resp config error\n");
			ret = -2;
			break;
		}
		write(fd, resp.data, resp.datalen);
		len_recv += resp.datalen;
		printf("get ----[%2.2f%%]----\n", len_recv/(double)datatotal * 100);
		memset(&resp, 0, SIZE_RESP);
		if(len_recv >= datatotal){
			printf("get success\n");
			ret = 0;
			break;
		}
	}
	close(fd);
	//kill(getpid(), SIGUSR1);
	return ret;
}

int handle_put(int socket, const char *filename)
{	
	int len_get = 0,num=0,res=-1;

	char path[50] = {0};
	sprintf(path, "%s%s", PATH, filename);
	struct stat stat_t;
	res = stat(path, &stat_t);
	if(res < 0){
		perror("stat error");
		return -2;
	}
	if(stat_t.st_size <= 0){
		printf("%s is empty\n", filename);
		return -2;
	}
	int fd = open(path, O_RDWR);
	if(fd < 0)
	{
		perror("open error");
		return -2;
	}

	RequestBody req;
	memset(&req, 0, SIZE_REQ);
	req.type = put;
	strcpy(req.s_arg, filename);
	req.i_arg = stat_t.st_size;	
	res = my_write(socket, &req, SIZE_REQ);	
	
	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);
	my_read(socket, (char *)&resp, SIZE_RESP);
	if(resp.type == putData){
		if(0 == resp.status){
			printf("putData -> %s\n", resp.data);
			goto puterror;
		}
	}

	while((num = read(fd, resp.data, SIZE_RESP_DATA)) > 0)
	{
		if(num <= 0){
			perror("read error");
			return -1;
		}
		resp.type = putData;
		resp.status = 1;
		resp.datalen = num;
		res = my_write(socket, &resp, SIZE_RESP);
		if(res < 0){
			return -1;
		}
		memset(&resp, 0, SIZE_RESP);
		//usleep(100);
	}
puterror:
	close(fd);
	return 0;
}
