// cmd1.c 显示当前时间与日期
# include <stdio.h>
# include <time.h>

int main(){
	time_t t=time(NULL);
	struct tm *tm_info=localtime(&t);

	printf("当前时间：\n");
	printf("日期：%04d-%02d-%02d\n",
			tm_info->tm_year+1900,
			tm_info->tm_mon+1,
			tm_info->tm_mday);
	printf("时间: %02d:%02d:%02d\n", 
			tm_info->tm_hour, 
			tm_info->tm_min, 
			tm_info->tm_sec);
	printf("星期：%d\n",tm_info->tm_wday);
	return 0;
}
