/*
 * elevator sstf
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct sstf_data {
	struct list_head queue;
	sector_t cur_header_pos;
};

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *nd = q->elevator->elevator_data;
	sector_t min = 1<<31;
	
	if (!list_empty(&nd->queue)) {
		struct list_head *list;
		struct request *rq;
		sector_t cur_pos;
		
		//Scan the whole request list to find the minimized value
		list_for_each(list, &nd->queue) {
			rq = list_entry(list, struct request, queuelist);

			cur_pos = blk_rq_pos(rq);
			
			if (cur_pos > (nd->cur_header_pos)) {
				printk(KERN_DEBUG "[SSTF]%lu\t",(unsigned long)(cur_pos-nd->cur_header_pos));
				
				if (min > (cur_pos - nd->cur_header_pos))
					min = cur_pos - nd->cur_header_pos;
			}
			else {
				printk(KERN_DEBUG "[SSTF]%lu\t", (unsigned long)(nd->cur_header_pos-cur_pos));
				
				if (min > (nd->cur_header_pos - cur_pos))
					min = nd->cur_header_pos - cur_pos;
			}
		}

		printk(KERN_DEBUG "\n[SSTF]Current minimized value is: %lu\n[SSTF]diff: ", (unsigned long)min);
		
		//Find the minimized node in request queue and break
		list_for_each(list, &nd->queue) {
			sector_t diff;

			rq = list_entry(list, struct request, queuelist);

			cur_pos = blk_rq_pos(rq);

			if (cur_pos > (nd->cur_header_pos)) {
				diff = cur_pos - nd->cur_header_pos;
			}
			else {
				diff = nd->cur_header_pos - cur_pos;
			}

			printk(KERN_DEBUG "[SSTF]%lu\t", (unsigned long)diff);
			
			if (diff == min) {
				break;
			}
		}

		rq = list_entry(list, struct request, queuelist);
		list_del_init(&rq->queuelist);
		
		printk(KERN_DEBUG "\n[SSTF]Before elv_dispatch_sort cur_pos: %lu\n", (unsigned long)cur_pos);

		elv_dispatch_sort(q, rq);

		nd->cur_header_pos = cur_pos;

		return 1;
	}

	return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	list_add_tail(&rq->queuelist, &nd->queue);
}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sstf_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= sstf_former_request,
		.elevator_latter_req_fn		= sstf_latter_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void)
{
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Chao-Ting Wen, Chih-Hsiang Wang, .Suwadi");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");

