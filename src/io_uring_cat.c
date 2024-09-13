#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define QUEUE_SIZE 1
#define BLOCK_SZ 1024

struct file_info {
  __off_t file_size;
  struct iovec iovecs[];
};

__off_t get_file_size(const int file_fd) {
  struct stat file_stat;

  if (fstat(file_fd, &file_stat) == -1) {
    perror("fstat");
    return -1;
  }

  if (S_ISBLK(file_stat.st_mode)) {
    unsigned long long bytes = 0;
    if (ioctl(file_fd, BLKGETSIZE64, &bytes) != 0) {
      perror("ioctl");
      return -1;
    }
    return bytes;
  }

  if (S_ISREG(file_stat.st_mode)) {
    return file_stat.st_size;
  }

  return -1;
}

int submit_read_request(char *const file, struct io_uring *const uring) {
  const int file_fd = open(file, O_RDONLY);
  if (file_fd == -1) {
    perror("open");
    return 1;
  }

  const __off_t file_size = get_file_size(file_fd);
  if (file_size == -1) {
    fprintf(stderr, "Error fetching file size\n");
    return 1;
  }

  // 应当有溢出检测
  __off_t remaining = file_size;
  __off_t offset = 0;
  int current_block = 0;
  unsigned int blocks = file_size / BLOCK_SIZE;
  if (file_size % BLOCK_SIZE != 0) {
    blocks++;
  }

  struct file_info *f_info =
      malloc(sizeof(struct file_info) + sizeof(struct iovec) * blocks);

  while (remaining != 0) {
    __off_t bytes_to_read = remaining;
    if (bytes_to_read > BLOCK_SIZE) {
      bytes_to_read = BLOCK_SIZE;
    }

    offset += bytes_to_read;
    f_info->iovecs[current_block].iov_len = bytes_to_read;

    void *buf;
    if (posix_memalign(&buf, BLOCK_SIZE, BLOCK_SIZE) != 0) {
      perror("posix_memalign");
      free(f_info);
      return 1;
    }
    f_info->iovecs[current_block].iov_base = buf;

    current_block++;
    remaining -= bytes_to_read;
  }

  f_info->file_size = file_size;

  struct io_uring_sqe *sqe = io_uring_get_sqe(uring);

  io_uring_prep_readv(sqe, file_fd, f_info->iovecs, blocks, 0);
  io_uring_sqe_set_data(sqe, f_info);
  io_uring_submit(uring);

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s [file name] <[file name] ...>\n", argv[0]);
    return 1;
  }

  // io_uring 结构体
  struct io_uring ring;

  // 初始化结构体
  io_uring_queue_init(QUEUE_SIZE, &ring, 0);

  for (int i = 1; i < argc; i++) {
    int ret = submit_read_request(argv[i], &ring);
    if (ret) {
      fprintf(stderr, "Error reading file: %s\n", argv[i]);
      return 1;
    }
  }

  io_uring_queue_exit(&ring);
  return 0;
}
