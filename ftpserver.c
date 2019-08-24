/*************************************************************************
  > File Name: telser.c
# File Name: telser.c
# Author :pengjing
# QQ : 784915651
# Email:784915651@qq.com
 ************************************************************************/

#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
//IO multiplexing
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
//select
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
//socket config
#include <sys/socket.h>
//sqlite3
#include <sqlite3.h>
//helper interface
#include "common.h"
#include "seqlist.h"

//socket init
int my_socket_listen(const char *ip, int port);
int my_socket_while_select(int listenfd);

//business handle
int business_rwaync_config();

//helper	
RequestType get_request_type(const char *type, char *status);

//business handle
int handle_op(int confd_t);
int handle_list(int socket);
void* handle_get(void *arg);
void* handle_put(void *arg);
int handle_login(int socket, const char *username, const char *userpwd);
int handle_regist(int socket, const char *username, const char *userpwd);
int handle_say(int socket, const char *data);

//define macro
#define DB_PATH "../ftpdb.db"

//define global var
fd_set set_rfd;//create file desc set
sqlite3 *db;//database
Seqlist *op_files = NULL;
pthread_mutex_t op_files_mutex;

int main(int argc, char *argv[])
{
	//user input ip,port
	/*
	   if(argc != 3){
	   printf("./tels <ip> <port>");
	   return -1;
	   }*/
	//open database
	int res = sqlite3_open(DB_PATH, &db);
	if(SQLITE_OK != res){
		printf("%s\n", sqlite3_errmsg(db));
		return -1;
	}

	//user config
	res = business_rwaync_config();
	if(res < 0){
		return -1;
	}
	//test
	//int ttt = handle_login("pengjing", "123456");
	/*int ttt = handle_regist(7, "test2", "test2");
	  if(ttt == 0){
	  printf("login success\n");
	  }else{
	  printf("login fail\n");
	  }
	  return 0;*/

	//socket listen
	int listenfd = my_socket_listen(NULL, 11223);
	if(listenfd < 0){
		return -1;
	}

	my_socket_while_select(listenfd);

	sqlite3_close(db); //close database
	return 0;
}

int my_socket_listen(const char *ip, int port){
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == listenfd)
	{
		perror("server socket error");
		return -1;
	}

	int optval = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));	

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	//addr.sin_port = htons(atoi(argv[2]));
	//addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = htons(port);
	if(ip == NULL){
		addr.sin_addr.s_addr = INADDR_ANY;
	}else{
		addr.sin_addr.s_addr = inet_addr(ip);
	}

	int res = bind(listenfd, (struct sockaddr *)&addr, sizeof(addr));
	if(-1 == res)
	{
		perror("server bind error");
		return -1;
	}

	res = listen(listenfd, 100);
	if(-1 == res)
	{
		perror("server listen error");
		return -1;
	}
	printf("server init success\n");
	return listenfd;
}

int my_socket_while_select(int listenfd){
	//check client connect request
	struct sockaddr_in addr_con;
	int size_addr_con = sizeof(addr_con);
	memset(&addr_con, 0, size_addr_con);
	int confd = -1;
	int res = -1;
	fd_set set_rfd_t;
	FD_ZERO(&set_rfd);
	FD_SET(listenfd, &set_rfd);//add a listenfd to the collection
	int max_rfd = listenfd;//max file desc + 1
	struct timeval timeout;
	int fd = 0;
	while(1){
		//looping data reassign
		set_rfd_t = set_rfd;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

		//loop check is file desc need op
		res = select(max_rfd+1, &set_rfd_t, NULL, NULL, &timeout);
		if(res < 0){
			perror("select error");
			break;
		}else if(0 == res){
			printf("timeout !\n");
			continue;
		}
		//need op
		int count = max_rfd;
		for(fd = 0; fd <= count; fd++){
			if(FD_ISSET(fd, &set_rfd_t) > 0){
				if(fd == listenfd){
					//is listend socket
					confd = accept(listenfd, (struct sockaddr *)&addr_con, &size_addr_con);
					if(-1 == confd)
					{
						perror("client connect fail");
						continue;
					}
					printf("IP %s connect\n", inet_ntoa(addr_con.sin_addr));
					FD_SET(confd, &set_rfd);
					if(confd > max_rfd) max_rfd = confd; 
				}else{
					//is connect socket
					res = handle_op(fd);
					if(res < 0){
						FD_CLR(fd, &set_rfd);
						close(fd);
					}
				}
			}
		}
	}
	close(listenfd);
	return 0;
}

int business_rwaync_config(){
	int res = -1;
	op_files = seqlist_create();
	if(NULL == op_files){
		printf("op_files create fail\n");
		return -1;
	}
	res = pthread_mutex_init(&op_files_mutex, NULL);
	if(res < 0){
		printf("op_files_mutex init fail\n");
		return -1;
	}
	return 0;	
}

int handle_op(int confd_t)
{
	RequestBody req;
	memset(&req, 0, SIZE_REQ);

	int res = -1;
	int num = read(confd_t, &req, SIZE_REQ);
	if(num < 0)
	{
		printf("read %d error\n", confd_t);
		return -2;
	}
	else if(num == 0)
	{
		printf("%d exit\n", confd_t);
		return -1;
	}

	printf("%d:%s\n", confd_t, RequestTypeKey[req.type]);
	//business handle
	switch(req.type)
	{
		case none:
			printf("%d:none\n", confd_t);
			break;
		case list:
			{
				handle_list(confd_t);
				break;
			}
		case get:
			{
				FD_CLR(confd_t, &set_rfd);
				pthread_t thr;
				pthread_detach(thr);
				ThreadArg *parg = (ThreadArg *)malloc(sizeof(ThreadArg));
				parg->i_arg = confd_t;
				strcpy(parg->s_arg, req.s_arg);
				pthread_create(&thr, NULL, handle_get, parg);
				break;
			}
		case put:
			{
				FD_CLR(confd_t, &set_rfd);
				pthread_t thr;
				pthread_detach(thr);
				ThreadArg *parg = (ThreadArg *)malloc(sizeof(ThreadArg));
				parg->i_arg = confd_t;
				parg->i_arg2 = req.i_arg;
				strcpy(parg->s_arg, req.s_arg);
				pthread_create(&thr, NULL, handle_put, parg);
				break;
			}
		case login:{
					   handle_login(confd_t, req.s_arg, req.s_arg2);
					   break;
				   }
		case regist:{
						handle_regist(confd_t, req.s_arg, req.s_arg2);
						break;
					}
		case say:{
					 handle_say(confd_t, req.s_arg);
					 break;
				 }
		default:{
					printf("%d request error\n", confd_t);
					break;
				}
	}
	return 0;
}

int handle_list(int socket)
{
	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);

	//query the file name in the directory
	DIR *dir = opendir(PATH);
	if(NULL == dir)
	{
		return -1;
	}
	struct dirent* rent = NULL;
	while((rent = readdir(dir))!= NULL)
	{
		if(rent->d_name[0] == '.')
		{
			continue;
		}
		strcat(resp.data, rent->d_name);
		strcat(resp.data, "\n");
	}
	int len = strlen(resp.data);
	resp.data[len-1] = '\0';
	resp.datalen = len-1;
	closedir(dir);

	//send result
	resp.type = list;
	resp.status = 1;
	my_write(socket, &resp, SIZE_RESP);
	printf("list success\n");
	return 0;
}

bool condition_op_file(data_t* data, void *cmpvalue){
	if(strcmp(data->name, cmpvalue) == 0){
		return true;
	}
	return false;
}


void* handle_get(void *arg)
{
	printf("get ----\n");
	int res = -1;
	void * ret = NULL;
	ThreadArg* parg = (ThreadArg *)arg;
	int socket = parg->i_arg;
	if(socket <= 0){
		printf("get -> socket error\n");
		goto geterror;
	}
	ResponeBody resp;
	resp.type = getData;
	int len_get = 0,num=0;
	memset(&resp, 0, SIZE_RESP);

	//printf("%d:get\n", socket);

	if(strlen(parg->s_arg) <= 0){
		sprintf(resp.data, "please input filename");
		my_write(socket, &resp, SIZE_RESP);
		printf("get -> please input filename\n");
		goto geterror;
	}

	//set file is read status
	int index = -1;
	pthread_mutex_lock(&op_files_mutex);
	res = seqlist_find_bycondition(op_files, condition_op_file, parg->s_arg);
	if(res < 0){
		data_t data;
		strcpy(data.name, parg->s_arg);
		data.type = OP_READ;
		index = seqlist_get_length(op_files);
		seqlist_insert(op_files, index, data);
	}else{
		index = res;
		data_t data = op_files->data[res];
		if(data.type == OP_WRITE){//already write
			// can not read
			pthread_mutex_unlock(&op_files_mutex);
			printf("get -> curr have user operating, please try again later\n");

			resp.status = 0;
			strcpy(resp.data, "curr have user operating, please try again later");
			my_write(socket, &resp, SIZE_RESP);
			goto geterror;
		}
	}
	pthread_mutex_unlock(&op_files_mutex);

	char path[50] = {0};
	sprintf(path, "%s%s", PATH, parg->s_arg);
	struct stat stat_t;
	res = stat(path, &stat_t);
	if(res < 0){
		strcpy(resp.data, strerror(errno));
		my_write(socket, &resp, SIZE_RESP);
		printf("get -> stat error\n");
		goto geterror;
	}
	if(stat_t.st_size <= 0){
		sprintf(resp.data, "%s is empty", parg->s_arg);
		my_write(socket, &resp, SIZE_RESP);
		printf("get -> %s\n",resp.data);
		goto geterror;
	}
	int fd = open(path, O_RDWR);
	if(fd < 0)
	{
		strcpy(resp.data, strerror(errno));
		my_write(socket, &resp, SIZE_RESP);
		goto geterror;
	}
	resp.type = getData;
	resp.status = 1;
	resp.datatotal = stat_t.st_size;
	my_write(socket, &resp, SIZE_RESP);

	printf("get ++++ while\n");
	while((num = read(fd, resp.data, SIZE_RESP_DATA)) > 0)
	{
		resp.type = getData;
		resp.status = 1;
		resp.datalen = num;
		res = my_write(socket, &resp, SIZE_RESP);
		if(res < 0){
			printf("get ++++ res < 0\n");
			goto geterror;
		}
		memset(&resp, 0, SIZE_RESP);
		usleep(20);
	}
	close(fd);
geterror:
	pthread_mutex_lock(&op_files_mutex);
	op_files->data[res].type &= ~OP_READ;
	pthread_mutex_unlock(&op_files_mutex);
	FD_SET(parg->i_arg, &set_rfd);
	free(parg);
	printf("get ====\n");
	return ret;
}

void* handle_put(void *arg)
{
	void *ret = NULL;
	ThreadArg *parg = (ThreadArg *)arg;
	int socket = parg->i_arg;
	char *filename = parg->s_arg;
	int datatotal = parg->i_arg2;

	if(NULL == filename || 0 == *filename){
		ret = (void *)-1;
		printf("put -> filename error\n");
		goto puterror;
	}

	if(datatotal <= 0){
		ret = (void *)-1;
		printf("put -> datatotal <= 0\n");
		goto puterror;
	}
	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);
	resp.type = putData;

	//set file is write status
	int index = -1;
	pthread_mutex_lock(&op_files_mutex);
	int res = seqlist_find_bycondition(op_files, condition_op_file, filename);
	if(res < 0){
		data_t data;
		strcpy(data.name, filename);
		data.type = OP_WRITE;
		index = seqlist_get_length(op_files);
		seqlist_insert(op_files, index, data);
	}else{	
		index = res;
		data_t data = op_files->data[res];
		if(data.type != OP_NONE){
			pthread_mutex_unlock(&op_files_mutex);
			printf("put -> curr have user operating, please try again late\n");

			resp.status = 0;
			strcpy(resp.data, "curr have user operating, please try again later\n");
			my_write(socket, &resp, SIZE_RESP);
			ret = (void *)0;
			goto puterror;
		}
	}
	pthread_mutex_unlock(&op_files_mutex);

	char path[50] = {0};
	sprintf(path, "%s%s", PATH, filename);

	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0){
		ret = (void *)-2;
		printf("put -> open error\n");
		resp.status = 0;
		strcpy(resp.data, "server put -> open file error\n");
		my_write(socket, &resp, SIZE_RESP);
		goto puterror;
	}

	resp.status = 1;
	strcpy(resp.data, "data start\n");
	my_write(socket, &resp, SIZE_RESP);

	int len_curr_resp = resp.datalen + SIZE_RESP_INFO;
	int num = 0;
	int len_recv = 0; 

	while(1){
		my_read(socket, (char *)&resp, SIZE_RESP);
		if(resp.type != putData || 0 == resp.status){
			printf("server put -> client resp config error\n");
			ret = (void *)-2;
			break;
		}
		write(fd, resp.data, resp.datalen);
		len_recv += resp.datalen;
		printf("server recv ----[%2.2f%%]----\n", len_recv/(double)datatotal * 100);
		memset(&resp, 0, SIZE_RESP);
		usleep(20);
		if(len_recv >= datatotal){
			printf("server recv success\n");
			ret = (void *)0;
			break;
		}
	}
	close(fd);
puterror:
	pthread_mutex_lock(&op_files_mutex);
	op_files->data[res].type &= ~OP_WRITE;
	pthread_mutex_unlock(&op_files_mutex);
	FD_SET(socket, &set_rfd);
	free(parg);
	return ret;
}

int handle_login(int socket, const char *username, const char *userpwd){
	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);
	resp.type = login;
	resp.status = 1;
	/*
	   my_write(socket, &resp, SIZE_RESP);
	   return 0;*/
	if(username == NULL || '\0' == *username){
		my_write(socket, &resp, SIZE_RESP);
		return -1;
	}
	if(userpwd == NULL || '\0' == *userpwd){
		my_write(socket, &resp, SIZE_RESP);
		return -1;
	}
	int ret = -1, res = -1;
	char sql[100] = {0};
	sprintf(sql, "select count(*) from ftp_user where status=1 and username='%s' and userpwd='%s'",username, userpwd);
	char **result;
	int nrow=0, ncolumn=0;
	char *errmsg = NULL;
	res = sqlite3_get_table(db, sql, &result, &nrow, &ncolumn, &errmsg);
	if(0 == res){
		int count = atoi(*(result+ncolumn*1+0));
		printf("count:%d\n", count);
		if(count > 0){
			resp.status = 1;
			strcpy(resp.data, "login success");
			res = my_write(socket, &resp, SIZE_RESP);
			ret = 0;
		}else{
			resp.status = 0;
			strcpy(resp.data, "username or userpwd error");
			res = my_write(socket, &resp, SIZE_RESP);
			ret = 0;
		}
	}else{
		printf("errmsg:%s\n", errmsg);
		strcpy(resp.data, errmsg);
		my_write(socket, &resp, SIZE_RESP);
	}
	sqlite3_free_table(result);
	return ret;
}

int handle_regist(int socket, const char *username, const char *userpwd){
	ResponeBody resp;
	memset(&resp, 0, SIZE_RESP);
	resp.type = regist;
	resp.status = 0;
	if(username == NULL || '\0' == *username){
		my_write(socket, &resp, SIZE_RESP);
		return -1;
	}
	if(userpwd == NULL || '\0' == *userpwd){
		my_write(socket, &resp, SIZE_RESP);
		return -1;
	}
	int res = -1;
	//query username is exist
	char sql[100] = {0};
	sprintf(sql, "select count(username) from ftp_user where username='%s'",username);
	char **result;
	int nrow=0, ncolumn=0;
	char *errmsg = NULL;
	res = sqlite3_get_table(db, sql, &result, &nrow, &ncolumn, &errmsg);
	if(0 == res){
		int count = atoi(*(result+ncolumn*1+0));
		if(count > 0){
			strcpy(resp.data, "username is exist");
			res = my_write(socket, &resp, SIZE_RESP);
			return 0;
		}
	}else{
		printf("errmsg:%s\n", errmsg);
		strcpy(resp.data, errmsg);
		my_write(socket, &resp, SIZE_RESP);
		return 0;
	}
	//insert record
	sprintf(sql, "insert into ftp_user(username, userpwd) values('%s','%s')",username, userpwd);
	res = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if(0 == res){
		resp.status = 1;
		strcpy(resp.data, "regist success");
		res = my_write(socket, &resp, SIZE_RESP);
	}else{
		printf("errmsg:%s\n", errmsg);
		strcpy(resp.data, errmsg);
		my_write(socket, &resp, SIZE_RESP);
	}
	return 0;
}

int handle_say(int socket, const char *data){
	printf("%d say: %s\n", socket, data);
	return 0;
}
