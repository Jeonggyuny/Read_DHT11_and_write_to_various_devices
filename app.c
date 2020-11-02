#include <stdio.h> /* fprintf() */
#include <stdlib.h> /* exit() */
#include <string.h> /* strlen() */
#include <fcntl.h> /* open() */
#include <unistd.h> /* read(), write(), lseek(), close(), sleep() */

#define DHT11_TEMP_FILE_PATH "/sys/bus/iio/devices/iio:device0/in_temp_input"
#define DHT11_HUMI_FILE_PATH "/sys/bus/iio/devices/iio:device0/in_humidityrelative_input"
#define RYGLEDS_FILE_PATH "/sys/class/RYGleds_class/RYGleds_dev/temperature"
#define BUZZER_FILE_PATH "/sys/class/buzzer_class/buzzer_dev/temperature"
#define LCD1602_TEMP_FILE_PATH "/sys/devices/platform/soc/soc:my_lcd1602/temperature"
#define LCD1602_HUMI_FILE_PATH "/sys/devices/platform/soc/soc:my_lcd1602/humidity"

#define BUF_SIZE 1024

int main(void)
{
	int dht11_temp_fd, dht11_humi_fd, RYGleds_fd, buzzer_fd, lcd1602_temp_fd, lcd1602_humi_fd;
	ssize_t num_read, num_write;
	char temp_buf[BUF_SIZE], humi_buf[BUF_SIZE];

	dht11_temp_fd = open(DHT11_TEMP_FILE_PATH, O_RDONLY);
	if (dht11_temp_fd == -1) {
		fprintf(stderr, "Fail to open file: %s\n", DHT11_TEMP_FILE_PATH);

		exit(EXIT_FAILURE);
	}

	dht11_humi_fd = open(DHT11_HUMI_FILE_PATH, O_RDONLY);
	if (dht11_humi_fd == -1) {
		fprintf(stderr, "Fail to open file: %s\n", DHT11_HUMI_FILE_PATH);

		close(dht11_temp_fd);

		exit(EXIT_FAILURE);
	}

	RYGleds_fd = open(RYGLEDS_FILE_PATH, O_WRONLY);
	if (RYGleds_fd == -1) {
		fprintf(stderr, "Fail to open file: %s\n", RYGLEDS_FILE_PATH);

		close(dht11_humi_fd);
		close(dht11_temp_fd);

		exit(EXIT_FAILURE);
	}

	buzzer_fd = open(BUZZER_FILE_PATH, O_WRONLY);
	if (buzzer_fd == -1) {
		fprintf(stderr, "Fail to open file: %s\n", BUZZER_FILE_PATH);

		close(RYGleds_fd);
		close(dht11_humi_fd);
		close(dht11_temp_fd);

		exit(EXIT_FAILURE);
	}

	lcd1602_temp_fd = open(LCD1602_TEMP_FILE_PATH, O_WRONLY);
	if (lcd1602_temp_fd == -1) {
		fprintf(stderr, "Fail to open file: %s\n", LCD1602_TEMP_FILE_PATH);

		close(buzzer_fd);
		close(RYGleds_fd);
		close(dht11_humi_fd);
		close(dht11_temp_fd);

		exit(EXIT_FAILURE);
	}

	lcd1602_humi_fd = open(LCD1602_HUMI_FILE_PATH, O_WRONLY);
	if (lcd1602_humi_fd == -1) {
		fprintf(stderr, "Fail to open file: %s\n", LCD1602_HUMI_FILE_PATH);

		close(lcd1602_temp_fd);
		close(buzzer_fd);
		close(RYGleds_fd);
		close(dht11_humi_fd);
		close(dht11_temp_fd);

		exit(EXIT_FAILURE);
	}

	while (1) {
		sleep(5);

		num_read = read(dht11_temp_fd, temp_buf, BUF_SIZE);
		if (num_read == -1) {
			fprintf(stderr, "Fail to read file: %s\n", DHT11_TEMP_FILE_PATH);

			close(lcd1602_humi_fd);
			close(lcd1602_temp_fd);
			close(buzzer_fd);
			close(RYGleds_fd);
			close(dht11_humi_fd);
			close(dht11_temp_fd);

			exit(EXIT_FAILURE);
		}
		temp_buf[num_read - 1] = '\0';
		lseek(dht11_temp_fd, -num_read, SEEK_CUR);

		num_read = read(dht11_humi_fd, humi_buf, BUF_SIZE);
		if (num_read == -1) {
			fprintf(stderr, "Fail to read file: %s\n", DHT11_HUMI_FILE_PATH);

			close(lcd1602_humi_fd);
			close(lcd1602_temp_fd);
			close(buzzer_fd);
			close(RYGleds_fd);
			close(dht11_humi_fd);
			close(dht11_temp_fd);

			exit(EXIT_FAILURE);
		}
		humi_buf[num_read - 1] = '\0';
		lseek(dht11_humi_fd, -num_read, SEEK_CUR);

		num_write = write(RYGleds_fd, temp_buf, (size_t)strlen(temp_buf));
		if (num_write == -1) {
			fprintf(stderr, "Fail to write file: %s\n", RYGLEDS_FILE_PATH);

			close(lcd1602_humi_fd);
			close(lcd1602_temp_fd);
			close(buzzer_fd);
			close(RYGleds_fd);
			close(dht11_humi_fd);
			close(dht11_temp_fd);

			exit(EXIT_FAILURE);
		}

		num_write = write(buzzer_fd, temp_buf, (size_t)strlen(temp_buf));
		if (num_write == -1) {
			fprintf(stderr, "Fail to write file: %s\n", BUZZER_FILE_PATH);

			close(lcd1602_humi_fd);
			close(lcd1602_temp_fd);
			close(buzzer_fd);
			close(RYGleds_fd);
			close(dht11_humi_fd);
			close(dht11_temp_fd);

			exit(EXIT_FAILURE);
		}

		num_write = write(lcd1602_temp_fd, temp_buf, (size_t)strlen(temp_buf));
		if (num_write == -1) {
			fprintf(stderr, "Fail to write file: %s\n", LCD1602_TEMP_FILE_PATH);

			close(lcd1602_humi_fd);
			close(lcd1602_temp_fd);
			close(buzzer_fd);
			close(RYGleds_fd);
			close(dht11_humi_fd);
			close(dht11_temp_fd);

			exit(EXIT_FAILURE);
		}

		num_write = write(lcd1602_humi_fd, humi_buf, (size_t)strlen(humi_buf));
		if (num_write == -1) {
			fprintf(stderr, "Fail to write file: %s\n", LCD1602_HUMI_FILE_PATH);

			close(lcd1602_humi_fd);
			close(lcd1602_temp_fd);
			close(buzzer_fd);
			close(RYGleds_fd);
			close(dht11_humi_fd);
			close(dht11_temp_fd);

			exit(EXIT_FAILURE);
		}
	}

	return EXIT_SUCCESS;
}
