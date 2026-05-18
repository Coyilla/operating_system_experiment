#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>

// ========== 磁盘布局常量 ==========
#define BLOCKSIZE 1024           // 磁盘块大小（字节）
#define SIZE 1024000             // 虚拟磁盘总大小（字节） → 1000块
#define END 65535                // FAT 文件结束标志
#define FREE 0                   // FAT 空闲标志
#define MAXOPENFILE 10           // 最多同时打开文件数
#define MAX_PATH_LEN 80          // 最大路径长度
#define MAX_FILENAME_LEN 12      // 最大文件名长度

// FAT 表相关
#define FAT_BLOCKS       2       // 每个 FAT 表占用的块数
#define ROOT_BLOCKS      2       // 根目录占用的块数（改为2个块）

// 各区域起始块号
#define BOOT_BLOCK       0
#define FAT1_START       1
#define FAT2_START       (FAT1_START + FAT_BLOCKS)          // 3
#define ROOT_START       (FAT2_START + FAT_BLOCKS)          // 5
#define DATA_START       (ROOT_START + ROOT_BLOCKS)         // 7

// 总块数
#define TOTAL_BLOCKS     (SIZE / BLOCKSIZE)                  // 1000

// 写入模式
enum WRITE_MODE {
    WRITE_TRUNCATE = 0,
    WRITE_OVERWRITE = 1,
    WRITE_APPEND = 2
};

// 文件控制块
typedef struct FCB {
    char filename[9];
    char exname[4];
    unsigned char attribute; // 0-目录, 1-文件
    unsigned short time;
    unsigned short date;
    unsigned short first;
    unsigned long length;
    char free;
} fcb;

// 文件分配表项
typedef struct FAT {
    unsigned short id;
} fat;

// 打开文件表项
typedef struct USEROPEN {
    char filename[9];
    char exname[4];
    unsigned char attribute;
    unsigned short time;
    unsigned short date;
    unsigned short first;
    unsigned long length;
    char dir[MAX_PATH_LEN];
    int count;
    char fcbstate;
    char topenfile;
} useropen;

// 引导块
typedef struct BLOCK0 {
    char information[200];
    unsigned short root;
    unsigned char *startblock;
} block0;

// 全局变量
unsigned char *myhard;
useropen openfilelist[MAXOPENFILE];
int curdir;
char currentdir[MAX_PATH_LEN];
unsigned char *startp;
int currfd;
int current_dir_block;

// 函数声明
void startsys();
void my_format();
void my_mkdir(char *dirname);
void my_rmdir(char *dirname);
void my_ls(void);
void my_cd(char *dirname);
int my_create(char *filename);
void my_rm(char *filename);
int my_open(char *filename);
void my_close(int fd);
int my_write(int fd, int mode);
int my_read(int fd, int len);
void my_exitsys();
void init_openfilelist();
void init_fat(fat *fat1, fat *fat2);
void my_help();
int find_free_block();
int find_free_fd();
void update_fcb(int fd);
fcb* get_fcb_by_name_in_current_dir(char *filename, int *index);
int parse_filename(char *input, char *name, char *ext);
void show_open_files();
int get_write_mode();
int get_current_dir_block();
void set_current_dir_block(int block);
fcb* get_current_dir_fcb();
int is_file_open(char *filename, char *dir);
int allocate_blocks(int num_blocks, int *first_block);
void free_block_chain(int first_block);
int get_block_chain_length(int first_block);
int write_to_block_chain(int first_block, const char *data, int data_len, int offset, int is_append);
int read_from_block_chain(int first_block, char *buffer, int offset, int len);
void get_fat_time(unsigned short *time, unsigned short *date);
fcb* get_fcb_in_block(int block_num, int index);
int get_fcb_count_in_dir(int block_num);
void traverse_dir_blocks(int block_num, void (*callback)(fcb *entry, int index, int block_num));

void get_fat_time(unsigned short *ftime, unsigned short *fdate) {
    time_t now = time(NULL);                     // 使用全局 time 函数
    struct tm *tm_info = localtime(&now);
    *ftime = (tm_info->tm_hour << 11) | (tm_info->tm_min << 5) | (tm_info->tm_sec / 2);
    *fdate = ((tm_info->tm_year + 1900 - 1980) << 9) |
             ((tm_info->tm_mon + 1) << 5) |
             tm_info->tm_mday;
}

// ========== 遍历目录块的回调函数 ==========
typedef struct {
    fcb *target;
    int found;
    int index;
    int block_num;
    char *name;
    char *ext;
} find_fcb_context;

static void find_fcb_callback(fcb *entry, int index, int block_num) {
    find_fcb_context *ctx = (find_fcb_context *)entry;  // 注意：这里使用了entry作为context指针
    // 实际上我们需要从参数中获取context
    // 由于函数指针限制，这里需要重新设计
}

// ========== 核心功能实现 ==========
int read_from_block_chain(int first_block, char *buffer, int offset, int len) {
    if (first_block == 0 || len <= 0) return 0;
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    int current_block = first_block;
    int bytes_read = 0;
    int block_offset = offset;
    while (block_offset >= BLOCKSIZE) {
        if (fat1[current_block].id == END || fat1[current_block].id == FREE)
            return bytes_read;
        current_block = fat1[current_block].id;
        block_offset -= BLOCKSIZE;
    }
    if (current_block == END || current_block == FREE) return 0;
    while (bytes_read < len) {
        unsigned char *data_block = myhard + current_block * BLOCKSIZE;
        int bytes_in_block = BLOCKSIZE - block_offset;
        int bytes_to_read = (len - bytes_read < bytes_in_block) ? (len - bytes_read) : bytes_in_block;
        memcpy(buffer + bytes_read, data_block + block_offset, bytes_to_read);
        bytes_read += bytes_to_read;
        block_offset = 0;
        if (bytes_read < len) {
            int next_block = fat1[current_block].id;
            if (next_block == END || next_block == FREE) break;
            current_block = next_block;
        }
    }
    return bytes_read;
}

int allocate_blocks(int num_blocks, int *first_block) {
    if (num_blocks <= 0) {
        *first_block = 0;
        return 0;
    }
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    int prev_block = -1;
    int first = -1;
    int allocated = 0;
    
    // 记录本次分配的块号（用于回滚）
    int allocated_blocks[1024];  // 足够大
    int allocated_count = 0;
    
    for (int i = DATA_START; i < TOTAL_BLOCKS && allocated < num_blocks; i++) {
        if (fat1[i].id == FREE) {
            if (first == -1) first = i;
            if (prev_block != -1) fat1[prev_block].id = i;
            prev_block = i;
            allocated_blocks[allocated_count++] = i;
            allocated++;
        }
    }
    if (allocated < num_blocks) {
        // 回滚：释放本次已分配的块
        for (int j = 0; j < allocated_count; j++) {
            fat1[allocated_blocks[j]].id = FREE;
        }
        return -1;
    }
    if (prev_block != -1) fat1[prev_block].id = END;
    *first_block = first;
    return allocated;
}

void free_block_chain(int first_block) {
    if (first_block == 0) return;
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    int current = first_block;
    while (current != END && current != FREE && current < TOTAL_BLOCKS) {
        int next = fat1[current].id;
        fat1[current].id = FREE;
        current = next;
    }
}

int get_block_chain_length(int first_block) {
    if (first_block == 0) return 0;
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    int count = 0;
    int current = first_block;
    while (current != END && current != FREE && current < TOTAL_BLOCKS) {
        count++;
        int next = fat1[current].id;
        if (next == END || next == FREE) break;
        current = next;
    }
    return count;
}

int parse_filename(char *input, char *name, char *ext) {
    char temp[MAX_FILENAME_LEN];
    strncpy(temp, input, MAX_FILENAME_LEN-1);
    temp[MAX_FILENAME_LEN-1] = '\0';
    char *dot = strchr(temp, '.');
    if (dot) {
        if (dot - temp > 8) return -1;
        if (strlen(dot + 1) > 3) return -2;
        *dot = '\0';
        strcpy(name, temp);
        strcpy(ext, dot + 1);
    } else {
        if (strlen(temp) > 8) return -1;
        strcpy(name, temp);
        strcpy(ext, "");
    }
    return 0;
}

int get_write_mode() {
    int mode = WRITE_TRUNCATE;
    char input[20];
    printf("请选择写入模式 (0-截断, 1-覆盖, 2-追加): ");
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    if (fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0) return mode;
        if (sscanf(input, "%d", &mode) != 1) mode = WRITE_TRUNCATE;
        if (mode < 0 || mode > 2) mode = WRITE_TRUNCATE;
    }
    return mode;
}

int is_file_open(char *filename, char *dir) {
    char name[9], ext[4];
    if (parse_filename(filename, name, ext) != 0) return 0;
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile == 1 &&
            strcmp(openfilelist[i].filename, name) == 0 &&
            strcmp(openfilelist[i].exname, ext) == 0 &&
            strcmp(openfilelist[i].dir, dir) == 0)
            return 1;
    }
    return 0;
}

int get_current_dir_block() { return current_dir_block; }
void set_current_dir_block(int block) { current_dir_block = block; }
fcb* get_current_dir_fcb() { return (fcb *)(myhard + get_current_dir_block() * BLOCKSIZE); }

// 修改为支持多块目录
fcb* get_fcb_in_block(int block_num, int index) {
    if (block_num < ROOT_START || block_num >= TOTAL_BLOCKS) return NULL;
    fcb *block_fcbs = (fcb *)(myhard + block_num * BLOCKSIZE);
    int fcb_per_block = BLOCKSIZE / sizeof(fcb);
    if (index < 0 || index >= fcb_per_block) return NULL;
    return &block_fcbs[index];
}

int get_fcb_count_in_dir(int block_num) {
    if (block_num < ROOT_START || block_num >= TOTAL_BLOCKS) return 0;
    fcb *block_fcbs = (fcb *)(myhard + block_num * BLOCKSIZE);
    int fcb_per_block = BLOCKSIZE / sizeof(fcb);
    int count = 0;
    for (int i = 0; i < fcb_per_block; i++) {
        if (block_fcbs[i].free == 1) count++;
    }
    return count;
}

// 修改为支持多块目录
fcb* get_fcb_by_name_in_current_dir(char *filename, int *index) {
    char name[9], ext[4];
    if (parse_filename(filename, name, ext) != 0) return NULL;
    
    int dir_block = get_current_dir_block();
    int is_root = (dir_block == ROOT_START);
    int block_count = is_root ? ROOT_BLOCKS : 1;
    
    for (int block_offset = 0; block_offset < block_count; block_offset++) {
        int current_block = dir_block + block_offset;
        fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
        int max_entries = BLOCKSIZE / sizeof(fcb);
        
        for (int i = 0; i < max_entries; i++) {
            if (current_dir[i].free == 1 &&
                strcmp(current_dir[i].filename, name) == 0 &&
                strcmp(current_dir[i].exname, ext) == 0) {
                if (index != NULL) {
                    *index = block_offset * max_entries + i;
                }
                return &current_dir[i];
            }
        }
    }
    return NULL;
}

void init_openfilelist() {
    for (int i = 0; i < MAXOPENFILE; i++) {
        openfilelist[i].topenfile = 0;
        strcpy(openfilelist[i].filename, "");
        strcpy(openfilelist[i].exname, "");
        openfilelist[i].attribute = 0;
        openfilelist[i].time = 0;
        openfilelist[i].date = 0;
        openfilelist[i].first = 0;
        openfilelist[i].length = 0;
        strcpy(openfilelist[i].dir, "");
        openfilelist[i].count = 0;
        openfilelist[i].fcbstate = 0;
    }
    currfd = -1;
}

void init_fat(fat *fat1, fat *fat2) {
    // 0: 引导块, 1-2: FAT1, 3-4: FAT2, 5-6: 根目录, 7... 数据区
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        unsigned short value = FREE;
        if (i < DATA_START) value = END;   // 系统保留区全部标记为 END
        fat1[i].id = value;
        fat2[i].id = value;
    }
}

int find_free_block() {
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    for (int i = DATA_START; i < TOTAL_BLOCKS; i++) {
        if (fat1[i].id == FREE) return i;
    }
    return -1;
}

int find_free_fd() {
    for (int i = 0; i < MAXOPENFILE; i++)
        if (openfilelist[i].topenfile == 0) return i;
    return -1;
}

void update_fcb(int fd) {
    if (openfilelist[fd].fcbstate == 1) {
        char name[9], ext[4];
        strcpy(name, openfilelist[fd].filename);
        strcpy(ext, openfilelist[fd].exname);
        
        int dir_block = get_current_dir_block();
        int is_root = (dir_block == ROOT_START);
        int block_count = is_root ? ROOT_BLOCKS : 1;
        
        for (int block_offset = 0; block_offset < block_count; block_offset++) {
            int current_block = dir_block + block_offset;
            fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
            int max_entries = BLOCKSIZE / sizeof(fcb);
            
            for (int i = 0; i < max_entries; i++) {
                fcb *fcb_ptr = &current_dir[i];
                if (fcb_ptr->free == 1 &&
                    strcmp(fcb_ptr->filename, name) == 0 &&
                    strcmp(fcb_ptr->exname, ext) == 0) {
                    
                    // 更新时间
                    unsigned short fat_time, fat_date;
                    get_fat_time(&fat_time, &fat_date);
                    fcb_ptr->time = fat_time;
                    fcb_ptr->date = fat_date;
                    
                    fcb_ptr->length = openfilelist[fd].length;
                    fcb_ptr->first = openfilelist[fd].first;
                    
                    // 同步 FAT2
                    fat *fat1 = (fat *)(myhard + FAT1_START * BLOCKSIZE);
                    fat *fat2 = (fat *)(myhard + FAT2_START * BLOCKSIZE);
                    for (int j = 0; j < TOTAL_BLOCKS; j++)
                        fat2[j].id = fat1[j].id;
                    
                    openfilelist[fd].fcbstate = 0;
                    return;
                }
            }
        }
    }
}

int write_to_block_chain(int first_block, const char *data, int data_len, int offset, int is_append) {
    if (data_len <= 0) return 0;
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    int current_block = first_block;
    int bytes_written = 0;
    int block_offset = offset % BLOCKSIZE;

    if (first_block == 0) return 0;

    if (is_append && first_block != 0) {
        int last_block = first_block;
        while (fat1[last_block].id != END && fat1[last_block].id != FREE)
            last_block = fat1[last_block].id;
        current_block = last_block;
        block_offset = 0;
    }

    int blocks_needed = (data_len + block_offset + BLOCKSIZE - 1) / BLOCKSIZE;
    int existing_blocks = get_block_chain_length(first_block);

    if (blocks_needed > existing_blocks) {
        int new_blocks_needed = blocks_needed - existing_blocks;
        int new_first_block = 0;
        int allocated = allocate_blocks(new_blocks_needed, &new_first_block);
        if (allocated < new_blocks_needed) return -1;
        if (first_block != 0) {
            int last_block = first_block;
            while (fat1[last_block].id != END && fat1[last_block].id != FREE)
                last_block = fat1[last_block].id;
            fat1[last_block].id = new_first_block;
        } else {
            first_block = new_first_block;
        }
        current_block = first_block;
    } else if (first_block == 0) return 0;

    // 定位到写入起始块
    int temp_offset = offset;
    int temp_block = first_block;
    while (temp_offset >= BLOCKSIZE) {
        int next_block = fat1[temp_block].id;
        if (next_block == END || next_block == FREE) return bytes_written;
        temp_block = next_block;
        temp_offset -= BLOCKSIZE;
    }
    current_block = temp_block;
    block_offset = temp_offset;

    while (bytes_written < data_len) {
        unsigned char *data_block = myhard + current_block * BLOCKSIZE;
        int space_in_block = BLOCKSIZE - block_offset;
        int bytes_to_write = (data_len - bytes_written < space_in_block) ?
                             (data_len - bytes_written) : space_in_block;
        memcpy(data_block + block_offset, data + bytes_written, bytes_to_write);
        bytes_written += bytes_to_write;
        block_offset = 0;
        if (bytes_written < data_len) {
            int next_block = fat1[current_block].id;
            if (next_block == END || next_block == FREE) break;
            current_block = next_block;
        }
    }
    return bytes_written;
}

void show_open_files() {
    printf("\n已打开的文件:\n");
    printf("FD\t文件名\t\t读写指针\t文件长度\t状态\n");
    printf("--------------------------------\n");
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile == 1) {
            char fullname[13];
            if (strlen(openfilelist[i].exname) > 0)
                sprintf(fullname, "%s.%s", openfilelist[i].filename, openfilelist[i].exname);
            else
                strcpy(fullname, openfilelist[i].filename);
            printf("%d\t%s\t\t%d\t\t%lu\t\t%s\n",
                   i, fullname, openfilelist[i].count,
                   openfilelist[i].length,
                   (i == currfd) ? "当前" : "打开");
        }
    }
    printf("当前文件描述符: %d\n", currfd);
}

void my_help() {
    printf("文件系统命令帮助\n");
    printf("--------------------------------\n");
    printf("format     - 格式化磁盘\n");
    printf("mkdir <dir> - 创建目录\n");
    printf("rmdir <dir> - 删除目录\n");
    printf("ls         - 显示目录内容\n");
    printf("cd <dir>   - 切换目录\n");
    printf("create <file> - 创建文件\n");
    printf("rm <file>  - 删除文件\n");
    printf("open <file> - 打开文件\n");
    printf("close [fd] - 关闭文件\n");
    printf("write [fd] - 写入文件(0-截断,1-覆盖,2-追加)\n");
    printf("read [fd] [len] - 读取文件\n");
    printf("openlist   - 显示已打开文件\n");
    printf("help       - 帮助信息\n");
    printf("exit       - 退出系统\n");
    printf("--------------------------------\n");
}

void startsys() {
    myhard = (unsigned char *)malloc(SIZE);
    if (myhard == NULL) {
        printf("内存分配失败！\n");
        exit(1);
    }
    FILE *fp = fopen("disk.img", "rb");
    if (fp != NULL) {
        fread(myhard, 1, SIZE, fp);
        fclose(fp);
        printf("加载虚拟磁盘文件 disk.img\n");
    } else {
        printf("虚拟磁盘文件不存在，进行格式化...\n");
        my_format();
    }
    curdir = 0;
    strcpy(currentdir, "/");
    set_current_dir_block(ROOT_START);
    startp = myhard + DATA_START * BLOCKSIZE;
    init_openfilelist();
    printf("文件系统启动成功！\n");
    printf("输入 'help' 查看命令\n");
}

void my_format() {
    block0 *boot = (block0 *)myhard;
    strcpy(boot->information, "Simple File System v2.0 - Root: 2 blocks");
    boot->root = ROOT_START;                // 根目录起始块号 = 5
    boot->startblock = myhard + DATA_START * BLOCKSIZE;

    fat *fat1 = (fat *)(myhard + FAT1_START * BLOCKSIZE);
    fat *fat2 = (fat *)(myhard + FAT2_START * BLOCKSIZE);
    init_fat(fat1, fat2);

    // 初始化根目录（占用 ROOT_START 块，共 ROOT_BLOCKS 个块）
    unsigned short fat_time, fat_date;
    get_fat_time(&fat_time, &fat_date);
    
    for (int block_offset = 0; block_offset < ROOT_BLOCKS; block_offset++) {
        fcb *root_dir = (fcb *)(myhard + (ROOT_START + block_offset) * BLOCKSIZE);
        int fcb_per_block = BLOCKSIZE / sizeof(fcb);
        for (int i = 0; i < fcb_per_block; i++) {
            root_dir[i].free = 0;
            strcpy(root_dir[i].filename, "");
            strcpy(root_dir[i].exname, "");
            root_dir[i].attribute = 0;
            root_dir[i].time = 0;
            root_dir[i].date = 0;
            root_dir[i].first = 0;
            root_dir[i].length = 0;
        }
        
        // 第一个块包含 "." 和 ".."
        if (block_offset == 0) {
            fcb *dot = &root_dir[0];
            strcpy(dot->filename, ".");
            strcpy(dot->exname, "");
            dot->attribute = 0;
            dot->first = ROOT_START;
            dot->length = ROOT_BLOCKS * BLOCKSIZE;  // 根目录大小为2个块
            dot->free = 1;
            dot->time = fat_time;
            dot->date = fat_date;

            fcb *dotdot = &root_dir[1];
            strcpy(dotdot->filename, "..");
            strcpy(dotdot->exname, "");
            dotdot->attribute = 0;
            dotdot->first = ROOT_START;  // 根目录的父目录指向自己
            dotdot->length = ROOT_BLOCKS * BLOCKSIZE;
            dotdot->free = 1;
            dotdot->time = fat_time;
            dotdot->date = fat_date;
        }
    }

    printf("格式化完成！磁盘布局：\n");
    printf("  引导块: 块 %d\n", BOOT_BLOCK);
    printf("  FAT1: 块 %d-%d\n", FAT1_START, FAT1_START + FAT_BLOCKS - 1);
    printf("  FAT2: 块 %d-%d\n", FAT2_START, FAT2_START + FAT_BLOCKS - 1);
    printf("  根目录: 块 %d-%d (共%d个块)\n", ROOT_START, ROOT_START + ROOT_BLOCKS - 1, ROOT_BLOCKS);
    printf("  数据区: 块 %d-%d\n", DATA_START, TOTAL_BLOCKS - 1);
}

void my_mkdir(char *dirname) {
    char name[9], ext[4];
    int result = parse_filename(dirname, name, ext);
    if (result == -1) {
        printf("目录名不能超过8个字符！\n");
        return;
    }
    if (strlen(ext) > 0) {
        printf("目录名不能包含扩展名！\n");
        return;
    }
    if (get_fcb_by_name_in_current_dir(dirname, NULL) != NULL) {
        printf("目录 %s 已存在！\n", dirname);
        return;
    }
    
    int dir_block = get_current_dir_block();
    int is_root = (dir_block == ROOT_START);
    int max_entries_per_block = BLOCKSIZE / sizeof(fcb);
    int total_entries = (is_root ? ROOT_BLOCKS : 1) * max_entries_per_block;
    int free_index = -1;
    int free_block_offset = -1;
    int free_entry_index = -1;
    
    // 在多个块中查找空闲FCB
    for (int block_offset = 0; block_offset < (is_root ? ROOT_BLOCKS : 1); block_offset++) {
        int current_block = dir_block + block_offset;
        fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
        for (int i = 0; i < max_entries_per_block; i++) {
            if (current_dir[i].free == 0) {
                free_index = block_offset * max_entries_per_block + i;
                free_block_offset = block_offset;
                free_entry_index = i;
                break;
            }
        }
        if (free_index != -1) break;
    }
    
    if (free_index == -1) {
        printf("当前目录已满，无法创建目录！\n");
        return;
    }
    
    int new_block = find_free_block();
    if (new_block == -1) {
        printf("磁盘空间不足！\n");
        return;
    }
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    fat1[new_block].id = END;
    
    unsigned short fat_time, fat_date;
    get_fat_time(&fat_time, &fat_date);
    
    // 在当前目录的相应块中创建FCB
    int current_block = dir_block + free_block_offset;
    fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
    fcb *new_dir_fcb = &current_dir[free_entry_index];
    
    strcpy(new_dir_fcb->filename, name);
    strcpy(new_dir_fcb->exname, ext);
    new_dir_fcb->attribute = 0;
    new_dir_fcb->first = new_block;
    new_dir_fcb->length = BLOCKSIZE;
    new_dir_fcb->free = 1;
    new_dir_fcb->time = fat_time;
    new_dir_fcb->date = fat_date;

    // 初始化子目录
    fcb *sub_dir = (fcb *)(myhard + new_block * BLOCKSIZE);
    for (int i = 0; i < max_entries_per_block; i++) {
        sub_dir[i].free = 0;
        strcpy(sub_dir[i].filename, "");
        strcpy(sub_dir[i].exname, "");
        sub_dir[i].attribute = 0;
        sub_dir[i].time = 0;
        sub_dir[i].date = 0;
        sub_dir[i].first = 0;
        sub_dir[i].length = 0;
    }
    
    fcb *dot_sub = &sub_dir[0];
    strcpy(dot_sub->filename, ".");
    strcpy(dot_sub->exname, "");
    dot_sub->attribute = 0;
    dot_sub->first = new_block;
    dot_sub->length = BLOCKSIZE;
    dot_sub->free = 1;
    dot_sub->time = fat_time;
    dot_sub->date = fat_date;

    fcb *dotdot_sub = &sub_dir[1];
    strcpy(dotdot_sub->filename, "..");
    strcpy(dotdot_sub->exname, "");
    dotdot_sub->attribute = 0;
    dotdot_sub->first = dir_block;
    dotdot_sub->length = BLOCKSIZE;
    dotdot_sub->free = 1;
    dotdot_sub->time = fat_time;
    dotdot_sub->date = fat_date;

    printf("目录 %s 创建成功！起始块号: %d\n", dirname, new_block);
}

void my_ls(void) {
    int dir_block = get_current_dir_block();
    int is_root = (dir_block == ROOT_START);
    int block_count = is_root ? ROOT_BLOCKS : 1;
    
    printf("当前目录: %s (块号: %d-%d)\n", currentdir, dir_block, dir_block + block_count - 1);
    printf("文件名\t\t扩展名\t属性\t首块号\t长度\t块数\t创建时间\t\t状态\n");
    printf("--------------------------------------------------------------------------------\n");
    
    int total_count = 0;
    for (int block_offset = 0; block_offset < block_count; block_offset++) {
        int current_block = dir_block + block_offset;
        fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
        int max_entries = BLOCKSIZE / sizeof(fcb);
        
        for (int i = 0; i < max_entries; i++) {
            if (current_dir[i].free == 1) {
                char fullname[13];
                if (strlen(current_dir[i].exname) > 0)
                    sprintf(fullname, "%s.%s", current_dir[i].filename, current_dir[i].exname);
                else
                    strcpy(fullname, current_dir[i].filename);
                
                int block_count_f = 0;
                if (current_dir[i].attribute == 1 && current_dir[i].first != 0)
                    block_count_f = get_block_chain_length(current_dir[i].first);
                else if (current_dir[i].attribute == 0 && current_dir[i].first != 0)
                    block_count_f = 1;
                
                // 解析FAT时间格式
                unsigned short fat_time = current_dir[i].time;
                unsigned short fat_date = current_dir[i].date;
                
                int hour = (fat_time >> 11) & 0x1F;
                int minute = (fat_time >> 5) & 0x3F;
                int second = (fat_time & 0x1F) * 2;  // FAT时间以2秒为单位
                
                int year = 1980 + ((fat_date >> 9) & 0x7F);
                int month = (fat_date >> 5) & 0x0F;
                int day = fat_date & 0x1F;
                
                char time_str[20];
                snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d", 
                        year, month, day, hour, minute, second);
                
                printf("%-12s\t%-3s\t%s\t%d\t%lu\t%d\t%s\t%s\n",
                       fullname, current_dir[i].exname,
                       current_dir[i].attribute == 0 ? "目录" : "文件",
                       current_dir[i].first, current_dir[i].length, block_count_f,
                       time_str,
                       current_dir[i].free == 0 ? "空闲" : "已用");
                total_count++;
            }
        }
    }
    
    printf("--------------------------------------------------------------------------------\n");
    printf("总文件数: %d (块号: %d-%d)\n", total_count, dir_block, dir_block + block_count - 1);
}

int my_create(char *filename) {
    char name[9], ext[4];
    int result = parse_filename(filename, name, ext);
    if (result == -1) {
        printf("文件名不能超过8个字符！\n");
        return -1;
    }
    if (result == -2) {
        printf("扩展名不能超过3个字符！\n");
        return -1;
    }
    if (get_fcb_by_name_in_current_dir(filename, NULL) != NULL) {
        printf("文件 %s 已存在！\n", filename);
        return -1;
    }
    
    int dir_block = get_current_dir_block();
    int is_root = (dir_block == ROOT_START);
    int max_entries_per_block = BLOCKSIZE / sizeof(fcb);
    int free_index = -1;
    int free_block_offset = -1;
    int free_entry_index = -1;
    
    // 在多个块中查找空闲FCB
    for (int block_offset = 0; block_offset < (is_root ? ROOT_BLOCKS : 1); block_offset++) {
        int current_block = dir_block + block_offset;
        fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
        for (int i = 0; i < max_entries_per_block; i++) {
            if (current_dir[i].free == 0) {
                free_index = block_offset * max_entries_per_block + i;
                free_block_offset = block_offset;
                free_entry_index = i;
                break;
            }
        }
        if (free_index != -1) break;
    }
    
    if (free_index == -1) {
        printf("当前目录已满，无法创建文件！\n");
        return -1;
    }
    
    unsigned short fat_time, fat_date;
    get_fat_time(&fat_time, &fat_date);
    
    int current_block = dir_block + free_block_offset;
    fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
    fcb *new_file_fcb = &current_dir[free_entry_index];
    
    strcpy(new_file_fcb->filename, name);
    strcpy(new_file_fcb->exname, ext);
    new_file_fcb->attribute = 1;
    new_file_fcb->first = 0;
    new_file_fcb->length = 0;
    new_file_fcb->free = 1;
    new_file_fcb->time = fat_time;
    new_file_fcb->date = fat_date;
    
    printf("文件 %s 创建成功！位置: 块%d[%d]\n", filename, current_block, free_entry_index);
    return 0;
}

void my_rm(char *filename) {
    int index = -1;
    fcb *file = get_fcb_by_name_in_current_dir(filename, &index);
    if (file == NULL) {
        printf("文件 %s 不存在！\n", filename);
        return;
    }
    if (file->attribute == 0) {
        printf("%s 是一个目录，请使用 rmdir 删除目录\n", filename);
        return;
    }
    if (is_file_open(filename, currentdir)) {
        printf("文件 %s 正在被使用，请先关闭文件！\n", filename);
        return;
    }
    if (file->first != 0)
        free_block_chain(file->first);
    file->free = 0;
    strcpy(file->filename, "");
    strcpy(file->exname, "");
    file->first = 0;
    file->length = 0;
    printf("文件 %s 已删除！\n", filename);
}

void my_rmdir(char *dirname) {
    int index = -1;
    fcb *dir = get_fcb_by_name_in_current_dir(dirname, &index);
    if (dir == NULL) {
        printf("目录 %s 不存在！\n", dirname);
        return;
    }
    if (dir->attribute == 1) {
        printf("%s 是一个文件，请使用 rm 删除文件\n", dirname);
        return;
    }
    fcb *sub_dir = (fcb *)(myhard + dir->first * BLOCKSIZE);
    int max_entries = BLOCKSIZE / sizeof(fcb);
    int empty = 1;
    for (int i = 2; i < max_entries; i++) {
        if (sub_dir[i].free == 1) {
            empty = 0;
            break;
        }
    }
    if (!empty) {
        printf("目录 %s 不为空，不能删除！\n", dirname);
        return;
    }
    fat *fat1 = (fat *)(myhard + BLOCKSIZE);
    int block = dir->first;
    fat1[block].id = FREE;
    dir->free = 0;
    strcpy(dir->filename, "");
    strcpy(dir->exname, "");
    dir->first = 0;
    dir->length = 0;
    printf("目录 %s 已删除！\n", dirname);
}

int my_open(char *filename) {
    fcb *file = get_fcb_by_name_in_current_dir(filename, NULL);
    if (file == NULL) {
        printf("文件 %s 不存在！\n", filename);
        return -1;
    }
    if (file->attribute == 0) {
        printf("%s 是一个目录，不能打开！\n", filename);
        return -1;
    }
    int fd = find_free_fd();
    if (fd == -1) {
        printf("打开文件数已达上限！\n");
        return -1;
    }
    strcpy(openfilelist[fd].filename, file->filename);
    strcpy(openfilelist[fd].exname, file->exname);
    openfilelist[fd].attribute = file->attribute;
    openfilelist[fd].first = file->first;
    openfilelist[fd].length = file->length;
    openfilelist[fd].time = file->time;
    openfilelist[fd].date = file->date;
    strcpy(openfilelist[fd].dir, currentdir);
    openfilelist[fd].count = 0;
    openfilelist[fd].fcbstate = 0;
    openfilelist[fd].topenfile = 1;
    currfd = fd;
    printf("文件 %s 已打开，文件描述符: %d，文件长度: %lu\n", filename, fd, file->length);
    return fd;
}

void my_close(int fd) {
    if (fd < 0 || fd >= MAXOPENFILE) {
        printf("无效的文件描述符！\n");
        return;
    }
    if (openfilelist[fd].topenfile == 0) {
        printf("文件描述符 %d 未打开！\n", fd);
        return;
    }
    update_fcb(fd);
    char fullname[13];
    if (strlen(openfilelist[fd].exname) > 0)
        sprintf(fullname, "%s.%s", openfilelist[fd].filename, openfilelist[fd].exname);
    else
        strcpy(fullname, openfilelist[fd].filename);
    openfilelist[fd].topenfile = 0;
    printf("文件 %s (fd=%d) 已关闭\n", fullname, fd);
    if (currfd == fd) currfd = -1;
}

int my_write(int fd, int mode) {
    if (fd < 0 || fd >= MAXOPENFILE || openfilelist[fd].topenfile == 0) {
        printf("无效的文件描述符或文件未打开！\n");
        return -1;
    }
    char fullname[13];
    if (strlen(openfilelist[fd].exname) > 0)
        sprintf(fullname, "%s.%s", openfilelist[fd].filename, openfilelist[fd].exname);
    else
        strcpy(fullname, openfilelist[fd].filename);
    const char *mode_str = (mode == WRITE_TRUNCATE) ? "截断" : (mode == WRITE_OVERWRITE) ? "覆盖" : "追加";
    printf("使用%s写模式\n", mode_str);
    printf("请输入要写入的内容（输入.结束输入）:\n");
    char *buffer = NULL;
    int total_bytes = 0;
    int line_count = 0;
    while (1) {
        printf("> ");
        fflush(stdout);
        char line[BLOCKSIZE];
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 1 && line[0] == '.') break;
        int len = strlen(line);
        char *new_buffer = realloc(buffer, total_bytes + len + 2);
        if (!new_buffer) {
            printf("内存分配失败！\n");
            free(buffer);
            return -1;
        }
        buffer = new_buffer;
        if (line_count > 0) {
            buffer[total_bytes] = '\n';
            total_bytes++;
        }
        strcpy(buffer + total_bytes, line);
        total_bytes += len;
        line_count++;
    }
    if (total_bytes <= 0) {
        printf("没有内容可写入\n");
        if (buffer) free(buffer);
        return 0;
    }
    buffer[total_bytes] = '\0';
    int bytes_written = 0;
    switch (mode) {
        case WRITE_TRUNCATE:
            if (openfilelist[fd].first != 0)
                free_block_chain(openfilelist[fd].first);
            openfilelist[fd].first = 0;
            if (total_bytes > 0) {
                int blocks_needed = (total_bytes + BLOCKSIZE - 1) / BLOCKSIZE;
                int first_block = 0;
                if (allocate_blocks(blocks_needed, &first_block) == blocks_needed) {
                    openfilelist[fd].first = first_block;
                    bytes_written = write_to_block_chain(first_block, buffer, total_bytes, 0, 0);
                    if (bytes_written > 0) {
                        openfilelist[fd].length = bytes_written;
                        openfilelist[fd].count = 0;
                    }
                } else {
                    printf("磁盘空间不足！\n");
                }
            } else {
                openfilelist[fd].length = 0;
            }
            break;
        case WRITE_OVERWRITE:
            if (openfilelist[fd].first == 0 && total_bytes > 0) {
                int blocks_needed = (total_bytes + BLOCKSIZE - 1) / BLOCKSIZE;
                int first_block = 0;
                if (allocate_blocks(blocks_needed, &first_block) != blocks_needed) {
                    printf("磁盘空间不足！\n");
                    free(buffer);
                    return -1;
                }
                openfilelist[fd].first = first_block;
            }
            if (openfilelist[fd].first != 0) {
                bytes_written = write_to_block_chain(openfilelist[fd].first, buffer, total_bytes, 0, 0);
                if (bytes_written > 0) {
                    if (bytes_written > openfilelist[fd].length)
                        openfilelist[fd].length = bytes_written;
                    openfilelist[fd].count = 0;
                }
            } else {
                printf("文件没有分配块，且写入长度为0！\n");
            }
            break;
        case WRITE_APPEND:
            if (openfilelist[fd].first == 0 && total_bytes > 0) {
                int blocks_needed = (total_bytes + BLOCKSIZE - 1) / BLOCKSIZE;
                int first_block = 0;
                if (allocate_blocks(blocks_needed, &first_block) != blocks_needed) {
                    printf("磁盘空间不足！\n");
                    free(buffer);
                    return -1;
                }
                openfilelist[fd].first = first_block;
            }
            if (openfilelist[fd].first != 0) {
                bytes_written = write_to_block_chain(openfilelist[fd].first, buffer, total_bytes,
                                                     openfilelist[fd].length, 1);
                if (bytes_written > 0) {
                    openfilelist[fd].length += bytes_written;
                    openfilelist[fd].count = 0;
                }
            } else {
                printf("文件没有分配块，且追加长度为0！\n");
            }
            break;
        default:
            printf("无效的写入模式！\n");
            free(buffer);
            return -1;
    }
    free(buffer);
    if (bytes_written > 0) {
        unsigned short fat_time, fat_date;
        get_fat_time(&fat_time, &fat_date);
        openfilelist[fd].time = fat_time;
        openfilelist[fd].date = fat_date;
        openfilelist[fd].fcbstate = 1;
    }
    printf("%s模式: 写入 %d 字节到文件 %s (fd=%d)\n", mode_str, bytes_written, fullname, fd);
    return bytes_written;
}

int my_read(int fd, int len) {
    if (fd < 0 || fd >= MAXOPENFILE || openfilelist[fd].topenfile == 0) {
        printf("无效的文件描述符或文件未打开！\n");
        return -1;
    }
    if (len <= 0) {
        printf("读取长度必须大于0！\n");
        return -1;
    }
    if (openfilelist[fd].length == 0) {
        printf("文件为空！\n");
        return 0;
    }
    int read_len = (len < openfilelist[fd].length) ? len : openfilelist[fd].length;
    char *buffer = (char *)malloc(read_len + 1);
    if (!buffer) {
        printf("内存分配失败！\n");
        return -1;
    }
    int bytes_read = read_from_block_chain(openfilelist[fd].first, buffer, 0, read_len);
    buffer[bytes_read] = '\0';
    char fullname[13];
    if (strlen(openfilelist[fd].exname) > 0)
        sprintf(fullname, "%s.%s", openfilelist[fd].filename, openfilelist[fd].exname);
    else
        strcpy(fullname, openfilelist[fd].filename);
    printf("从文件 %s (fd=%d) 读取 %d 字节:\n", fullname, fd, bytes_read);
    printf("--------------------------------\n%s\n--------------------------------\n", buffer);
    free(buffer);
    return bytes_read;
}

void my_cd(char *dirname) {
    if (strcmp(dirname, "..") == 0) {
        if (strcmp(currentdir, "/") == 0) {
            printf("已经在根目录！\n");
        } else {
            int dir_block = get_current_dir_block();
            int is_root = (dir_block == ROOT_START);
            int max_entries_per_block = BLOCKSIZE / sizeof(fcb);
            
            for (int block_offset = 0; block_offset < (is_root ? ROOT_BLOCKS : 1); block_offset++) {
                int current_block = dir_block + block_offset;
                fcb *current_dir = (fcb *)(myhard + current_block * BLOCKSIZE);
                for (int i = 0; i < max_entries_per_block; i++) {
                    if (current_dir[i].free == 1 && 
                        strcmp(current_dir[i].filename, "..") == 0 &&
                        strcmp(current_dir[i].exname, "") == 0) {
                        int parent_block = current_dir[i].first;
                        set_current_dir_block(parent_block);
                        
                        char *last_slash = strrchr(currentdir, '/');
                        if (last_slash != NULL) {
                            if (last_slash == currentdir) {
                                strcpy(currentdir, "/");
                            } else {
                                *last_slash = '\0';
                            }
                        }
                        printf("切换到目录: %s (块号: %d)\n", currentdir, parent_block);
                        return;
                    }
                }
            }
            printf("无法找到父目录！\n");
        }
    } else if (strcmp(dirname, ".") == 0) {
        printf("当前目录: %s (块号: %d)\n", currentdir, get_current_dir_block());
    } else {
        fcb *dir = get_fcb_by_name_in_current_dir(dirname, NULL);
        if (dir == NULL) {
            printf("目录 %s 不存在！\n", dirname);
            return;
        }
        if (dir->attribute == 1) {
            printf("%s 是一个文件，不是目录！\n", dirname);
            return;
        }
        int new_dir_block = dir->first;
        set_current_dir_block(new_dir_block);
        char new_dir[MAX_PATH_LEN];
       int result;
        
        if (strcmp(currentdir, "/") == 0) {
            result = snprintf(new_dir, sizeof(new_dir), "/%s", dirname);
        } else {
            result = snprintf(new_dir, sizeof(new_dir), "%s/%s", currentdir, dirname);
        }
        
        if (result < 0 || result >= (int)sizeof(new_dir)) {
            printf("路径过长！最大允许长度: %zu\n", sizeof(new_dir) - 1);
            printf("当前路径: %s\n", currentdir);
            printf("目标目录: %s\n", dirname);
            printf("总长度: %d (最大允许: %zu)\n", result, sizeof(new_dir) - 1);
            return;
        }
        strcpy(currentdir, new_dir);
        printf("切换到目录: %s (块号: %d)\n", currentdir, new_dir_block);
    }
}

void my_exitsys() {
    for (int i = 0; i < MAXOPENFILE; i++)
        if (openfilelist[i].topenfile == 1) my_close(i);
    FILE *fp = fopen("disk.img", "wb");
    if (fp != NULL) {
        fwrite(myhard, 1, SIZE, fp);
        fclose(fp);
        printf("虚拟磁盘已保存到 disk.img\n");
    } else {
        printf("无法保存虚拟磁盘文件！\n");
    }
    free(myhard);
    printf("文件系统已退出。\n");
}

int main() {
    startsys();
    char command[80], arg1[80];
    int arg2, arg3;
    while (1) {
        printf("\n[%s] $ ", currentdir);
        if (scanf("%s", command) != 1) break;
        if (strcmp(command, "help") == 0) {
            my_help();
        } else if (strcmp(command, "mkdir") == 0) {
            scanf("%s", arg1); my_mkdir(arg1);
        } else if (strcmp(command, "ls") == 0) {
            my_ls();
        } else if (strcmp(command, "create") == 0) {
            scanf("%s", arg1); my_create(arg1);
        } else if (strcmp(command, "rm") == 0) {
            scanf("%s", arg1); my_rm(arg1);
        } else if (strcmp(command, "rmdir") == 0) {
            scanf("%s", arg1); my_rmdir(arg1);
        } else if (strcmp(command, "open") == 0) {
            scanf("%s", arg1); int fd = my_open(arg1); if (fd >= 0) currfd = fd;
        } else if (strcmp(command, "close") == 0) {
            char next = getchar();
            if (next == '\n') {
                if (currfd >= 0) my_close(currfd);
                else printf("当前没有打开的文件！\n");
            } else {
                ungetc(next, stdin);
                scanf("%d", &arg2); my_close(arg2);
            }
        } else if (strcmp(command, "write") == 0) {
            int write_fd = currfd;
            char next = getchar();
            if (next != '\n') {
                ungetc(next, stdin);
                if (scanf("%d", &write_fd) != 1) {
                    printf("无效的文件描述符！\n");
                    while ((next = getchar()) != '\n' && next != EOF);
                    continue;
                }
            }
            if (write_fd < 0) {
                printf("当前没有打开的文件！\n");
                while ((next = getchar()) != '\n' && next != EOF);
                continue;
            }
            int mode = get_write_mode();
            my_write(write_fd, mode);
        } else if (strcmp(command, "read") == 0) {
            char next = getchar();
            if (next == '\n') {
                printf("用法: read [fd] [len] 或 read [len]\n");
            } else {
                ungetc(next, stdin);
                if (scanf("%d", &arg2) == 1) {
                    next = getchar();
                    if (next == '\n') {
                        if (currfd >= 0) my_read(currfd, arg2);
                        else printf("当前没有打开的文件！\n");
                    } else {
                        if (scanf("%d", &arg3) == 1) my_read(arg2, arg3);
                        else printf("参数错误！\n");
                    }
                } else {
                    printf("参数错误！\n");
                }
            }
        } else if (strcmp(command, "openlist") == 0) {
            show_open_files();
        } else if (strcmp(command, "cd") == 0) {
            scanf("%s", arg1); my_cd(arg1);
        } else if (strcmp(command, "format") == 0) {
            printf("确定要格式化磁盘吗？(y/n): ");
            char confirm; scanf(" %c", &confirm);
            if (confirm == 'y' || confirm == 'Y') my_format();
        } else if (strcmp(command, "exit") == 0) {
            printf("确定要退出吗？(y/n): ");
            char confirm; scanf(" %c", &confirm);
            if (confirm == 'y' || confirm == 'Y') { my_exitsys(); break; }
        } else {
            printf("未知命令: %s\n", command);
            printf("输入 'help' 查看可用命令\n");
        }
        int c; while ((c = getchar()) != '\n' && c != EOF);
    }
    return 0;
}
