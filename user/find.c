#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char* path, char* target_filename) {
    char buf[256],*p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf)-1;
    if (*p != '/'){
        p++;
        *p++ = '/'; // 加斜杠
    }
    // 通过 read 函数从文件描述符 fd 中读取目录项
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || strcmp(de.name,".") == 0 || strcmp(de.name,"..") == 0){
            continue;
        } 
        memmove(p, de.name,sizeof(de.name));     
        if (stat(buf, &st) < 0) {
            fprintf(2, "find: cannot stat %s\n", buf);
            close(fd);
            continue;
        }
        // printf("buf is %s,type is %d\n",buf,st.type);
        switch (st.type){
            case T_DIR:{
                if(strcmp(target_filename, de.name) == 0){
                    printf("%s\n",buf);
                }
                find(buf, target_filename);
                break;
            }
            case T_FILE:{
                if(strcmp(target_filename, de.name) == 0){
                    printf("%s\n",buf);
                }
                break;
            }
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc == 3) {
        find(argv[1], argv[2]);
    }else{
        printf("find: find needs path and filename!\n");
    }
    exit(0);
}
