/*
 * int_stack.c
 * Simple character device
 * Implements a global integer stack with push/pop/ioctl
 * Author: n.sannikov@innopolis.university
 */

 #include <linux/module.h>    // core module stuff
 #include <linux/kernel.h>    // printk
 #include <linux/init.h>      // __init, __exit
 #include <linux/fs.h>        // file_operations
 #include <linux/uaccess.h>   // copy_to_user, copy_from_user
 #include <linux/cdev.h>      // cdev
 #include <linux/device.h>    // class_create, device_create
 #include <linux/slab.h>      // kmalloc, kfree
 #include <linux/mutex.h>     // mutex
 
 #define DEVICE_NAME "int_stack"
 #define CLASS_NAME  "intstack"
 #define DEFAULT_SIZE 1024    // default max stack entries
 
 // ioctl definitions
 #define IOC_MAGIC 's'
 #define IOC_SET_SIZE _IOW(IOC_MAGIC, 1, unsigned int)
 
 static dev_t dev_num;
 static struct class *stack_class;
 static struct cdev stack_cdev;
 
 // Our stack container
 struct int_stack {
     int *buf;              // pointer to entries
     size_t top;            // next free index
     size_t max;            // max entries
     struct mutex lock;     // to protect stack
 };
 static struct int_stack my_stack;
 
 // open: just tag private_data
 static int stack_open(struct inode *inode, struct file *file)
 {
     file->private_data = &my_stack;
     pr_info("[int_stack] opened\n");
     return 0;
 }
 
 // release: nothing fancy
 static int stack_release(struct inode *inode, struct file *file)
 {
     pr_info("[int_stack] closed\n");
     return 0;
 }
 
 // read = pop
 static ssize_t stack_read(struct file *file, char __user *ubuf, size_t cnt, loff_t *off)
 {
     struct int_stack *s = file->private_data;
     int val;
 
     if (cnt < sizeof(int))
         return -EINVAL;    // need enough space
 
     mutex_lock(&s->lock);
     if (s->top == 0) {
         mutex_unlock(&s->lock);
         return 0;          // stack empty -> EOF
     }
     s->top--;
     val = s->buf[s->top];
     mutex_unlock(&s->lock);
 
     if (copy_to_user(ubuf, &val, sizeof(val)))
         return -EFAULT;
 
     return sizeof(int);
 }
 
 // write = push
 static ssize_t stack_write(struct file *file, const char __user *ubuf, size_t cnt, loff_t *off)
 {
     struct int_stack *s = file->private_data;
     int val;
 
     if (cnt < sizeof(int))
         return -EINVAL;
     if (copy_from_user(&val, ubuf, sizeof(val)))
         return -EFAULT;
 
     mutex_lock(&s->lock);
     if (s->top >= s->max) {
         mutex_unlock(&s->lock);
         return -ERANGE;    // full stack
     }
     s->buf[s->top++] = val;
     mutex_unlock(&s->lock);
 
     return sizeof(int);
 }
 
 // ioctl: change max size
 static long stack_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
 {
     struct int_stack *s = file->private_data;
     unsigned int new_max;
     int *new_buf;
 
     if (_IOC_TYPE(cmd) != IOC_MAGIC)
         return -ENOTTY;
 
     switch (cmd) {
     case IOC_SET_SIZE:
         if (copy_from_user(&new_max, (unsigned int __user *)arg, sizeof(new_max)))
             return -EFAULT;
         if (new_max == 0)
             return -EINVAL;
 
         // resize the stack
         mutex_lock(&s->lock);
         new_buf = kmalloc_array(new_max, sizeof(int), GFP_KERNEL);
         if (!new_buf) {
             mutex_unlock(&s->lock);
             return -ENOMEM;
         }
         // if we're shrinking, drop extra values
         if (s->top > new_max)
             s->top = new_max;
         // copy old entries
         memcpy(new_buf, s->buf, s->top * sizeof(int));
         kfree(s->buf);
         s->buf = new_buf;
         s->max = new_max;
         mutex_unlock(&s->lock);
 
         pr_info("[int_stack] resized to %zu\n", s->max);
         return 0;
 
     default:
         return -ENOTTY;
     }
 }
 
 static const struct file_operations fops = {
     .owner          = THIS_MODULE,
     .open           = stack_open,
     .release        = stack_release,
     .read           = stack_read,
     .write          = stack_write,
     .unlocked_ioctl = stack_ioctl,
 };
 
 // module init
 static int __init stack_init(void)
 {
     int ret;
 
     // allocate char device region
     ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
     if (ret < 0) {
         pr_err("[int_stack] alloc_chrdev_region failed: %d\n", ret);
         return ret;
     }
 
     // init stack data
     my_stack.buf = kmalloc_array(DEFAULT_SIZE, sizeof(int), GFP_KERNEL);
     if (!my_stack.buf) {
         unregister_chrdev_region(dev_num, 1);
         return -ENOMEM;
     }
     my_stack.top = 0;
     my_stack.max = DEFAULT_SIZE;
     mutex_init(&my_stack.lock);
 
     // setup cdev
     cdev_init(&stack_cdev, &fops);
     stack_cdev.owner = THIS_MODULE;
     ret = cdev_add(&stack_cdev, dev_num, 1);
     if (ret) {
         pr_err("[int_stack] cdev_add failed: %d\n", ret);
         kfree(my_stack.buf);
         unregister_chrdev_region(dev_num, 1);
         return ret;
     }
 
     // create device node
     stack_class = class_create(THIS_MODULE, CLASS_NAME);
     if (IS_ERR(stack_class)) {
         pr_err("[int_stack] class_create failed\n");
         cdev_del(&stack_cdev);
         kfree(my_stack.buf);
         unregister_chrdev_region(dev_num, 1);
         return PTR_ERR(stack_class);
     }
     if (IS_ERR(device_create(stack_class, NULL, dev_num, NULL, DEVICE_NAME))) {
         pr_err("[int_stack] device_create failed\n");
         class_destroy(stack_class);
         cdev_del(&stack_cdev);
         kfree(my_stack.buf);
         unregister_chrdev_region(dev_num, 1);
         return -1;
     }
 
     pr_info("[int_stack] loaded: /dev/%s (major=%d)\n", DEVICE_NAME, MAJOR(dev_num));
     return 0;
 }
 
 // module exit
 static void __exit stack_exit(void)
 {
     device_destroy(stack_class, dev_num);
     class_destroy(stack_class);
     cdev_del(&stack_cdev);
     unregister_chrdev_region(dev_num, 1);
 
     mutex_destroy(&my_stack.lock);
     kfree(my_stack.buf);
 
     pr_info("[int_stack] unloaded\n");
 }
 
 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("Nikita Sannikov");
 MODULE_DESCRIPTION("lab4 int stack module");
 MODULE_VERSION("1.0");
 
 module_init(stack_init);
 module_exit(stack_exit);
 