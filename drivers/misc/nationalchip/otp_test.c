#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/select.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#define DBG_PRINT(x, args...) printf(x, ##args)
struct private_info{
	unsigned int index;
	unsigned int length;
	char         *addr;
}info;

#define DEV_NAME "/dev/gx_otp"
#define GX_OTP_READ_NAME       (0x0100) //get chip name
#define GX_OTP_READ_ID         (0x0101) //get Public ID
#define GX_OTP_READ_DATA       (0x0102) //get private data
#define GX_OTP_WRITE_DATA      (0x0103) //set private data
#define GX_OTP_GET_LOCK        (0x0104) //lock all otp
#define GX_OTP_SET_LOCK        (0x0105) //get otp lock status
int fd;
static unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
	unsigned long result = 0,value;
	if (*cp == '0') {
		cp++;
		if ((*cp == 'x') && isxdigit(cp[1])) {
			base = 16;
			cp++;
		}
		if (!base) {
			base = 8;
		}
	}
	if (!base) {
		base = 10;
	}
	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
					? toupper(*cp) : *cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}
	if (endp)
		*endp = (char *)cp;
	return result;
}
static int otp_ioctl_cmd(int argc, char **argv)
{
	int err = 0, i, lock;
	unsigned char buf[100];
	unsigned long long public_id;
	char *cmd = NULL;

	info.addr = buf;
	cmd = (char *)argv[1];
	printf("cmd = %s\n", cmd);
	if (strcmp("name",cmd) == 0){
		err = ioctl(fd, GX_OTP_READ_NAME, buf);
		if (err)
		{
			printf("error: ioctl err !\n");
		}else{
			printf("name : %s\n", buf);
		}
	}else if(strcmp("id",cmd) == 0){
		err = ioctl(fd, GX_OTP_READ_ID, &public_id);
		if (err)
		{
			printf("error: ioctl err !\n");
		}else{
			printf("public_id: %llx\n", public_id);
		}
	}else if (strcmp("set",cmd) == 0){
	}else if (strcmp("data_read",cmd) == 0){
		info.index  = simple_strtoul((const char *)argv[2], NULL, 10);;
		info.length = simple_strtoul((const char *)argv[3], NULL, 10);;
		printf("index %d length %d\n", info.index, info.length);
		err = ioctl(fd, GX_OTP_READ_DATA, &info);
		if (err){
			printf("error: ioctl err !\n");
		}else{
			printf("data :\n");
			for (i = 0; i < info.length; i++){
				printf("0x%x ", buf[i]);
				if (buf[i] < 0x10)
					printf(" ");
				if((i+1)%16 == 0)
					printf("\n");

			}
			printf("\n");
		}
	}else if (strcmp("data_write",cmd) == 0){
		info.index  = simple_strtoul((const char *)argv[2], NULL, 10);;
		info.length = simple_strtoul((const char *)argv[3], NULL, 10);;
		for (i = 0; i < info.length; i++){
			buf[i] = i;
		}
		printf("index %d length %d\n", info.index, info.length);
		err = ioctl(fd, GX_OTP_WRITE_DATA, &info);
		if (err)
			printf("error: ioctl err !\n");
	}else if (strcmp("lock",cmd) == 0){
		err = ioctl(fd, GX_OTP_SET_LOCK, &info);
		if (err)
			printf("error: ioctl err !\n");
	}else if (strcmp("get_lock",cmd) == 0){
		err = ioctl(fd, GX_OTP_GET_LOCK, &lock);
		if (err)
			printf("error: ioctl err !\n");
		else
			printf("otp status: %s\n", lock ? "locked" : "unlocked");
	}else{
		printf(" error: ioctl parameter !\n");
	}
	return 0;
}
static int otp_init(void)
{
	fd = open(DEV_NAME, O_RDWR);
	if( fd < 0 ) {
		printf("ERROR:open %s faile \n",DEV_NAME);
		return -1;
	}
	return 0;
}
static void otp_help(char **argv)
{
	DBG_PRINT("%s name\n", argv[0]);
	DBG_PRINT("%s id\n", argv[0]);
	DBG_PRINT("%s data_read  index length\n", argv[0]);
	DBG_PRINT("%s data_write index length\n", argv[0]);
	DBG_PRINT("%s lock\n", argv[0]);
	DBG_PRINT("%s get_lock\n", argv[0]);
	DBG_PRINT("\n");
}
int main(int argc, char **argv)
{
	char *cmd = NULL;
	if (argc < 2) {
		otp_help(argv);
		return -1;
	}
	else
		cmd = (char *)argv[1];
	if(-1==otp_init()) {
		return -1;
	}
	else {
		otp_ioctl_cmd(argc, (char **)argv);
	}
	close(fd);
	return 0;
}
