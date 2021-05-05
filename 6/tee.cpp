/**
 * @file tee.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-01
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf( "usage: %s <file>\n", argv[0] );
        return 1;
    }
    int filefd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);  //O_TRUNC如果文件存在，并且以只写/读写方式打开，则清空文件全部内容
    assert(filefd > 0);

    int pipefd_stdout[2];
    int ret = pipe(pipefd_stdout);
    assert(ret != -1);

    int pipefd_file[2];
    ret = pipe(pipefd_file);
    assert(ret != -1);

    //char input_message[100] = "Hello World!\n";
	//write(STDIN_FILENO, input_message, sizeof(input_message));

    close(STDIN_FILENO);
	  dup2(pipefd_stdout[1], STDIN_FILENO);
	  write(pipefd_stdout[1], "abc\n", 4);

    /* 将标准输入内容输入管道 pipfd_stdout */
    ret = splice(STDIN_FILENO, NULL, pipefd_stdout[1], NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
    assert(ret != -1);                            // 原因未知
    /* 将管道pipefd_stdout的输出复制到管道pipfd_file的输入端 */
    ret = tee(pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK);
    assert(ret != -1);
    /* 将管道pipefd_file的输出定向到文件描述符filefd上，从而将标准输入内容写入文件 */
    ret = splice(pipefd_file[0], NULL, filefd, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
    assert(ret != -1);
    /* 将管道pipefd_file的输出定向到标准输出，其内容和写入文件的内容完全一致 */
    ret = splice(pipefd_stdout[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);

    close(filefd);
    for (size_t i = 0; i <= 1; ++i) {
        close(pipefd_stdout[i]);
        close(pipefd_file[i]);
    }
    return 0;
}
