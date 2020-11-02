#include <linux/module.h>
#include <linux/platform_device.h> /* platform_driver_register(), platform_set_drvdata() */
#include <linux/kernel.h> /* kstrtol() */
#include <linux/gpio/consumer.h> /* devm_gpiod_get_index() */
#include <linux/delay.h> /* msleep(), msleep(), mdelay() */
#include <linux/workqueue.h> /* INIT_WORK() */

struct lcd1602
{
	struct device *dev;
	struct platform_device * pdev;

	struct gpio_desc *d4, *d5, *d6, *d7;
	struct gpio_desc *rs, *rw, *e;

	struct work_struct work;
};

#define LOW 0
#define HIGH 1

#define DRIVER_NAME "my_lcd1602"

static int Temperature = 0;
static int Humidity = 0;

static ssize_t set_temperature(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	// struct lcd1602 *lcd1602 = dev_get_platdata(dev);
	long temperature_value = 0;

	dev_info(dev, "[+] set_temperature enter\n");

	if (kstrtol(buf, 10, &temperature_value) < 0) {
		return -EINVAL;
	}

	Temperature = temperature_value;

	// schedule_work(&lcd1602->work);

	dev_info(dev, "[+] set_temperature exit\n");

	return count;
}
static DEVICE_ATTR(temperature, S_IWUSR, NULL, set_temperature);

static ssize_t set_humidity(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct lcd1602 *lcd1602 = dev_get_drvdata(dev);
	long humidity_value = 0;

	dev_info(dev, "[+] set_humidity enter\n");

	if (kstrtol(buf, 10, &humidity_value) < 0) {
		return -EINVAL;
	}

	Humidity = humidity_value;

	schedule_work(&lcd1602->work);

	dev_info(dev, "[+] set_humidity exit\n");

	return count;
}
static DEVICE_ATTR(humidity, S_IWUSR, NULL, set_humidity);

static inline void __lcd1602_set_inst_value(struct platform_device *pdev,
		unsigned rs_val, unsigned rw_val, unsigned e_val)
{
	struct lcd1602 *lcd1602 = platform_get_drvdata(pdev);

	gpiod_set_value(lcd1602->rs, rs_val);
	gpiod_set_value(lcd1602->rw, rw_val);
	gpiod_set_value(lcd1602->e, e_val);
}

static void lcd1602_write(struct platform_device *pdev, u8 val)
{
	struct lcd1602 *lcd1602 = platform_get_drvdata(pdev);

	__lcd1602_set_inst_value(pdev, HIGH, LOW, LOW); /* Data mode, Write mode */
	udelay(1);

	__lcd1602_set_inst_value(pdev, HIGH, LOW, HIGH); /* Data mode, Write mode */
	gpiod_set_value(lcd1602->d4, val & 0b00010000);
	gpiod_set_value(lcd1602->d5, val & 0b00100000);
	gpiod_set_value(lcd1602->d6, val & 0b01000000);
	gpiod_set_value(lcd1602->d7, val & 0b10000000);
	__lcd1602_set_inst_value(pdev, HIGH, LOW, LOW); /* Data mode, Write mod */
	udelay(2);

	__lcd1602_set_inst_value(pdev, HIGH, LOW, HIGH); /* Data mode, Write mode */
	gpiod_set_value(lcd1602->d4, val & 0b00000001);
	gpiod_set_value(lcd1602->d5, val & 0b00000010);
	gpiod_set_value(lcd1602->d6, val & 0b00000100);
	gpiod_set_value(lcd1602->d7, val & 0b00001000);
	__lcd1602_set_inst_value(pdev, HIGH, LOW, LOW); /* Data mode, Write mode */
	mdelay(1);
}

static void lcd1602_inst(struct platform_device *pdev, u8 val)
{
	struct lcd1602 *lcd1602 = platform_get_drvdata(pdev);

	__lcd1602_set_inst_value(pdev, LOW, LOW, LOW); /* Inst mode, Write mode */
	udelay(1);

	__lcd1602_set_inst_value(pdev, LOW, LOW, HIGH); /* Inst mode, Write mode */
	gpiod_set_value(lcd1602->d4, val & 0b00010000);
	gpiod_set_value(lcd1602->d5, val & 0b00100000);
	gpiod_set_value(lcd1602->d6, val & 0b01000000);
	gpiod_set_value(lcd1602->d7, val & 0b10000000);
	__lcd1602_set_inst_value(pdev, LOW, LOW, LOW); /* Inst mode, Write mode */
	udelay(2);

	__lcd1602_set_inst_value(pdev, LOW, LOW, HIGH); /* Inst mode, Write mode */
	gpiod_set_value(lcd1602->d4, val & 0b00000001);
	gpiod_set_value(lcd1602->d5, val & 0b00000010);
	gpiod_set_value(lcd1602->d6, val & 0b00000100);
	gpiod_set_value(lcd1602->d7, val & 0b00001000);
	__lcd1602_set_inst_value(pdev, LOW, LOW, LOW); /* Inst mode, Write mode */
	mdelay(1);
}

static void lcd1602_puts(struct platform_device *pdev, char *ptr)
{
	while (*ptr != NULL) {
		lcd1602_write(pdev, *ptr++);
	}
}

static void lcd1602_work(struct work_struct *work)
{
	struct lcd1602 *lcd1602 = container_of(work, struct lcd1602, work);

	dev_info(lcd1602->dev, "[+] lcd1602_work enter\n");

	char s1[16], s2[16];

	sprintf(s1, "Temperature: %d", Temperature);
	sprintf(s2, "Humidity: %d", Humidity);

	msleep(50);
	lcd1602_inst(lcd1602->pdev, 0x01); /* Clear display */

	msleep(2);
	lcd1602_inst(lcd1602->pdev, 0x80); /* Move cursor position using DDRAM (00) */
	lcd1602_puts(lcd1602->pdev, s1); /* Display string */
	lcd1602_inst(lcd1602->pdev, 0xc0); /* Move cursor position using DDRAM (40) */
	lcd1602_puts(lcd1602->pdev, s2); /* Display string */

	dev_info(lcd1602->dev, "[+] lcd1602_work exit\n");
}

static const struct of_device_id lcd1602_dt_ids[] = {
	{ .compatible = "arrow,my_lcd1602", },
	{ }
};
MODULE_DEVICE_TABLE(of, lcd1602_dt_ids);

static int lcd1602_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lcd1602 *lcd1602;

	int ret;

	dev_info(dev, "[+] lcd1602_probe enter");

	lcd1602 = devm_kmalloc(dev, sizeof(struct lcd1602), GFP_KERNEL);
	lcd1602->dev = dev;
	lcd1602->pdev = pdev;
	lcd1602->d4 = devm_gpiod_get_index(dev, "lcd1602", 0, GPIOD_OUT_LOW); /* D4 */
	lcd1602->d5 = devm_gpiod_get_index(dev, "lcd1602", 1, GPIOD_OUT_LOW); /* D5 */
	lcd1602->d6 = devm_gpiod_get_index(dev, "lcd1602", 2, GPIOD_OUT_LOW); /* D6 */
	lcd1602->d7 = devm_gpiod_get_index(dev, "lcd1602", 3, GPIOD_OUT_LOW); /* D7 */
	lcd1602->rs = devm_gpiod_get_index(dev, "lcd1602", 4, GPIOD_OUT_LOW); /* RS */
	lcd1602->rw = devm_gpiod_get_index(dev, "lcd1602", 5, GPIOD_OUT_LOW); /* R/W */
	lcd1602->e = devm_gpiod_get_index(dev, "lcd1602", 6, GPIOD_OUT_LOW); /* E */
	INIT_WORK(&lcd1602->work, lcd1602_work);

	platform_set_drvdata(pdev, lcd1602);

	msleep(50);
	lcd1602_inst(pdev, 0x28); /* 4-Bits, 2-Lines, 5x8 Dots */
	lcd1602_inst(pdev, 0x0c); /* Display ON, Cursor OFF, Blinking cursor OFF */
	lcd1602_inst(pdev, 0x06); /* Assign cursor moving direction */
	lcd1602_inst(pdev, 0x01); /* Clear display */

	msleep(2);
	lcd1602_inst(pdev, 0x80); /* Move cursor position using DDRAM (00) */
	lcd1602_puts(pdev, "Temperature: xx"); /* Display string */
	lcd1602_inst(pdev, 0xc0); /* Move cursor position using DDRAM (40) */
	lcd1602_puts(pdev, "Humidity: xx"); /* Display string */

	ret = device_create_file(dev, &dev_attr_temperature);
	if (ret != 0) {
		dev_err(dev, "[+] Failed to create sysfs entry");

		return ret;
	}

	ret = device_create_file(dev, &dev_attr_humidity);
	if (ret != 0) {
		dev_err(dev, "[+] Failed to create sysfs entry");

		return ret;
	}

	dev_info(dev, "[+] lcd1602_probe exit");

	return 0;
}

static int lcd1602_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lcd1602 *lcd1602 = platform_get_drvdata(pdev);

	dev_info(dev, "[+] lcd1602_remove enter");

	device_remove_file(dev, &dev_attr_humidity);
	device_remove_file(dev, &dev_attr_temperature);

	msleep(50);
	lcd1602_inst(pdev, 0x01); /* Clear display */

	devm_gpiod_put(dev, lcd1602->e); /* E */
	devm_gpiod_put(dev, lcd1602->rw); /* R/W */
	devm_gpiod_put(dev, lcd1602->rs); /* RS */
	devm_gpiod_put(dev, lcd1602->d7); /* D7 */
	devm_gpiod_put(dev, lcd1602->d6); /* D6 */
	devm_gpiod_put(dev, lcd1602->d5); /* D5 */
	devm_gpiod_put(dev, lcd1602->d4); /* D4 */

	dev_info(dev, "[+] lcd1602_remove exit");

	return 0;
}

static struct platform_driver lcd1602_driver = {
		.driver = {
				.name = DRIVER_NAME,
				.of_match_table = lcd1602_dt_ids,
		},
		.probe = lcd1602_probe,
		.remove = lcd1602_remove,
};

module_platform_driver(lcd1602_driver)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeonggyuny <Jeonggyuny@protonmail.com>");
MODULE_DESCRIPTION("This is a lcd1602 driver");
