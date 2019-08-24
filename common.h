#ifndef COMMON_H
#define COMMON_H

#define SIZE_REQ 88

#define SIZE_RESP_DATA 1000
#define SIZE_RESP_INFO 36
#define SIZE_RESP 1036

#define PATH "./"

typedef struct RequestBody{
    char type;//4
	char s_arg[40];
	char s_arg2[40];
    int i_arg;//4
}RequestBody;

typedef struct ResponeBody{
    char type;//2 byte
    char status;//2 byte
    int datalen;//4 byte
    int datatotal;//4 byte
    char arg[24];//24 byte
    char data[SIZE_RESP_DATA];
}ResponeBody;

typedef enum RequestType{
	none, list, get, put, 
	login, regist, say, quit, clear, getData, putData
}RequestType;

char *RequestTypeKey[] = {"none", "list", "get", "put",
						"login", "regist", "say", "quit", "clear", "getData", "putData"};

typedef struct ThreadArg{
	int i_arg;
	int i_arg2;
	char s_arg[50];
	char s_arg2[50];
}ThreadArg;

int my_write(int socket, void *data, int len){
	if(socket == 0 || data == NULL || len <= 0){
		return -1;
	}
	int i = 0;
	int len_t = 0;
	while(i < len){
		len_t = len-i > SIZE_RESP ? SIZE_RESP : len-i;
		i +=  write(socket, data+i, len_t);
	}
	return len;
}

void my_read(int socket, void *data, int len){
	int i = 0;
	int len_t = 0;
	while(i < len){
		len_t = len-i > SIZE_RESP ? SIZE_RESP : len-i;
		i += read(socket, data+i, len_t);
	}
}

#endif // COMMON_H
