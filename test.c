/*
* Name : output_test.c
* Author : Vu Nguyen <quangngmetro@gmail.com>
* Version : 0.1
* Copyright : GPL
* Description : This is a test application which is used for testing
* GPIO output functionality of the raspi-gpio Linux device driver
* implemented for Raspberry Pi revision B platform. The test
* application first sets all the GPIO pins on the Raspberry Pi to
* output, then it sets all the GPIO pins to "high"/"low" logic
* level based on the options passed to the program from the command
* line
* Usage example:
* ./output_test 1 // Set all GPIO pins to output, high state
* ./output_test 0 // Set all GPIO pins to output, low state
*/
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#define NUM_GPIO_PINS 3
#define MAX_GPIO_NUMBER 4
#define BUF_SIZE 4
#define PATH_SIZE 20
int main(int argc, char **argv)
{
    char path[PATH_SIZE], buf[BUF_SIZE];
    int i = 0, fd = 0;
    snprintf(path, sizeof(path), "/dev/LED_0");
    fd = open(path, O_WRONLY);
    if (fd < 0) {
	perror("Error opening GPIO pin");
	exit(EXIT_FAILURE);
    }

    printf("Set GPIO pins to output, logic level :\n");
    strncpy(buf, "1", 1);
    buf[1] = '\0';
    if (write(fd, buf, sizeof(buf)) < 0) {
	perror("write, set pin output");
	exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}
