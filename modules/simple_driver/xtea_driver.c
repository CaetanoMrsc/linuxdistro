#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>

#define DEVICE_NAME "xtea_driver"
#define CLASS_NAME  "xtea_class"

#define XTEA_ROUNDS 32

MODULE_LICENSE("GPL");
static char *key0 = "00000000";
static char *key1 = "00000000";
static char *key2 = "00000000";
static char *key3 = "00000000";

module_param(key0, charp, 0644);
module_param(key1, charp, 0644);
module_param(key2, charp, 0644);
module_param(key3, charp, 0644);

MODULE_PARM_DESC(key0, "XTEA key 0 in hexadecimal");
MODULE_PARM_DESC(key1, "XTEA key 1 in hexadecimal");
MODULE_PARM_DESC(key2, "XTEA key 2 in hexadecimal");
MODULE_PARM_DESC(key3, "XTEA key 3 in hexadecimal");
MODULE_AUTHOR("Caetano");
MODULE_DESCRIPTION("XTEA Linux character driver");
MODULE_VERSION("1.0");

static int majorNumber;
static struct class *charClass = NULL;
static struct device *charDevice = NULL;

static void encipher(uint32_t num_rounds, uint32_t v[2],
                     const uint32_t key[4])
{
	uint32_t i;
	uint32_t v0 = v[0], v1 = v[1];
	uint32_t sum = 0;
	uint32_t delta = 0x9E3779B9;

	for (i = 0; i < num_rounds; i++) {
		v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^
		      (sum + key[sum & 3]);

		sum += delta;

		v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^
		      (sum + key[(sum >> 11) & 3]);
	}

	v[0] = v0;
	v[1] = v1;
}

static void decipher(uint32_t num_rounds, uint32_t v[2],
                     const uint32_t key[4])
{
	uint32_t i;
	uint32_t v0 = v[0], v1 = v[1];
	uint32_t delta = 0x9E3779B9;
	uint32_t sum = delta * num_rounds;

	for (i = 0; i < num_rounds; i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^
		      (sum + key[(sum >> 11) & 3]);

		sum -= delta;

		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^
		      (sum + key[sum & 3]);
	}

	v[0] = v0;
	v[1] = v1;
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "XTEA Driver: device opened\n");
	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "XTEA Driver: device closed\n");
	return 0;
}

static char output_buffer[128];
static size_t output_len;

static ssize_t dev_read(struct file *filep, char *buffer,
                        size_t len, loff_t *offset)
{
        if (*offset >= output_len)
                return 0;

        if (len > output_len - *offset)
                len = output_len - *offset;

        if (copy_to_user(buffer, output_buffer + *offset, len))
                return -EFAULT;

        *offset += len;

        return len;
}

static ssize_t dev_write(struct file *filep, const char *buffer,
                         size_t len, loff_t *offset)
{
        char input[128];
	char operation[4];
        unsigned int key[4];
        unsigned int data[2];
        unsigned int data_size;
        int ret;
	char *endp;

        if (len >= sizeof(input))
                return -EINVAL;

        if (copy_from_user(input, buffer, len))
                return -EFAULT;

        input[len] = '\0';

        ret = sscanf(input, "%3s %u %x %x",
             operation, &data_size, &data[0], &data[1]);

        if (ret != 4) {
                printk(KERN_ALERT "XTEA Driver: invalid command\n");
                return -EINVAL;
        }

        if (data_size != 8) {
                printk(KERN_ALERT "XTEA Driver: data_size must be 8\n");
                return -EINVAL;
        }

	key[0] = simple_strtoul(key0, &endp, 16);
	key[1] = simple_strtoul(key1, &endp, 16);
	key[2] = simple_strtoul(key2, &endp, 16);
	key[3] = simple_strtoul(key3, &endp, 16);

        if (strcmp(operation, "enc") == 0) {
        encipher(XTEA_ROUNDS, data, key);
	} else if (strcmp(operation, "dec") == 0) {
        decipher(XTEA_ROUNDS, data, key);
	} else {
        printk(KERN_ALERT "XTEA Driver: invalid operation\n");
        return -EINVAL;
	}

        output_len = snprintf(output_buffer, sizeof(output_buffer),
                              "%08x%08x\n", data[0], data[1]);

        printk(KERN_INFO "XTEA Driver: %s completed\n", operation);

        return len;
}

static struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int __init xtea_init(void)
{
	printk(KERN_INFO "XTEA Driver: Initializing the LKM\n");

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);

	if (majorNumber < 0) {
		printk(KERN_ALERT "XTEA Driver: failed to register major number\n");
		return majorNumber;
	}

	charClass = class_create(THIS_MODULE, CLASS_NAME);

	if (IS_ERR(charClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		return PTR_ERR(charClass);
	}

	charDevice = device_create(charClass, NULL,
				   MKDEV(majorNumber, 0),
				   NULL, DEVICE_NAME);

	if (IS_ERR(charDevice)) {
		class_destroy(charClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		return PTR_ERR(charDevice);
	}

	printk(KERN_INFO "XTEA Driver: device created successfully\n");

	return 0;
}

static void __exit xtea_exit(void)
{
	device_destroy(charClass, MKDEV(majorNumber, 0));
	class_unregister(charClass);
	class_destroy(charClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);

	printk(KERN_INFO "XTEA Driver: module unloaded\n");
}

module_init(xtea_init);
module_exit(xtea_exit);
