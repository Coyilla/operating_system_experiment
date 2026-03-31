// cmd3.c 文件信息查看器
# include <stdio.h>
# include <sys/stat.h>
# include <unistd.h>
# include <time.h>
# include <string.h>
# include <pwd.h>
# include <grp.h>

void print_file_info(const char *filename){
	struct stat file_stat;
	if (stat(filename,&file_stat)<0){
		printf("错误：无法获取文件'%s'的信息\n",filename);
		return;
	}

	printf("文件信息：%s \n",filename);
	printf("文件大小：%ld 字节（%.2f KB）\n",file_stat.st_size,file_stat.st_size/1024.0);

	//文件类型
	printf("文件类型：");
	if (S_ISREG(file_stat.st_mode)) printf("普通文件\n");
	else if (S_ISDIR(file_stat.st_mode)) printf("目录\n");
	else if (S_ISLNK(file_stat.st_mode)) printf("符号链接\n");
	else if (S_ISCHR(file_stat.st_mode)) printf("字符设备\n");
	else if (S_ISBLK(file_stat.st_mode)) printf("块文件\n");
	else if (S_ISFIFO(file_stat.st_mode)) printf("FIFO/管道\n");
	else if (S_ISSOCK(file_stat.st_mode)) printf("套接字\n");
	else printf("未知\n");

	//权限
	printf("权限：");
	printf((S_ISDIR(file_stat.st_mode)) ? "d" : "-");
	printf((file_stat.st_mode & S_IRUSR) ? "r" : "-");
	printf((file_stat.st_mode & S_IWUSR) ? "w" : "-");
	printf((file_stat.st_mode & S_IXUSR) ? "x" : "-");
	printf((file_stat.st_mode & S_IRGRP) ? "r" : "-");
	printf((file_stat.st_mode & S_IWGRP) ? "w" : "-");
	printf((file_stat.st_mode & S_IXGRP) ? "x" : "-");
	printf((file_stat.st_mode & S_IROTH) ? "r" : "-");
	printf((file_stat.st_mode & S_IWOTH) ? "w" : "-");
	printf((file_stat.st_mode & S_IXOTH) ? "x" : "-");
	printf("\n");

	//用户和组信息
	struct passwd *pwd =getpwuid(file_stat.st_uid);
	struct group *grp = getgrgid(file_stat.st_gid);
	printf("所有者: %s\n", pwd ? pwd->pw_name : "Unknown");
	printf("所属组: %s\n", grp ? grp->gr_name : "Unknown");

	// 时间信息
	printf("最后修改: %s", ctime(&file_stat.st_mtime));
	printf("最后访问: %s", ctime(&file_stat.st_atime));
}

int main(int argc, char *argv[]) {
	printf("=== 文件信息查看器 ===\n");

	if (argc < 2) {
		printf("使用方法: cmd3 [文件名]\n");
		printf("例如: cmd3 myshell.c\n");
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		if (i > 1) printf("\n");
		print_file_info(argv[i]);
	}
	return 0;
}
