#include <linux/module.h>
#include <linux/platform_device.h> /* platform_driver_register(), platform_set_drvdata() */
#include <linux/io.h> /* ioremap(), iowrite32() */
#include <linux/of.h> /* of_property_read_string() */
#include <linux/miscdevice.h>

struct led_dev
{
	struct miscdevice led_misc_device; /* Assign device for each led */
	const char * led_name; /* Stores "label" string */
};

#define BCM2710_PERI_BASE 0x3F000000
#define GPIO_BASE (BCM2710_PERI_BASE + 0x200000) /* GPIO Controller */

#define GPIO_17 17
#define GPIO_27	27
#define GPIO_22	22

/* To set and clear each individual LED */
#define GPIO_17_INDEX 1 << (GPIO_17 % 32)
#define GPIO_27_INDEX 1 << (GPIO_27 % 32)
#define GPIO_22_INDEX 1 << (GPIO_22 % 32)

/* Select the output function */
#define GPIO_17_FUNC 1 << ((GPIO_17 % 10) * 3)
#define GPIO_27_FUNC 1 << ((GPIO_27 % 10) * 3)
#define GPIO_22_FUNC 1 << ((GPIO_22 % 10) * 3)

/* Mask the GPIO function */
#define FSEL_17_MASK 0b111 << ((GPIO_17 % 10) * 3) /* Red since bit 21 (FSEL17) */
#define FSEL_27_MASK 0b111 << ((GPIO_27 % 10) * 3) /* Yellow since bit 21 (FSEL27) */
#define FSEL_22_MASK 0b111 << ((GPIO_22 % 10) * 3) /* Green since bit 6 (FSEL22) */

#define GPIO_SET_FUNCTION_17_LED (GPIO_17_FUNC)
#define GPIO_SET_FUNCTION_27_22_LEDS (GPIO_27_FUNC | GPIO_22_FUNC)
#define GPIO_MASK_17_LED (FSEL_17_MASK)
#define GPIO_MASK_27_22_LEDS (FSEL_27_MASK | FSEL_22_MASK)
#define GPIO_SET_17_LED	(GPIO_17_INDEX)
#define GPIO_SET_27_22_LEDS (GPIO_27_INDEX | GPIO_22_INDEX)

#define GPIO_SET_FUNCTION_LEDS (GPIO_17_FUNC | GPIO_27_FUNC | GPIO_22_FUNC)
#define GPIO_MASK_ALL_LEDS (FSEL_17_MASK | FSEL_27_MASK | FSEL_22_MASK)
#define GPIO_SET_ALL_LEDS (GPIO_17_INDEX | GPIO_27_INDEX | GPIO_22_INDEX)

#define GPFSEL1 GPIO_BASE + 0x04
#define GPFSEL2	GPIO_BASE + 0x08
#define GPSET0 GPIO_BASE + 0x1C
#define GPCLR0 GPIO_BASE + 0x28

#define CLASS_NAME "RYGleds_class"
#define DEVICE_NAME "RYGleds_dev"

/* Declear __iomem pointers that will keep virtual addresses */
static void __iomem *GPFSEL1_V;
static void __iomem *GPFSEL2_V;
static void __iomem *GPSET0_V;
static void __iomem *GPCLR0_V;

static struct timer_list BlinkTimer;
static int BlinkPeriod = 500;
static int Temperature = 0;

static struct class *RYGleds_class;
static struct device *RYGleds_dev;
dev_t dev;

static void SetGPIOOutputValue(int temperature, bool outputValue)
{
	if (temperature == 0) { /* Default - All leds is blinking */
		if (outputValue) {
			iowrite32(GPIO_SET_ALL_LEDS, GPSET0_V);
		} else {
			iowrite32(GPIO_SET_ALL_LEDS, GPCLR0_V);
		}
	} else { /* Operating */
		if (temperature > 0 && temperature <= 25) { /* Green led is blinking */
			if (outputValue) {
				iowrite32(GPIO_22_INDEX, GPSET0_V);
			} else {
				iowrite32(GPIO_SET_ALL_LEDS, GPCLR0_V);
			}
		} else if (temperature > 25 && temperature <= 30) { /* Yellow led is blinking */
			if (outputValue) {
				iowrite32(GPIO_27_INDEX, GPSET0_V);
			} else {
				iowrite32(GPIO_SET_ALL_LEDS, GPCLR0_V);
			}
		} else if (temperature > 30) { /* Red led is blinking */
			if (outputValue) {
				iowrite32(GPIO_17_INDEX, GPSET0_V);
			} else {
				iowrite32(GPIO_SET_ALL_LEDS, GPCLR0_V);
			}
		}
	}
}

static void BlinkTimerHandler(unsigned long unused)
{
	static bool on = false;

	on = !on;

	SetGPIOOutputValue(Temperature, on);

	mod_timer(&BlinkTimer, jiffies + msecs_to_jiffies(BlinkPeriod));
}

static ssize_t set_temperature(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	long temperature_value = 0;

	pr_info("[+] set_temperature enter\n");

	if (kstrtol(buf, 10, &temperature_value) < 0) {
		return -EINVAL;
	}

	Temperature = temperature_value;

	pr_info("[+] set_temperature exit\n");

	return count;
}
static DEVICE_ATTR(temperature, S_IWUSR, NULL, set_temperature);

static int __init led_probe(struct platform_device *pdev)
{
	struct led_dev *led_device;
	int ret_val;

	pr_info("[+] led_probe enter\n");

	led_device = devm_kzalloc(&pdev->dev, sizeof(struct led_dev), GFP_KERNEL);

	of_property_read_string(pdev->dev.of_node, "label", &led_device->led_name);
	led_device->led_misc_device.minor = MISC_DYNAMIC_MINOR;
	led_device->led_misc_device.name = led_device->led_name;

	ret_val = misc_register(&led_device->led_misc_device);
	if (ret_val) {
		return ret_val;
	}

	platform_set_drvdata(pdev, led_device);

	pr_info("[+] led_probe exit\n");

	return 0;
}

static int __exit led_remove(struct platform_device *pdev)
{
	struct led_dev *led_device = platform_get_drvdata(pdev);

	pr_info("[+] led_remove enter\n");

	misc_deregister(&led_device->led_misc_device);

	pr_info("[+] led_remove exit\n");

	return 0;
}

static const struct of_device_id my_of_ids[] = {
		{ .compatible = "arrow,my_RYGleds" },
		{ },
};
MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver led_platform_driver = {
		.probe = led_probe,
		.remove = led_remove,
		.driver = {
				.name = "my_RYGleds",
				.of_match_table = my_of_ids,
				.owner = THIS_MODULE,
		}
};

static int RYGleds_init(void)
{
	int ret_val;
	u32 GPFSEL_read, GPFSEL_write;
	dev_t dev_no;
	int Major;

	pr_info("[+] RYGleds_init enter\n");

	ret_val = platform_driver_register(&led_platform_driver);
	if (ret_val != 0) {
		pr_err("[+] Platform value returned %d\n", ret_val);

		return ret_val;
	}

	GPFSEL1_V = ioremap(GPFSEL1, sizeof(u32));
	GPFSEL2_V = ioremap(GPFSEL2, sizeof(u32));
	GPSET0_V = ioremap(GPSET0, sizeof(u32));
	GPCLR0_V = ioremap(GPCLR0, sizeof(u32));

	GPFSEL_read = ioread32(GPFSEL1_V); /* Read current value */
	/* Set to 0 3 bits of each FSEL and keep equal the rest of bits,
	 * then set to 1 the first bit of each FSEL (function) to set 3 GPIOS to output.
	 */
	GPFSEL_write = (GPFSEL_read & ~GPIO_MASK_17_LED)
			| (GPIO_SET_FUNCTION_17_LED & GPIO_MASK_17_LED);
	iowrite32(GPFSEL_write, GPFSEL1_V); /* Set leds to output */

	GPFSEL_read = ioread32(GPFSEL2_V); /* Read current value */
	/* Set to 0 3 bits of each FSEL and keep equal the rest of bits,
	 * then set to 1 the first bit of each FSEL (function) to set 3 GPIOS to output.
	 */
	GPFSEL_write = (GPFSEL_read & ~GPIO_MASK_27_22_LEDS)
			| (GPIO_SET_FUNCTION_27_22_LEDS & GPIO_MASK_27_22_LEDS);
	iowrite32(GPFSEL_write, GPFSEL2_V); /* Set leds to output */

	iowrite32(GPIO_SET_ALL_LEDS, GPCLR0_V); /* Clear all the leds, output is low */

	/* Allocate dynamically device numbers */
	ret_val = alloc_chrdev_region(&dev_no, 0, 1, DEVICE_NAME);
	if (ret_val < 0) {
		pr_info("[+] Unable to allocate Major number\n");

		return ret_val;
	}

	/* Get the device identifiers */
	Major = MAJOR(dev_no);
	dev = MKDEV(Major, 0);
	pr_info("[+] Allocated correctly with major number %d\n", Major);

	/* Register the device class */
	RYGleds_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(RYGleds_class)) {
		unregister_chrdev_region(dev, 1);

		pr_info("[+] Failed to register device class\n");

		return PTR_ERR(RYGleds_class);
	}
	pr_info("[+] Device class registered correclty\n");

	/* Create a device node named DEVICE_NAME associated to dev */
	RYGleds_dev = device_create(RYGleds_class, NULL, dev, NULL, DEVICE_NAME);
	if (IS_ERR(RYGleds_dev)) {
		class_destroy(RYGleds_class);
		unregister_chrdev_region(dev, 1);

		pr_info("[+] Failed to create the device\n");

		return PTR_ERR(RYGleds_dev);
	}
	pr_info("[+] The device is created correctly\n");

	ret_val = device_create_file(RYGleds_dev, &dev_attr_temperature);
	if (ret_val != 0) {
		dev_err(RYGleds_dev, "[+] Failed to create sysfs entry");

		return ret_val;
	}

	setup_timer(&BlinkTimer, BlinkTimerHandler, 0);
	ret_val = mod_timer(&BlinkTimer, jiffies + msecs_to_jiffies(BlinkPeriod));

	pr_info("[+] RYGleds_init exit\n");

	return 0;
}

static void RYGleds_exit(void)
{
	pr_info("[+] RYGleds_exit enter\n");

	del_timer(&BlinkTimer);

	device_remove_file(RYGleds_dev, &dev_attr_temperature);

	device_destroy(RYGleds_class, dev); /* Remove the device */
	class_destroy(RYGleds_class); /* Remove the device class */
	unregister_chrdev_region(dev, 1); /* Unregister the device numbers */

	iowrite32(GPIO_SET_ALL_LEDS, GPCLR0_V); /* Clear all the leds */

	iounmap(GPFSEL1_V);
	iounmap(GPFSEL2_V);
	iounmap(GPSET0_V);
	iounmap(GPCLR0_V);

	platform_driver_unregister(&led_platform_driver);

	pr_info("[+] RYGleds_exit exit\n");
}

module_init(RYGleds_init);
module_exit(RYGleds_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeonggyuny <Jeonggyuny@protonmail.com>");
MODULE_DESCRIPTION("This is a RYGleds driver");
