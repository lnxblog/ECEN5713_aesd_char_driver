/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/string.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("lnxblog"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev;
	PDEBUG("aesdchar open");
	/**
	 * TODO: handle open
	 */
	 dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */
	 
	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("aesdchar release");
	/**
	 * TODO: handle release
	 */
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct aesd_buffer_entry *entry;
	size_t byte_offset,tx_size;
	ssize_t retval = 0;
	const char *source;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */
 	if (mutex_lock_interruptible(&aesd_device.cb_mutex))	// obtain lock
	return -ERESTARTSYS;
	
	entry=aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.cbuffer,*f_pos,&byte_offset);
	if(entry==NULL)  // circular buffer empty
		goto out;
	
	source = &entry->buffptr[byte_offset];
	tx_size = entry->size-byte_offset;
	PDEBUG("copying %lld bytes to user",tx_size);
	copy_to_user(buf,source,tx_size);
	*f_pos += tx_size;
	retval = tx_size;
		 
	out:
	mutex_unlock(&aesd_device.cb_mutex);		// release lock
	return retval;
}
 	
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct aesd_buffer_entry *entry = &aesd_device.curr;
	ssize_t retval = -ENOMEM;
	char *eop;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */
	 if (mutex_lock_interruptible(&aesd_device.cb_mutex))	// obtain lock
		return -ERESTARTSYS;
	 if(entry->buffptr==NULL)
	 {
	 	entry->buffptr = kmalloc(count,GFP_KERNEL);
	 	if(!entry->buffptr)
	 		goto out;
	 	//entry->size = count;
	 }
	 else
	 {
	 	entry->buffptr = krealloc(entry->buffptr,entry->size+count,GFP_KERNEL);
	 	if(!entry->buffptr)
	 		goto out;
	 	//entry->size += count;
	 }
	 memset(entry->buffptr+entry->size,0,count);
	 if(copy_from_user(entry->buffptr+entry->size,buf,count))
	 {
		goto out;
	 }
	 entry->size += count;
	 retval = count;
	 if(strchr(entry->buffptr,'\n'))
	 {
		 PDEBUG("found EOF at offset %d FINISH WRITE. string %s",eop-entry->buffptr,entry->buffptr);
	 	char *free_me;
	 	free_me=aesd_circular_buffer_add_entry(&aesd_device.cbuffer,entry);
	 	if(free_me)
	 	{
	 		kfree(free_me);
	 	}
	 	entry->buffptr=NULL;
	 	entry->size=0;
	 }

	 
	 out:

	 mutex_unlock(&aesd_device.cb_mutex);		// release lock
	return retval;
}
struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */
	mutex_init(&aesd_device.cb_mutex);
	aesd_circular_buffer_init(&aesd_device.cbuffer);
	//aesd_device.cbuffer.in_offs=0;
	//aesd_device.cbuffer.out_offs=0;
	//aesd_device.cbuffer.full=0;
	aesd_device.curr.buffptr=0;
	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */
	int index;
 	struct aesd_buffer_entry *entry;
 	AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.cbuffer,index) {
  		kfree(entry->buffptr);
 	}

	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
