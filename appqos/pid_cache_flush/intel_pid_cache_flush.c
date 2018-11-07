/*
 *   Copyright (C) 2019 Intel Corporation
 *
 *   This software is licensed under
 *   (a) a 3-clause BSD license; or alternatively
 *   (b) the GPL v2 license
 *
 *   -- A. BSD-3-Clause ----------------------------
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER
 *   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *   -- B. GPL-2.0 ----------------------------
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License, version 2, as published
 *   by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 *   ------------------------------
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/tlbflush.h>
#include <linux/highmem.h>
#include <linux/proc_fs.h>
#include <linux/pid.h>

/* define for additional TBL flush - used in some kernel flushing code
 * when using that option LLC occupancy is lower in jailing scenarios
 */
#define TBL_FLUSH_ALL (0)	/* disabled because of risk of interrupting
				 * other processes on other cores
				 */

#define PRINT_DEBUG (1)

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#else
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#endif

#if KERNEL_VERSION(3, 9, 0) > LINUX_VERSION_CODE
#error "Requires kernel v3.9 or newer"
#endif

#if KERNEL_VERSION(4, 11, 0) < LINUX_VERSION_CODE
#include <linux/sched/signal.h>	/*for_each_process - moved to separate header */
#endif

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Module for flushing cache content for target PID process");

#define PROCFS_IPCF_NAME ("intel_pid_cache_flush")
#define PROCFS_IPCF_BUF_MAX_SIZE (256)	/* size of the buff with PID to flush */
#define FLUSH_STEP (64)	/* It is amount of physical pages that are
			 * retrieved at once in order
			 * (NOTE: has to be power of 2, 1 also
			 * works fine here)
			 */

static struct proc_dir_entry *ipcf_proc_file;

/**
 * @brief Compares two strings
 *        Used to compare memory area names
 *
 * @param [in] str1 1st string
 * @param [in] len1 1st string length
 * @param [in] str2 2nd string
 * @param [in] len2 2nd string length
 *
 * @return Operations status
 * @retval 0 when equal
 */
static int namecmp(const char *str1, int len1, const char *str2, int len2)
{
	int minlen;
	int cmp;

	minlen = len1;
	if (minlen > len2)
		minlen = len2;

	cmp = memcmp(str1, str2, minlen);
	if (cmp == 0)
		cmp = len1 - len2;

	return cmp;
}

/**
 * @brief Indicate if the VMA is a stack for the given task;
 *        for /proc/PID/maps that is the stack of the main task.
 *
 * @param [in] vma VM area to be tested
 *
 * @return Operations status
 */
static int is_stack(struct vm_area_struct *vma)
{
	return (vma->vm_start <= vma->vm_mm->start_stack) &&
		(vma->vm_end >= vma->vm_mm->start_stack);
}

#define MEM_NAME_VDSO ("[vdso]")
#define MEM_NAME_HEAP ("[heap]")
#define MEM_NAME_STACK ("[stack]")
#define MEM_NAME_EMPTY ("")

/**
 * @brief Flushes task memory for given address.
 *
 * @param [in] task task info
 * @param [in] start_user_addr starting address
 * @param [in] max_num_pages max number of pages to flush
 *
 * @return amount of flushed pages
 * @retval 0/-ERRNO on error
 */
#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE
static long flush_phys_page_for_addr(struct task_struct *task,
	unsigned long start_user_addr, unsigned long max_num_pages)
{
	long num_pages;
	struct page *pages[max_num_pages];

	if (!task->mm) {
		pr_err("task->mm not found, returning\n");
		return 0;
	}

	down_read(&(task->mm->mmap_sem));
	num_pages =
		get_user_pages(NULL, task->mm, start_user_addr, max_num_pages,
		0 /* ro */, 0 /* no force */, pages, NULL);

	if (num_pages > 0) {
		void *page_addr;
		long i;

		for (i = 0; i < num_pages; ++i) {
			page_addr = kmap(pages[i]);
			clflush_cache_range(page_addr, PAGE_SIZE);
			kunmap(pages[i]);
		}
	}

	/* silently ignoring unaccessible pages,
	 * cannot do anything with that now anyway
	 */

	up_read(&(task->mm->mmap_sem));

	return num_pages;
}
#else /* kernel > 4.5.0 */
static long flush_phys_page_for_addr(struct task_struct *task,
	unsigned long start_user_addr,
	unsigned long max_num_pages)
{
	long num_pages;
	struct page *pages[max_num_pages];

	down_read(&(task->mm->mmap_sem));
#if KERNEL_VERSION(4, 10, 0) < LINUX_VERSION_CODE
	num_pages =
		get_user_pages_remote(task, task->mm, start_user_addr,
		max_num_pages, 0 /* ro */,
		pages, NULL, NULL);
#else
	/* using older API with less params at the end (no 'locked' param) */
	num_pages =
		get_user_pages_remote(task, task->mm, start_user_addr,
		max_num_pages, 0 /* ro */,
		pages, NULL);
#endif
	up_read(&(task->mm->mmap_sem));

	if (num_pages > 0) {
		long i;

		for (i = 0; i < num_pages; ++i) {
			void *page_addr = kmap(pages[i]);

			clflush_cache_range(page_addr, PAGE_SIZE);
			kunmap(pages[i]);
			put_page(pages[i]);
		}
	}

	/* silently ignoring unaccessible pages,
	 * cannot do anything with that now anyway
	 */

	return num_pages;
}
#endif

/**
 * @brief Flushes cache used by task.
 *
 * @param [in] task task info
 * @param [in] start start of block of memory to flush
 * @param [in] end end of block of memory to flush
 */
static void flush_cache(struct task_struct *task, unsigned long start,
	unsigned long end)
{
	long bytes_to_flush = end > start ? end - start : start - end;
	const long pages_to_flush = bytes_to_flush / PAGE_SIZE;

	if (access_ok(VERIFY_READ, start, bytes_to_flush)) {
		/* iterating on all pages in order to get physical addresses.
		 * If addr has been found - flushing cache.
		 * If not, trying the next page in the memory area.
		 * Cleaning 'flush_step_size' amount by single step,
		 * value of this step is set to '1' in case of non-existing
		 * pages in order to do not skip too much memory to flush
		 */
		long flush_step_size = (pages_to_flush > FLUSH_STEP) ?
			FLUSH_STEP : pages_to_flush;
		long i;
		long long bytes_flushed = 0;

		for (i = 0; i < pages_to_flush; i += flush_step_size) {
			long pages_flushed =
				flush_phys_page_for_addr(task, start,
				flush_step_size);
			bytes_flushed += pages_flushed > 0 ?
				(pages_flushed * PAGE_SIZE) : 0;

			if (pages_flushed <= 0 && flush_step_size > 1) {
				/* decreasing flush_step_size to '1'
				 * to do not jump over free pages
				 */
				pr_debug("Changing flush_step_size from %ld to 1 page at once",
					flush_step_size);
				flush_step_size = 1;
			}
			start += (flush_step_size * PAGE_SIZE);
		}
		pr_debug("Flushed %lld bytes", bytes_flushed);
	}
}

/**
 * @brief Flushes task memory.
 *        Walks through task's VM areas and flushes only flushable ones.
 *
 * @param [in] task task info
 * @param [in] mmap VM area to walk through
 */
static void walk_vm_area(struct task_struct *task, struct vm_area_struct *mmap)
{
	struct vm_area_struct *vma;

	for (vma = mmap; vma != NULL; vma = vma->vm_next) {
		struct mm_struct *mm = vma->vm_mm;
		unsigned long chunk_size =
			(unsigned long) abs(vma->vm_end - vma->vm_start);
		struct file *file = vma->vm_file;
		unsigned long long pgoff = 0;
		unsigned long ino = 0;
		dev_t dev = 0;
		const char *mem_name = MEM_NAME_EMPTY;

		if (file) {
			struct inode *inode;

			inode = file_inode(file);
			dev = inode->i_sb->s_dev;
			ino = inode->i_ino;
			pgoff = ((loff_t) vma->vm_pgoff) << PAGE_SHIFT;
		}

#if KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
		if (vma->vm_ops && vma->vm_ops->name) {
			mem_name = vma->vm_ops->name(vma);
		} else
#endif
		if (!mm) {
			mem_name = MEM_NAME_VDSO;
		} else if (vma->vm_start <= mm->brk &&
			vma->vm_end >= mm->start_brk) {
			mem_name = MEM_NAME_HEAP;
		} else if (is_stack(vma)) {
			mem_name = MEM_NAME_STACK;
		}

		if (dev != 0)
			continue;

		if (ino != 0)
			continue;

		if (namecmp(mem_name, strlen(mem_name),
			MEM_NAME_VDSO, strlen(MEM_NAME_VDSO)) == 0) {
			pr_debug("NOTE: Ignoring vdso region\n");
			continue;
		}

		pr_debug("0x%lx - 0x%lx; Size: %ld (0x%lx) : %08llx %02x:%02x %lu %s\n",
			vma->vm_start, vma->vm_end, chunk_size, chunk_size,
			pgoff, MAJOR(dev), MINOR(dev), ino, mem_name);

		flush_cache(task, vma->vm_start, vma->vm_end);
	}
}

/**
 * @brief flush cache for given PID
 *
 * @param [in] pid PID to be flushed
 */
static void flush_cache_for_pid(pid_t pid)
{
	struct pid *pid_s;
	struct task_struct *task;

	pid_s = find_get_pid(pid);
	if (!pid_s) {
		pr_err("Could not find pid %d\n", pid);
		return;
	}

	task = pid_task(pid_s, PIDTYPE_PID);
	if (!task) {
		pr_err("Error with getting pid task for pid %d", pid);
		return;
	}

	pr_info("%s [%d]\n", task->comm, task->pid);

	if (task->mm && task->mm->mmap) { /* vm_area struct*/
		walk_vm_area(task, task->mm->mmap);

#if (TBL_FLUSH_ALL)
		pr_info("Flushing TBL\n");
		__flush_tlb_all();
#endif
	}
}

/**
 * @brief Callback, handles procfs file write from userspace
 *
 * @param [in] file unused
 * @param [in] user_buffer data from user space
 * @param [in] count size of user space data
 * @param [in] data unused
 *
 * @return number of processed bytes
 * @retval -EINVAL on error
 */
static ssize_t ipcf_proc_write(struct file *file, const char *user_buffer,
	size_t count, loff_t *data)
{
	long target_pid;
	int res;
	unsigned long bytes_not_copied;
	char ipcf_procfs_buf[PROCFS_IPCF_BUF_MAX_SIZE];

	if (!user_buffer)
		return -EINVAL;

	if (!count)
		return -EINVAL;

	if (count > PROCFS_IPCF_BUF_MAX_SIZE)
		count = PROCFS_IPCF_BUF_MAX_SIZE;

	memset(ipcf_procfs_buf, 0, PROCFS_IPCF_BUF_MAX_SIZE);
	bytes_not_copied = copy_from_user(ipcf_procfs_buf, user_buffer, count);

	if (bytes_not_copied > 0) {
		pr_err("Copy from user failed");
		return -EINVAL;
	}

	if (count < 1) { /* at least have to be 1 chars: PID number */
		pr_err("Bad format string passed to module: %s",
			ipcf_procfs_buf);
		return -EINVAL;
	}

	res = kstrtol((ipcf_procfs_buf), 10, &target_pid);
	if (res != 0) {
		pr_err("Could not parse PID passed to flushing module: %s",
			ipcf_procfs_buf);
		return -EINVAL;
	}

	flush_cache_for_pid((pid_t) target_pid);

	return (ssize_t) count;
}

static const struct file_operations ipcf_fops = {
	.owner = THIS_MODULE,
	.write = ipcf_proc_write,
};

/**
 * @brief Module init function
 *
 * @return Operation status
 * @retval 0 on success
 * @retval -ENOMEM on error
 */
static int __init intel_pid_cache_flush_init(void)
{
	pr_info("Cache flush init\n");

	ipcf_proc_file = proc_create(PROCFS_IPCF_NAME, 0644, NULL, &ipcf_fops);
	if (ipcf_proc_file == NULL) {
		pr_alert("Error: could not initialize /proc/%s\n",
			PROCFS_IPCF_NAME);
		return -ENOMEM;
	}

	pr_info("/proc/%s created\n", PROCFS_IPCF_NAME);
	return 0;
}

/**
 * @brief Module exit function
 */
static void __exit intel_pid_cache_flush_exit(void)
{
	remove_proc_entry(PROCFS_IPCF_NAME, NULL);
	pr_info("/proc/%s removed\n", PROCFS_IPCF_NAME);
}

module_init(intel_pid_cache_flush_init);
module_exit(intel_pid_cache_flush_exit);
