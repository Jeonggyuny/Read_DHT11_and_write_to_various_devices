#include <linux/module.h>
#include <linux/platform_device.h> /* platform_driver_register(), platform_set_drvdata() */
#include <linux/io.h> /* ioremap(), iowrite32() */
#include <linux/of.h> /* of_property_read_string() */
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

struct buzzer_dev
{
	struct miscdevice buzzer_misc_device; /* Assign device for buzzer */
	const char * buzzer_name; /* Stores "label" string */
};

#define BCM2710_PERI_BASE 0x3F000000
#define GPIO_BASE (BCM2710_PERI_BASE + 0x200000) /* GPIO Controller */

#define GPIO_18 18

/* To operate a buzzer */
#define GPIO_18_INDEX 1 << (GPIO_18 % 32)

/* Select the output function */
#define GPIO_18_FUNC 1 << ((GPIO_18 % 10) * 3)

/* Mask the GPIO function */
#define FSEL_18_MASK 0b111 << ((GPIO_18 % 10) * 3) /* buzzer since bit 24 (FSEL18) */

#define GPIO_SET_FUNCTION_18_BUZZER (GPIO_18_FUNC)
#define GPIO_MASK_18_BUZZER (FSEL_18_MASK)
#define GPIO_SET_18_BUZZER (GPIO_18_INDEX)

#define GPFSEL1 GPIO_BASE + 0x04
#define GPSET0 GPIO_BASE + 0x1C
#define GPCLR0 GPIO_BASE + 0x28

#define CLASS_NAME "buzzer_class"
#define DEVICE_NAME "buzzer_dev"

/* Declear __iomem pointers that will keep virtual addresses */
static void __iomem *GPFSEL1_V;
static void __iomem *GPSET0_V;
static void __iomem *GPCLR0_V;

static int Temperature = 0;

static struct class *buzzer_class;
static struct device *buzzer_dev;
dev_t dev;

static void buzzer_work(struct work_struct *unused);
static DECLARE_WORK(work, buzzer_work);

static void buzzer_work(struct work_struct *unused)
{
	iowrite32(GPIO_18_INDEX, GPSET0_V);
	mdelay(1);
	iowrite32(GPIO_18_INDEX, GPCLR0_V);
	mdelay(1);

	if (Temperature > 30) {
		schedule_work(&work);
	}
}

static ssize_t set_temperature(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	long temperature_value = 0;

	pr_info("[+] set_temperature enter\n");

	if (kstrtol(buf, 10, &temperature_value) < 0) {
		return -EINVAL;
	}

	Temperature = temperature_value;

	if (Temperature > 30) {
		schedule_work(&work);
	}

	pr_info("[+] set_temperature exit\n");

	return count;
}
static DEVICE_ATTR(temperature, S_IWUSR, NULL, set_temperature);

static int __init buzzer_probe(struct platform_device *pdev)
{
	struct buzzer_dev *buzzer_device;
	int ret_val;

	pr_info("[+] buzzer_probe enter\n");

	buzzer_device = devm_kzalloc(&pdev->dev, sizeof(struct buzzer_dev), GFP_KERNEL);
	buzzer_device->buzzer_misc_device.minor = MISC_DYNAMIC_MINOR;
	buzzer_device->buzzer_misc_device.name = "my_buzzer";

	ret_val = misc_register(&buzzer_device->buzzer_misc_device);
	if (ret_val) {
		return ret_val;
	}

	platform_set_drvdata(pdev, buzzer_device);

	pr_info("[+] buzzer_probe exit\n");

	return 0;
}

static int __exit buzzer_remove(struct platform_device *pdev)
{
	struct buzzer_dev *buzzer_device = platform_get_drvdata(pdev);

	pr_info("[+] buzzer_remove enter\n");

	misc_deregister(&buzzer_device->buzzer_misc_device);

	pr_info("[+] buzzer_remove exit\n");

	return 0;
}

static const struct of_device_id my_of_ids[] = {
		{ .compatible = "arrow,my_buzzer" },
		{ },
};
MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver buzzer_platform_driver = {
		.probe = buzzer_probe,
		.remove = buzzer_remove,
		.driver = {
				.name = "my_buzzer",
				.of_match_table = my_of_ids,
				.owner = THIS_MODULE,
		}
};

static int buzzer_init(void)
{
	int ret_val;
	u32 GPFSEL_read, GPFSEL_write;
	dev_t dev_no;
	int Major;

	pr_info("[+] buzzer_init enter\n");

	ret_val = platform_driver_register(&buzzer_platform_driver);
	if (ret_val != 0) {
		pr_err("[+] Platform value returned %d\n", ret_val);

		return ret_val;
	}

	GPFSEL1_V = ioremap(GPFSEL1, sizeof(u32));
	GPSET0_V = ioremap(GPSET0, sizeof(u32));
	GPCLR0_V = ioremap(GPCLR0, sizeof(u32));

	GPFSEL_read = ioread32(GPFSEL1_V); /* Read current value */
	/* Set to 0 3 bits of each FSEL and keep equal the rest of bits,
	 * then set to 1 the first bit of each FSEL (function) to set 3 GPIOS to output.
	 */
	GPFSEL_write = (GPFSEL_read & ~GPIO_MASK_18_BUZZER)
			| (GPIO_SET_FUNCTION_18_BUZZER & GPIO_MASK_18_BUZZER);
	iowrite32(GPFSEL_write, GPFSEL1_V); /* Set buzzer to output */

	iowrite32(GPIO_18_INDEX, GPCLR0_V); /* Clear GPIO 18, output is low */

	/* Allocate dynamically device numbers */
	ret_val = alloc_chrdev_region(&dev_no, 0, 1, DEVICE_NAME);
	if (ret_val < 0) {
		pr_info("[+] Unable to allocate major number\n");

		return ret_val;
	}

	/* Get the device identifiers */
	Major = MAJOR(dev_no);
	dev = MKDEV(Major, 0);
	pr_info("[+] Allocated correctly with major number %d\n", Major);

	/* Register the device class */
	buzzer_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(buzzer_class)) {
		unregister_chrdev_region(dev, 1);

		pr_info("[+] Failed to register device class\n");

		return PTR_ERR(buzzer_class);
	}
	pr_info("[+] Device class registered correclty\n");

	/* Create a device node named DEVICE_NAME associated to dev */
	buzzer_dev = device_create(buzzer_class, NULL, dev, NULL, DEVICE_NAME);
	if (IS_ERR(buzzer_dev)) {
		class_destroy(buzzer_class);
		unregister_chrdev_region(dev, 1);

		pr_info("[+] Failed to create the device\n");

		return PTR_ERR(buzzer_dev);
	}
	pr_info("[+] The device is created correctly\n");

	ret_val = device_create_file(buzzer_dev, &dev_attr_temperature);
	if (ret_val != 0) {
		dev_err(buzzer_dev, "[+] Failed to create sysfs entry");

		return ret_val;
	}

	pr_info("[+] buzzer_init exit\n");

	return 0;
}

static void buzzer_exit(void)
{
	pr_info("[+] buzzer_exit enter\n");

	device_remove_file(buzzer_dev, &dev_attr_temperature);

	device_destroy(buzzer_class, dev); /* Remove the device */
	class_destroy(buzzer_class); /* Remove the device class */
	unregister_chrdev_region(dev, 1); /* Unregister the device numbers */

	iowrite32(GPIO_18_INDEX, GPCLR0_V); /* Clear buzzer */

	iounmap(GPFSEL1_V);
	iounmap(GPSET0_V);
	iounmap(GPCLR0_V);

	platform_driver_unregister(&buzzer_platform_driver);

	pr_info("[+] buzzer_exit exit\n");
}

module_init(buzzer_init);
module_exit(buzzer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeonggyuny <Jeonggyuny@protonmail.com>");
MODULE_DESCRIPTION("This is a buzzer driver");
