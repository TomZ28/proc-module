#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

#define PROC_ENTRY_NAME "proc_module_file"

static ssize_t read_proc_module(struct file *fp, char __user *usrbuf, size_t length, loff_t *offset);
static ssize_t write_proc_module(struct file *fp, const char *usrbuf, size_t length, loff_t *offset);

/**
 * This is a data structure for storing user-written data.
 */
struct proc_entry_list {
	char *buf;
	size_t buf_size;
	struct list_head list;
};

static struct proc_dir_entry *proc_entry = NULL;

static struct proc_ops file_ops = {
	.proc_read = read_proc_module,
	.proc_write = write_proc_module,
};

static struct list_head proc_entry_list_head;
static size_t proc_entry_list_size;

rwlock_t proc_entry_list_rwlock;

/**
 * When the user reads from the proc file, fill the read buffer with list
 * entries (specifically, data the entries store) up to the given length.
 * 
 * Returns the actual size of the read data, or a negative value corresponding
 * to an error code otherwise.
 */
static ssize_t read_proc_module(struct file *fp, char __user *usrbuf, size_t length, loff_t *offset)
{
	struct proc_entry_list *entry;
	size_t next_pos, remaining_len, actual_size;
	size_t pos = 0, read_size = 0;
	loff_t off = *offset;
	ssize_t retval = 0;

	read_lock(&proc_entry_list_rwlock);

	if (length == 0 || off >= proc_entry_list_size)
		goto rd_end;
	else if (off < 0) {
		retval = -EINVAL;
		goto rd_end;
	}

	list_for_each_entry(entry, &proc_entry_list_head, list) {
		next_pos = pos + entry->buf_size;
		if (pos <= off && off < next_pos) {
			read_size = next_pos - off > length ? length : next_pos - off;
			
			remaining_len = copy_to_user(usrbuf, entry->buf + off - pos, read_size);
			if (remaining_len == read_size)
				retval = -EFAULT;
			else {
				actual_size = read_size - remaining_len;
				*offset += actual_size;
				retval = actual_size;
			}

			goto rd_end;
		}
		pos = next_pos;
	}

rd_end:
	read_unlock(&proc_entry_list_rwlock);
	return retval;
}

/**
 * When the user writes to the proc file, store a copy of the buffer in our
 * proc entry list.
 * 
 * Returns the actual size of the written data, or a negative value
 * corresponding to an error code otherwise.
 */
static ssize_t write_proc_module(struct file *fp, const char *usrbuf, size_t length, loff_t *offset)
{
	size_t remaining_len, actual_size;
	ssize_t errval;
	struct proc_entry_list *new_entry;
	char *tmp;
	loff_t off = *offset;

	if (length == 0)
		return 0;
	else if (off < 0)
		return -EINVAL;

	new_entry = kmalloc(sizeof(struct proc_entry_list), GFP_KERNEL);
	if (!new_entry)
		return -ENOMEM;

	tmp = kzalloc(length, GFP_KERNEL);
	if (!tmp) {
		errval = -ENOMEM;
		goto tmp_err;
	}

	remaining_len = copy_from_user(tmp, usrbuf, length);
	if (remaining_len == length) {
		errval = -EFAULT;
		goto cpy_err;
	}

	actual_size = length - remaining_len;
	new_entry->buf_size = actual_size;

	new_entry->buf = kmemdup(tmp, actual_size, GFP_KERNEL);
	if (!new_entry->buf) {
		errval = -ENOMEM;
		goto cpy_err;
	}
	kfree(tmp);

	*offset += actual_size;
	
	INIT_LIST_HEAD(&new_entry->list);

	write_lock(&proc_entry_list_rwlock);
	list_add_tail(&new_entry->list, &proc_entry_list_head);
	proc_entry_list_size += actual_size;
	write_unlock(&proc_entry_list_rwlock);

	return actual_size;

cpy_err:
	kfree(tmp);
tmp_err:
	kfree(new_entry);
	return errval;
}

/**
 * Module initialization.
 * 
 * Creates a proc entry under /proc and registers read/write file ops.
 * 
 * Returns 0 if the operation is successful, or a negative value corresponding
 * to an error code otherwise.
 */
static int __init init_proc_module(void) {
	proc_entry = proc_create(PROC_ENTRY_NAME, 0666, NULL, &file_ops);

	if (!proc_entry) {
		printk(KERN_ERR "Error: Could not create /proc/%s\n", PROC_ENTRY_NAME);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&proc_entry_list_head);
	proc_entry_list_size = 0;
	rwlock_init(&proc_entry_list_rwlock);

	printk(
		KERN_INFO "%s: Successfully created /proc/%s\n",
		module_name(THIS_MODULE),
		PROC_ENTRY_NAME
	);

	return 0;
}

/**
 * Module exit.
 * 
 * List memory and proc entry will get properly cleaned up when module is unloaded.
 */
static void __exit exit_proc_module(void)
{
	struct list_head *pos, *n;
	struct proc_entry_list *tmp;

	write_lock(&proc_entry_list_rwlock);

	list_for_each_safe(pos, n, &proc_entry_list_head) {
		tmp = list_entry(pos, struct proc_entry_list, list);
		list_del(pos);
		kfree(tmp->buf);
		kfree(tmp);
	}
	proc_entry_list_size = 0;

	if (proc_entry)
		remove_proc_entry(PROC_ENTRY_NAME, NULL);
		
	write_unlock(&proc_entry_list_rwlock);
	
	printk(
		KERN_INFO "%s: Successfully exited %s.\n",
		module_name(THIS_MODULE),
		module_name(THIS_MODULE)
	);
}

module_init(init_proc_module);
module_exit(exit_proc_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tommy Zhang");
MODULE_DESCRIPTION("Creates a proc entry and registers read/write file ops.");
