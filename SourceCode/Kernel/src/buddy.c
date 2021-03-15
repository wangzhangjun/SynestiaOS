#include "libc/stdint.h"
#include "arm/page.h"

#define offsetof(TYPE, MEMBER) ((unsigned int) &((TYPE *) 0)->MEMBER)
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
		(type *)( (char *)__mptr - offsetof(type,member) ); })
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

struct list_head {
    struct list_head *next, *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *new_lst,
                              struct list_head *prev,
                              struct list_head *next) {
    next->prev = new_lst;
    new_lst->next = next;
    new_lst->prev = prev;
    prev->next = new_lst;
}

static inline void list_add(struct list_head *new_lst, struct list_head *head) {
    __list_add(new_lst, head, head->next);
}

static inline void list_add_tail(struct list_head *new_lst, struct list_head *head) {
    __list_add(new_lst, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next) {
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry) {
    __list_del(entry->prev, entry->next);
}

static inline void list_remove_chain(struct list_head *ch, struct list_head *ct) {
    ch->prev->next = ct->next;
    ct->next->prev = ch->prev;
}

static inline void list_add_chain(struct list_head *ch, struct list_head *ct, struct list_head *head) {
    ch->prev = head;
    ct->next = head->next;
    head->next->prev = ct;
    head->next = ch;
}

static inline void list_add_chain_tail(struct list_head *ch, struct list_head *ct, struct list_head *head) {
    ch->prev = head->prev;
    head->prev->next = ch;
    head->prev = ct;
    ct->next = head;
}

static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}

struct page {
    unsigned int vaddr;
    unsigned int flags;
    int order;
    unsigned int counter;
    struct kmem_cache *cachep;
    struct list_head list;//to string the buddy member
};

#define PAGE_SHIFT (12)//一页是4k
#define PAGE_MASK (~(PAGE_SIZE - 1))

#define _MEM_START (uint32_t) & __KERNEL_END + PAGE_SIZE//伙伴算法能够操作的开始的位置
#define _MEM_SIZE KERNEL_PHYSICAL_SIZE - (uint32_t) &__KERNEL_END
#define _MEM_END _MEM_START + _MEM_SIZE//伙伴算法能够操作的结束的位置

#define KERNEL_MEM_END (_MEM_END)
#define KERNEL_PAGING_START ((_MEM_START + (~PAGE_MASK)) & ((PAGE_MASK)))                                                                   //_MEM_START的值按照PAGE_SIZE对齐，不对齐则取整，自动对齐
#define KERNEL_PAGING_END (((KERNEL_MEM_END - KERNEL_PAGING_START) / (PAGE_SIZE + sizeof(struct page))) * (PAGE_SIZE) + KERNEL_PAGING_START)//在这段内存里面不仅要存放页还要存放struct page,这个其实是页存放的结束地址，后面还有struct page
#define KERNEL_PAGE_NUM ((KERNEL_PAGING_END - KERNEL_PAGING_START) / (1 << PAGE_SHIFT))//页是多少个

#define KERNEL_PAGE_START (KERNEL_PAGE_END - KERNEL_PAGE_NUM * sizeof(struct page))//存放struct page结构体的开始的地方
#define KERNEL_PAGE_END _MEM_END                                                   //struct page的结束的地方就是_MEM_END

#define MAX_BUDDY_PAGE_NUM (9)                               //先存放10个链表，2的10次方，一次最多分配2m的大小的内存
#define PAGE_NUM_FOR_MAX_BUDDY ((1 << MAX_BUDDY_PAGE_NUM) - 1)//最大数组的struct page的个数

/*page flags*/
#define PAGE_AVAILABLE 0x00
#define PAGE_DIRTY 0x01
#define PAGE_PROTECT 0x02
#define PAGE_BUDDY_BUSY 0x04
#define PAGE_IN_CACHE 0x08

#define BUDDY_END(x, order) ((x) + (1 << (order)) - 1)//x是一个page的地址，然后往后跳几个，跳到第一个buddy的末尾，比如2的2次方，list_entry得到的是第一个Buddy中的第一个page的值，那要想得到第一个buddy中的最后一个page的值，就是加上2的2次方减1
#define NEXT_BUDDY_START(x, order) ((x) + (1 << (order)))
#define PREV_BUDDY_START(x, order) ((x) - (1 << (order)))

struct list_head page_buddy[MAX_BUDDY_PAGE_NUM];
extern uint32_t __KERNEL_END;

//初始化所有的buddy数组
void init_page_buddy(void) {
    int i;
    for (i = 0; i < MAX_BUDDY_PAGE_NUM; i++) {
        INIT_LIST_HEAD(&page_buddy[i]);
    }
}

void init_page_map(void) {
    init_page_buddy();
    struct page *pg = (struct page *) KERNEL_PAGE_START;//第一个struct page开始的地方
    for (int i = 0; i < KERNEL_PAGE_NUM; i++) {
        pg->vaddr = KERNEL_PAGING_START + i * PAGE_SIZE;//第一个struct page对应的地方就是第一个页的地址，第二个，第三个往下推
        pg->flags = PAGE_AVAILABLE;                     //flags，标志页和结构体
        pg->counter = 0;                                //表示该页被使用的次数
        INIT_LIST_HEAD(&(pg->list));
        if (i < (KERNEL_PAGE_NUM & (~PAGE_NUM_FOR_MAX_BUDDY))) {
            if ((i & PAGE_NUM_FOR_MAX_BUDDY) == 0) {
                pg->order = MAX_BUDDY_PAGE_NUM - 1;
            } else {
                pg->order = -1;
            }
            list_add_tail(&(pg->list), &page_buddy[MAX_BUDDY_PAGE_NUM - 1]);
        } else {
            //不足以分配时，分配到最小的0链上
            pg->order = 0;
            list_add_tail(&(pg->list), &page_buddy[0]);
        }
    }
}

//buddy的申请和释放
struct page* get_pages_from_list(int order)
{
    unsigned int vaddr;
    int neworder = order;
    struct page *pg;
    struct list_head *tlst, *tlst1;

    for(;neworder < MAX_BUDDY_PAGE_NUM; neworder++){
        if(list_empty(&page_buddy[neworder])) {
            continue;  //如果该链表是空的，则查找下一个链表
        }else{
            //找到了之后，就返回这个page，然后再把链表续上
            pg = list_entry(page_buddy[neworder].next, struct page, list);
            tlst = &(BUDDY_END(pg, neworder)->list);
            tlst->next->prev = &page_buddy[neworder];//这两句的意思是把找到的buddy拆下来，前后再接上
            page_buddy[neworder].next = tlst->next;
            goto OUT_OK;
        }
    }
    return nullptr;

OUT_OK:
    //如果说是从比本来要申请的order大的链表上申请下来的，那么就要拆分再放到不同的链表中
    for (neworder--; neworder >= order; neworder--){

        //拆分出比order小的第一个链表的buddy
        tlst1 = &(BUDDY_END(pg, neworder)->list);  
        tlst = &(pg->list);  

        pg = NEXT_BUDDY_START(pg, neworder);  //pg跳到上面拆分剩下的开始
        list_entry(tlst, struct page, list)->order = neworder;
        list_add_chain_tail(tlst, tlst1, &page_buddy[neworder]);
    }
    //如果说刚好就在目标order上找到了，就返回pg
    pg->flags |= PAGE_BUDDY_BUSY;
    pg->order = order;
    
    return pg;
}

struct page*  alloc_pages(int order, uint32_t flag)
{
    struct page *pg = get_pages_from_list(order);
    if(pg == nullptr) 
        return nullptr;
    for(int i = 0; i < (1 << order); i++) {
       (pg + i)->flags |= PAGE_DIRTY; 
    }
 
    return pg;
};

void * get_free_pages(int order, uint32_t flag)
{
    struct page *page;
    page = alloc_pages(flag, order);
    if (!page) return nullptr;
    return (void *)(page->vaddr);
}

void init_buddy_alloc(uint32_t base, uint32_t size) {
    printf("in buddy base : %x\n", base);
    printf("in buddy base+size : %x\n", base + size);
    printf("in buddy size : %x\n", size);//171M 内存可用
    printf("in buddy _MEM_START : %x\n", _MEM_START);
    printf("in buddy _MEM_END : %x\n", _MEM_END);
    printf("in buddy page start : %x\n", KERNEL_PAGING_START);
    printf("in buddy page end : %x\n", KERNEL_PAGING_END);
    printf("in buddy KERNEL_PAGE_NUM : %x\n", KERNEL_PAGE_NUM);
    init_page_map();
    char *pageAddr = (char *)get_free_pages(6,0);
    printf("buddy return address is : %x\n", pageAddr);
}