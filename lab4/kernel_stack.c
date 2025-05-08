/*
 * kernel_stack.c
 * Simple CLI for int_stack kernel module
 * Author: Nikita Sannikov
 *
 * Usage:
 *   kernel_stack set-size N
 *   kernel_stack push VALUE
 *   kernel_stack pop
 *   kernel_stack unwind
 *
 * Returns 0 on success, errno on error.
 */

 #include <stdio.h>      // printf, fprintf
 #include <stdlib.h>     // strtol
 #include <string.h>     // strcmp, strerror
 #include <fcntl.h>      // open
 #include <unistd.h>     // close, read, write
 #include <errno.h>      // errno
 #include <sys/ioctl.h>  // ioctl
 
 #define DEV_PATH "/dev/int_stack"
 #define MAGIC 's'
 #define SET_SIZE _IOW(MAGIC, 1, unsigned int)
 
 static void show_usage(const char *prog)
 {
     fprintf(stderr,
         "Usage: %s <cmd> [arg]\n"
         "  set-size N    set max entries (N>0)\n"
         "  push VAL      push integer VAL\n"
         "  pop           pop and print one (or NULL)\n"
         "  unwind        pop until empty\n",
         prog);
 }
 
 int main(int argc, char *argv[])
 {
     if (argc < 2) {
         show_usage(argv[0]);
         return EINVAL;
     }
 
     const char *cmd = argv[1];
     int fd, ret;
 
     if (!strcmp(cmd, "set-size")) {
         if (argc != 3) { show_usage(argv[0]); return EINVAL; }
         char *end;
         long n = strtol(argv[2], &end, 10);
         if (*end != '\0' || n <= 0) {
             fprintf(stderr, "ERROR: size should be > 0\n");
             return EINVAL;
         }
         unsigned int sz = n;
 
         fd = open(DEV_PATH, O_RDWR);
         if (fd < 0) { perror("open"); return errno; }
         ret = ioctl(fd, SET_SIZE, &sz);
         if (ret < 0) {
             if (errno == EINVAL)
                 fprintf(stderr, "ERROR: size must be >0\n");
             else if (errno == ENOTTY)
                 fprintf(stderr, "ERROR: ioctl not supported\n");
             else
                 fprintf(stderr, "ERROR: ioctl failed: %s\n", strerror(errno));
             close(fd);
             return errno;
         }
         close(fd);
         return 0;
 
     } else if (!strcmp(cmd, "push")) {
         if (argc != 3) { show_usage(argv[0]); return EINVAL; }
         char *end;
         long v = strtol(argv[2], &end, 10);
         if (*end) { fprintf(stderr, "ERROR: bad int '%s'\n", argv[2]); return EINVAL; }
         int val = v;
 
         fd = open(DEV_PATH, O_RDWR);
         if (fd < 0) { perror("open"); return errno; }
         ssize_t w = write(fd, &val, sizeof(val));
         if (w < 0) {
             if (errno == ERANGE)
                 fprintf(stderr, "ERROR: stack is full\n");
             else
                 fprintf(stderr, "ERROR: write err: %s\n", strerror(errno));
             close(fd);
             return errno;
         }
         close(fd);
         return 0;
 
     } else if (!strcmp(cmd, "pop")) {
         if (argc != 2) { show_usage(argv[0]); return EINVAL; }
         fd = open(DEV_PATH, O_RDWR);
         if (fd < 0) { perror("open"); return errno; }
         int val;
         ssize_t r = read(fd, &val, sizeof(val));
         if (r < 0) {
             fprintf(stderr, "ERROR: read err: %s\n", strerror(errno));
             close(fd);
             return errno;
         }
         if (r == 0) {
             printf("NULL\n");
         } else {
             printf("%d\n", val);
         }
         close(fd);
         return 0;
 
     } else if (!strcmp(cmd, "unwind")) {
         if (argc != 2) { show_usage(argv[0]); return EINVAL; }
         fd = open(DEV_PATH, O_RDWR);
         if (fd < 0) { perror("open"); return errno; }
         int val;
         while (1) {
             ssize_t r = read(fd, &val, sizeof(val));
             if (r < 0) {
                 fprintf(stderr, "ERROR: read err: %s\n", strerror(errno));
                 close(fd);
                 return errno;
             }
             if (r == 0) break;
             printf("%d\n", val);
         }
         close(fd);
         return 0;
 
     } else {
         show_usage(argv[0]);
         return EINVAL;
     }
 }
 