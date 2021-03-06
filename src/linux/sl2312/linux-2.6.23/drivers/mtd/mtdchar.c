/*
 * $Id: mtdchar.c,v 1.76 2005/11/07 11:14:20 gleixner Exp $
 *
 * Character-device access to raw MTD devices.
 *
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/compatmac.h>

#include <asm/uaccess.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#else
#include <linux/device.h>

static struct class *mtd_class;
#endif

static void mtd_notify_add(struct mtd_info* mtd)
{
	if (!mtd)
		return;

#ifdef CONFIG_DEVFS_FS
	devfs_mk_cdev(MKDEV(MTD_CHAR_MAJOR, mtd->index*2),
			S_IFCHR | S_IRUGO | S_IWUGO, "mtd/%d", mtd->index);

	devfs_mk_cdev(MKDEV(MTD_CHAR_MAJOR, mtd->index*2+1),
			S_IFCHR | S_IRUGO, "mtd/%dro", mtd->index);
#else
	class_device_create(mtd_class, NULL, MKDEV(MTD_CHAR_MAJOR, mtd->index*2),
			    NULL, "mtd%d", mtd->index);

	class_device_create(mtd_class, NULL,
			    MKDEV(MTD_CHAR_MAJOR, mtd->index*2+1),
			    NULL, "mtd%dro", mtd->index);
#endif
}

static void mtd_notify_remove(struct mtd_info* mtd)
{
	if (!mtd)
		return;

#ifdef CONFIG_DEVFS_FS
	devfs_remove("mtd/%d", mtd->index);
	devfs_remove("mtd/%dro", mtd->index);
#else
	class_device_destroy(mtd_class, MKDEV(MTD_CHAR_MAJOR, mtd->index*2));
	class_device_destroy(mtd_class, MKDEV(MTD_CHAR_MAJOR, mtd->index*2+1));
#endif
}

static struct mtd_notifier notifier = {
	.add	= mtd_notify_add,
	.remove	= mtd_notify_remove,
};

#ifdef CONFIG_DEVFS_FS
	static inline void mtdchar_devfs_init(void)
	{
		devfs_mk_dir("mtd");
		register_mtd_user(&notifier);
	}
	static inline void mtdchar_devfs_exit(void)
	{
		unregister_mtd_user(&notifier);
		devfs_remove("mtd");
	}
	#else /* !DEVFS */
	#define mtdchar_devfs_init() do { } while(0)
	#define mtdchar_devfs_exit() do { } while(0)
#endif

/*
 * Data structure to hold the pointer to the mtd device as well
 * as mode information ofr various use cases.
 */
struct mtd_file_info {
	struct mtd_info *mtd;
	enum mtd_file_modes mode;
};

/***********************************************************************
/*             Storlink SoC -- flash
/***********************************************************************/
#ifdef CONFIG_SL2312_SHARE_PIN
unsigned int share_pin_flag=0;		// bit0:FLASH, bit1:UART, bit2:EMAC, bit3-4:IDE
unsigned int check_sleep_flag=0;	// bit0:FLASH, bit1:IDE
static spinlock_t sl2312_flash_lock = SPIN_LOCK_UNLOCKED;
EXPORT_SYMBOL(share_pin_flag);
int dbg=0;
DECLARE_WAIT_QUEUE_HEAD(wq);
extern struct wait_queue_head_t *flash_wait;
unsigned int flash_req=0;
void mtd_lock()
{
	struct task_struct *tsk = current;
	unsigned int value ;
	unsigned long flags;
	flash_req = 1;
	DECLARE_WAITQUEUE(wait, tsk);
	add_wait_queue(&wq, &wait);
	for(;;)
	{
		set_task_state(tsk, TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&sl2312_flash_lock,flags);
		if((share_pin_flag&0x1E)){//||(check_sleep_flag&0x00000002)) {
			spin_unlock_irqrestore(&sl2312_flash_lock, flags);
			check_sleep_flag |= 0x00000001;
			if(dbg)
				printk("mtd yield %x %x\n",share_pin_flag,check_sleep_flag);
			wake_up_interruptible(&flash_wait);
			schedule();
		}
		else {
			check_sleep_flag &= ~0x01;
			share_pin_flag |= 0x00000001 ;			// set share pin flag
			spin_unlock_irqrestore(&sl2312_flash_lock, flags);
			value = readl(IO_ADDRESS((SL2312_GLOBAL_BASE+GLOBAL_MISC_REG)));
			value = value & (~PFLASH_SHARE_BIT) ;
			writel(value,IO_ADDRESS((SL2312_GLOBAL_BASE+GLOBAL_MISC_REG)));
			if(dbg)
				printk("mtd Go %x %x\n",share_pin_flag,check_sleep_flag);
			tsk->state = TASK_RUNNING;
			remove_wait_queue(&wq, &wait);
			return ;
		}
	}
}

void mtd_unlock()
{
	unsigned int value ;
	unsigned long flags;

	spin_lock_irqsave(&sl2312_flash_lock,flags);		// Disable IRQ
	value = readl(IO_ADDRESS((SL2312_GLOBAL_BASE+GLOBAL_MISC_REG)));
	value = value | PFLASH_SHARE_BIT ;				// Disable Flash PADs
	writel(value,IO_ADDRESS((SL2312_GLOBAL_BASE+GLOBAL_MISC_REG)));
	share_pin_flag &= ~(0x00000001);			// clear share pin flag
	check_sleep_flag &= ~0x00000001;
	spin_unlock_irqrestore(&sl2312_flash_lock, flags);	// Restore IRQ
	if (check_sleep_flag & 0x00000002)
	{
		check_sleep_flag &= ~(0x00000002);
		wake_up_interruptible(&flash_wait);
	}
	DEBUG(MTD_DEBUG_LEVEL0, "Flash Unlock...\n");
	flash_req = 0;
}
#endif
/***********************************************************************/

static loff_t mtd_lseek (struct file *file, loff_t offset, int orig)
{
	struct mtd_file_info *mfi = file->private_data;
	struct mtd_info *mtd = mfi->mtd;

	switch (orig) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += file->f_pos;
		break;
	case SEEK_END:
		offset += mtd->size;
		break;
	default:
		return -EINVAL;
	}

	if (offset >= 0 && offset <= mtd->size)
		return file->f_pos = offset;

	return -EINVAL;
}



static int mtd_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);
	int devnum = minor >> 1;
	struct mtd_info *mtd;
	struct mtd_file_info *mfi;

	DEBUG(MTD_DEBUG_LEVEL0, "MTD_open\n");

	if (devnum >= MAX_MTD_DEVICES)
		return -ENODEV;

	/* You can't open the RO devices RW */
	if ((file->f_mode & 2) && (minor & 1))
		return -EACCES;

	mtd = get_mtd_device(NULL, devnum);

	if (IS_ERR(mtd))
		return PTR_ERR(mtd);

	if (MTD_ABSENT == mtd->type) {
		put_mtd_device(mtd);
		return -ENODEV;
	}

	/* You can't open it RW if it's not a writeable device */
	if ((file->f_mode & 2) && !(mtd->flags & MTD_WRITEABLE)) {
		put_mtd_device(mtd);
		return -EACCES;
	}

	mfi = kzalloc(sizeof(*mfi), GFP_KERNEL);
	if (!mfi) {
		put_mtd_device(mtd);
		return -ENOMEM;
	}
	mfi->mtd = mtd;
	file->private_data = mfi;

	return 0;
} /* mtd_open */

/*====================================================================*/

static int mtd_close(struct inode *inode, struct file *file)
{
	struct mtd_file_info *mfi = file->private_data;
	struct mtd_info *mtd = mfi->mtd;

	DEBUG(MTD_DEBUG_LEVEL0, "MTD_close\n");

	if (mtd->sync)
		mtd->sync(mtd);

	put_mtd_device(mtd);
	file->private_data = NULL;
	kfree(mfi);

	return 0;
} /* mtd_close */

/* FIXME: This _really_ needs to die. In 2.5, we should lock the
   userspace buffer down and use it directly with readv/writev.
*/
#define MAX_KMALLOC_SIZE 0x20000

static ssize_t mtd_read(struct file *file, char __user *buf, size_t count,loff_t *ppos)
{
	struct mtd_file_info *mfi = file->private_data;
	struct mtd_info *mtd = mfi->mtd;
	size_t retlen=0;
	size_t total_retlen=0;
	int ret=0;
	int len;
	char *kbuf;

#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_lock();				// sl2312 share pin lock
#endif

	DEBUG(MTD_DEBUG_LEVEL0,"MTD_read\n");

	if (*ppos + count > mtd->size)
		count = mtd->size - *ppos;

	if (!count){
#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_unlock();				// sl2312 share pin lock
#endif
		return 0;
	}

	/* FIXME: Use kiovec in 2.5 to lock down the user's buffers
	   and pass them directly to the MTD functions */

	if (count > MAX_KMALLOC_SIZE)
		kbuf=kmalloc(MAX_KMALLOC_SIZE, GFP_KERNEL);
	else
		kbuf=kmalloc(count, GFP_KERNEL);

	if (!kbuf) {
#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_unlock();				// sl2312 share pin lock
#endif
		return -ENOMEM;
	}

	while (count) {

		if (count > MAX_KMALLOC_SIZE)
			len = MAX_KMALLOC_SIZE;
		else
			len = count;

		switch (mfi->mode) {
		case MTD_MODE_OTP_FACTORY:
			ret = mtd->read_fact_prot_reg(mtd, *ppos, len, &retlen, kbuf);
			break;
		case MTD_MODE_OTP_USER:
			ret = mtd->read_user_prot_reg(mtd, *ppos, len, &retlen, kbuf);
			break;
		case MTD_MODE_RAW:
		{
			struct mtd_oob_ops ops;

			ops.mode = MTD_OOB_RAW;
			ops.datbuf = kbuf;
			ops.oobbuf = NULL;
			ops.len = len;

			ret = mtd->read_oob(mtd, *ppos, &ops);
			retlen = ops.retlen;
			break;
		}
		default:
			ret = mtd->read(mtd, *ppos, len, &retlen, kbuf);
		}
		/* Nand returns -EBADMSG on ecc errors, but it returns
		 * the data. For our userspace tools it is important
		 * to dump areas with ecc errors !
		 * For kernel internal usage it also might return -EUCLEAN
		 * to signal the caller that a bitflip has occured and has
		 * been corrected by the ECC algorithm.
		 * Userspace software which accesses NAND this way
		 * must be aware of the fact that it deals with NAND
		 */
		if (!ret || (ret == -EUCLEAN) || (ret == -EBADMSG)) {
			*ppos += retlen;
			if (copy_to_user(buf, kbuf, retlen)) {
				kfree(kbuf);
#ifdef CONFIG_SL2312_SHARE_PIN
				mtd_unlock();				// sl2312 share pin lock
#endif
				return -EFAULT;
			}
			else
				total_retlen += retlen;

			count -= retlen;
			buf += retlen;
			if (retlen == 0)
				count = 0;
		}
		else {
	 		kfree(kbuf);
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return ret;
		}

	}

	kfree(kbuf);
#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_unlock();				// sl2312 share pin lock
#endif
	return total_retlen;
} /* mtd_read */

static ssize_t mtd_write(struct file *file, const char __user *buf, size_t count,loff_t *ppos)
{
	struct mtd_file_info *mfi = file->private_data;
	struct mtd_info *mtd = mfi->mtd;
	char *kbuf;
	size_t retlen;
	size_t total_retlen=0;
	int ret=0;
	int len;

#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_lock();				// sl2312 share pin lock
#endif

	DEBUG(MTD_DEBUG_LEVEL0,"MTD_write\n");

	if (*ppos == mtd->size){
#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_unlock();				// sl2312 share pin lock
#endif
		return -ENOSPC;
	}

	if (*ppos + count > mtd->size)
		count = mtd->size - *ppos;

	if (!count){
#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_unlock();				// sl2312 share pin lock
#endif
		return 0;
	}

	if (count > MAX_KMALLOC_SIZE)
		kbuf=kmalloc(MAX_KMALLOC_SIZE, GFP_KERNEL);
	else
		kbuf=kmalloc(count, GFP_KERNEL);

	if (!kbuf) {
#ifdef CONFIG_SL2312_SHARE_PIN
		mtd_unlock();				// sl2312 share pin lock
#endif
		return -ENOMEM;
	}

	while (count) {

		if (count > MAX_KMALLOC_SIZE)
			len = MAX_KMALLOC_SIZE;
		else
			len = count;

		if (copy_from_user(kbuf, buf, len)) {
			kfree(kbuf);
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}

		switch (mfi->mode) {
		case MTD_MODE_OTP_FACTORY:
			ret = -EROFS;
			break;
		case MTD_MODE_OTP_USER:
			if (!mtd->write_user_prot_reg) {
				ret = -EOPNOTSUPP;
				break;
			}
			ret = mtd->write_user_prot_reg(mtd, *ppos, len, &retlen, kbuf);
			break;

		case MTD_MODE_RAW:
		{
			struct mtd_oob_ops ops;

			ops.mode = MTD_OOB_RAW;
			ops.datbuf = kbuf;
			ops.oobbuf = NULL;
			ops.len = len;

			ret = mtd->write_oob(mtd, *ppos, &ops);
			retlen = ops.retlen;
			break;
		}

		default:
			ret = (*(mtd->write))(mtd, *ppos, len, &retlen, kbuf);
		}
		if (!ret) {
			*ppos += retlen;
			total_retlen += retlen;
			count -= retlen;
			buf += retlen;
		}
		else {
			kfree(kbuf);
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return ret;
		}
	}

	kfree(kbuf);
#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_unlock();				// sl2312 share pin lock
#endif
	return total_retlen;
} /* mtd_write */

/*======================================================================

    IOCTL calls for getting device parameters.

======================================================================*/
static void mtdchar_erase_callback (struct erase_info *instr)
{
	wake_up((wait_queue_head_t *)instr->priv);
}

#if defined(CONFIG_MTD_OTP) || defined(CONFIG_MTD_ONENAND_OTP)
static int otp_select_filemode(struct mtd_file_info *mfi, int mode)
{
	struct mtd_info *mtd = mfi->mtd;
	int ret = 0;

	switch (mode) {
	case MTD_OTP_FACTORY:
		if (!mtd->read_fact_prot_reg)
			ret = -EOPNOTSUPP;
		else
			mfi->mode = MTD_MODE_OTP_FACTORY;
		break;
	case MTD_OTP_USER:
		if (!mtd->read_fact_prot_reg)
			ret = -EOPNOTSUPP;
		else
			mfi->mode = MTD_MODE_OTP_USER;
		break;
	default:
		ret = -EINVAL;
	case MTD_OTP_OFF:
		break;
	}
	return ret;
}
#else
# define otp_select_filemode(f,m)	-EOPNOTSUPP
#endif

static int mtd_ioctl(struct inode *inode, struct file *file,
		     u_int cmd, u_long arg)
{
	struct mtd_file_info *mfi = file->private_data;
	struct mtd_info *mtd = mfi->mtd;
	void __user *argp = (void __user *)arg;
	int ret = 0;
	u_long size;
	struct mtd_info_user info;

#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_lock();				// sl2312 share pin lock
#endif

	DEBUG(MTD_DEBUG_LEVEL0, "MTD_ioctl\n");

	size = (cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
	if (cmd & IOC_IN) {
		if (!access_ok(VERIFY_READ, argp, size))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}
	}
	if (cmd & IOC_OUT) {
		if (!access_ok(VERIFY_WRITE, argp, size))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}
	}

	switch (cmd) {
	case MEMGETREGIONCOUNT:
		if (copy_to_user(argp, &(mtd->numeraseregions), sizeof(int)))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}
		break;

	case MEMGETREGIONINFO:
	{
		struct region_info_user ur;

		if (copy_from_user(&ur, argp, sizeof(struct region_info_user))) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}

		if (ur.regionindex >= mtd->numeraseregions) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EINVAL;
		}
		if (copy_to_user(argp, &(mtd->eraseregions[ur.regionindex]),
				sizeof(struct mtd_erase_region_info))) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}
		break;
	}

	case MEMGETINFO:
		info.type	= mtd->type;
		info.flags	= mtd->flags;
		info.size	= mtd->size;
		info.erasesize	= mtd->erasesize;
		info.writesize	= mtd->writesize;
		info.oobsize	= mtd->oobsize;
		/* The below fields are obsolete */
		info.ecctype	= -1;
		info.eccsize	= 0;
		if (copy_to_user(argp, &info, sizeof(struct mtd_info_user)))
			return -EFAULT;
		break;

	case MEMERASE:
	{
		struct erase_info *erase;

		if(!(file->f_mode & 2))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EPERM;
		}

		erase=kzalloc(sizeof(struct erase_info),GFP_KERNEL);
		if (!erase)
			ret = -ENOMEM;
		else {
			wait_queue_head_t waitq;
			DECLARE_WAITQUEUE(wait, current);

			init_waitqueue_head(&waitq);

			if (copy_from_user(&erase->addr, argp,
				    sizeof(struct erase_info_user))) {
				kfree(erase);
#ifdef CONFIG_SL2312_SHARE_PIN
				mtd_unlock();				// sl2312 share pin lock
#endif
				return -EFAULT;
			}
			erase->mtd = mtd;
			erase->callback = mtdchar_erase_callback;
			erase->priv = (unsigned long)&waitq;

			/*
			  FIXME: Allow INTERRUPTIBLE. Which means
			  not having the wait_queue head on the stack.

			  If the wq_head is on the stack, and we
			  leave because we got interrupted, then the
			  wq_head is no longer there when the
			  callback routine tries to wake us up.
			*/
			ret = mtd->erase(mtd, erase);
			if (!ret) {
				set_current_state(TASK_UNINTERRUPTIBLE);
				add_wait_queue(&waitq, &wait);
				if (erase->state != MTD_ERASE_DONE &&
				    erase->state != MTD_ERASE_FAILED)
					schedule();
				remove_wait_queue(&waitq, &wait);
				set_current_state(TASK_RUNNING);

				ret = (erase->state == MTD_ERASE_FAILED)?-EIO:0;
			}
			kfree(erase);
		}
		break;
	}

	case MEMWRITEOOB:
	{
		struct mtd_oob_buf buf;
		struct mtd_oob_ops ops;

		if(!(file->f_mode & 2)) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EPERM;
		}

		if (copy_from_user(&buf, argp, sizeof(struct mtd_oob_buf))) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}

		if (buf.length > 4096) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EINVAL;
		}

		if (!mtd->write_oob)
			ret = -EOPNOTSUPP;
		else
			ret = access_ok(VERIFY_READ, buf.ptr,
					buf.length) ? 0 : EFAULT;

		if (ret) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return ret;
		}

		ops.ooblen = buf.length;
		ops.ooboffs = buf.start & (mtd->oobsize - 1);
		ops.datbuf = NULL;
		ops.mode = MTD_OOB_PLACE;

		if (ops.ooboffs && ops.ooblen > (mtd->oobsize - ops.ooboffs))
			return -EINVAL;

		ops.oobbuf = kmalloc(buf.length, GFP_KERNEL);
		if (!ops.oobbuf)
			return -ENOMEM;

		if (copy_from_user(ops.oobbuf, buf.ptr, buf.length)) {
			kfree(ops.oobbuf);
			return -EFAULT;
		}

		buf.start &= ~(mtd->oobsize - 1);
		ret = mtd->write_oob(mtd, buf.start, &ops);

		if (copy_to_user(argp + sizeof(uint32_t), &ops.oobretlen,
				 sizeof(uint32_t)))
			ret = -EFAULT;

		kfree(ops.oobbuf);
		break;

	}

	case MEMREADOOB:
	{
		struct mtd_oob_buf buf;
		struct mtd_oob_ops ops;

		if (copy_from_user(&buf, argp, sizeof(struct mtd_oob_buf))) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}

		if (buf.length > 4096) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EINVAL;
		}

		if (!mtd->read_oob) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			ret = -EOPNOTSUPP;
		}
		else
			ret = access_ok(VERIFY_WRITE, buf.ptr,
					buf.length) ? 0 : -EFAULT;
		if (ret) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return ret;
		}

		ops.ooblen = buf.length;
		ops.ooboffs = buf.start & (mtd->oobsize - 1);
		ops.datbuf = NULL;
		ops.mode = MTD_OOB_PLACE;

		if (ops.ooboffs && ops.ooblen > (mtd->oobsize - ops.ooboffs))
			return -EINVAL;

		ops.oobbuf = kmalloc(buf.length, GFP_KERNEL);
		if (!ops.oobbuf)
			return -ENOMEM;

		buf.start &= ~(mtd->oobsize - 1);
		ret = mtd->read_oob(mtd, buf.start, &ops);

		if (put_user(ops.oobretlen, (uint32_t __user *)argp))
			ret = -EFAULT;
		else if (ops.oobretlen && copy_to_user(buf.ptr, ops.oobbuf,
						    ops.oobretlen))
			ret = -EFAULT;

		kfree(ops.oobbuf);
		break;
	}

	case MEMLOCK:
	{
		struct erase_info_user info;

		if (copy_from_user(&info, argp, sizeof(info)))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}

		if (!mtd->lock)
			ret = -EOPNOTSUPP;
		else
			ret = mtd->lock(mtd, info.start, info.length);
		break;
	}

	case MEMUNLOCK:
	{
		struct erase_info_user info;

		if (copy_from_user(&info, argp, sizeof(info)))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}

		if (!mtd->unlock)
			ret = -EOPNOTSUPP;
		else
			ret = mtd->unlock(mtd, info.start, info.length);
		break;
	}

	/* Legacy interface */
	case MEMGETOOBSEL:
	{
		struct nand_oobinfo oi;

		if (!mtd->ecclayout)
			return -EOPNOTSUPP;
		if (mtd->ecclayout->eccbytes > ARRAY_SIZE(oi.eccpos))
			return -EINVAL;

		oi.useecc = MTD_NANDECC_AUTOPLACE;
		memcpy(&oi.eccpos, mtd->ecclayout->eccpos, sizeof(oi.eccpos));
		memcpy(&oi.oobfree, mtd->ecclayout->oobfree,
		       sizeof(oi.oobfree));
		oi.eccbytes = mtd->ecclayout->eccbytes;

		if (copy_to_user(argp, &oi, sizeof(struct nand_oobinfo)))
			return -EFAULT;
		break;
	}

	case MEMGETBADBLOCK:
	{
		loff_t offs;

		if (copy_from_user(&offs, argp, sizeof(loff_t)))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}
		if (!mtd->block_isbad)
			ret = -EOPNOTSUPP;
		else
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return mtd->block_isbad(mtd, offs);
		}
		break;
	}

	case MEMSETBADBLOCK:
	{
		loff_t offs;

		if (copy_from_user(&offs, argp, sizeof(loff_t)))
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}
		if (!mtd->block_markbad)
			ret = -EOPNOTSUPP;
		else
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return mtd->block_markbad(mtd, offs);
		}
		break;
	}

#if defined(CONFIG_MTD_OTP) || defined(CONFIG_MTD_ONENAND_OTP)
	case OTPSELECT:
	{
		int mode;
		if (copy_from_user(&mode, argp, sizeof(int))) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}

		mfi->mode = MTD_MODE_NORMAL;

		ret = otp_select_filemode(mfi, mode);

		file->f_pos = 0;
		break;
	}

	case OTPGETREGIONCOUNT:
	case OTPGETREGIONINFO:
	{
		struct otp_info *buf = kmalloc(4096, GFP_KERNEL);
		if (!buf)
		{
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -ENOMEM;
		}
		ret = -EOPNOTSUPP;
		switch (mfi->mode) {
		case MTD_MODE_OTP_FACTORY:
			if (mtd->get_fact_prot_info)
				ret = mtd->get_fact_prot_info(mtd, buf, 4096);
			break;
		case MTD_MODE_OTP_USER:
			if (mtd->get_user_prot_info)
				ret = mtd->get_user_prot_info(mtd, buf, 4096);
			break;
		default:
			break;
		}
		if (ret >= 0) {
			if (cmd == OTPGETREGIONCOUNT) {
				int nbr = ret / sizeof(struct otp_info);
				ret = copy_to_user(argp, &nbr, sizeof(int));
			} else
				ret = copy_to_user(argp, buf, ret);
			if (ret)
				ret = -EFAULT;
		}
		kfree(buf);
		break;
	}

	case OTPLOCK:
	{
		struct otp_info info;

		if (mfi->mode != MTD_MODE_OTP_USER) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EINVAL;
		}
		if (copy_from_user(&info, argp, sizeof(info))) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EFAULT;
		}
		if (!mtd->lock_user_prot_reg) {
#ifdef CONFIG_SL2312_SHARE_PIN
			mtd_unlock();				// sl2312 share pin lock
#endif
			return -EOPNOTSUPP;
		}
		ret = mtd->lock_user_prot_reg(mtd, info.start, info.length);
		break;
	}
#endif

	case ECCGETLAYOUT:
	{
		if (!mtd->ecclayout)
			return -EOPNOTSUPP;

		if (copy_to_user(argp, mtd->ecclayout,
				 sizeof(struct nand_ecclayout)))
			return -EFAULT;
		break;
	}

	case ECCGETSTATS:
	{
		if (copy_to_user(argp, &mtd->ecc_stats,
				 sizeof(struct mtd_ecc_stats)))
			return -EFAULT;
		break;
	}

	case MTDFILEMODE:
	{
		mfi->mode = 0;

		switch(arg) {
		case MTD_MODE_OTP_FACTORY:
		case MTD_MODE_OTP_USER:
			ret = otp_select_filemode(mfi, arg);
			break;

		case MTD_MODE_RAW:
			if (!mtd->read_oob || !mtd->write_oob) {
#ifdef CONFIG_SL2312_SHARE_PIN
				mtd_unlock();				// sl2312 share pin lock
#endif
				return -EOPNOTSUPP;
			}
			mfi->mode = arg;

		case MTD_MODE_NORMAL:
			break;
		default:
			ret = -EINVAL;
		}
		file->f_pos = 0;
		break;
	}

	default:
		ret = -ENOTTY;
	}

#ifdef CONFIG_SL2312_SHARE_PIN
	mtd_unlock();				// sl2312 share pin lock
#endif

	return ret;
} /* memory_ioctl */

static const struct file_operations mtd_fops = {
	.owner		= THIS_MODULE,
	.llseek		= mtd_lseek,
	.read		= mtd_read,
	.write		= mtd_write,
	.ioctl		= mtd_ioctl,
	.open		= mtd_open,
	.release	= mtd_close,
};

static int __init init_mtdchar(void)
{
	if (register_chrdev(MTD_CHAR_MAJOR, "mtd", &mtd_fops)) {
		printk(KERN_NOTICE "Can't allocate major number %d for Memory Technology Devices.\n",
		       MTD_CHAR_MAJOR);
		return -EAGAIN;
	}

#ifdef CONFIG_DEVFS_FS
	mtdchar_devfs_init();
#else
	mtd_class = class_create(THIS_MODULE, "mtd");

	if (IS_ERR(mtd_class)) {
		printk(KERN_ERR "Error creating mtd class.\n");
		unregister_chrdev(MTD_CHAR_MAJOR, "mtd");
		return PTR_ERR(mtd_class);
	}

	register_mtd_user(&notifier);
#endif
	return 0;
}

static void __exit cleanup_mtdchar(void)
{

#ifdef CONFIG_DEVFS_FS
	mtdchar_devfs_exit();
#else
	unregister_mtd_user(&notifier);
	class_destroy(mtd_class);
#endif
	unregister_chrdev(MTD_CHAR_MAJOR, "mtd");
}

module_init(init_mtdchar);
module_exit(cleanup_mtdchar);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Direct character-device access to MTD devices");
