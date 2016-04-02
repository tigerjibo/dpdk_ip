#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>//will be replaced by rte_malloc.
#include <rte_malloc.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <rte_ring.h>
#include "ipfragment.h"
#include "module.h"

#define OUTOFMEM {printf("Out of Mem!\n");return ;}
#define TABLESIZE 1024
#define IP_CE 0x8000   /* Flag: "Congestion" */
#define IP_DF 0x4000   /* Flag: "Don't Fragment" */
#define IP_MF 0x2000   /* Flag: "More Fragments" */
#define IP_OFFSET 0x1FFF  /* "Fragment Offset" part */
/*
**to do://2016.3.16
**1.add the completed fragements to rings;
**2.notify the users to get packet of rings;
**3.add ttl about the fragement;
**4.delete the completed fragements safely.
*/

/*
*to do:
*1. ��һ��ʼ��ʱ��Ͷ�ip�����н�����ȡ����Ҫ����Ϣд��ṹ����
*2. ���ڴ���䲿�ֽ����޸ģ�for ���ܣ�
*3. �����ķ�Ƭ�����в�����ipͷ��ֻ����data�ͱ�Ҫ����Ϣ�����������ipͷ
*4. 
*/

typedef struct ipstruct{
	struct hashtable tables[TABLESIZE];
	struct rte_ring * r;
} IpImpl;

struct ring_buf{
	int type;
	void * ptr;
};

//hash
int addrtoHash(struct in_addr Src, struct in_addr Dest){
		//printf("In cacu %d %d.\n",Src.s_addr,Dest.s_addr);
		return (Src.s_addr + Dest.s_addr)%1024;//need change 
}

/*globle value*/
//struct hashtable  tables[TABLESIZE];


/*
static void sentPacket(struct ipPacketHead * table){
	//rebuild the packet and sent it to pcap
	//�򱣴�һ�����ݰ��أ������������ٷ���ȥ.
	printf("addr: %s %s.\n",inet_ntoa(table -> head -> ip_src),inet_ntoa(table -> head -> ip_dst));
}*/
//�����ݼ��뵽��Ƭ�����У����У���Ƭһ����Ψһ�ģ������ظ��ܲ����ظ���һ���д���ȶ
void adddToipFra(void *handle, struct srcDstAddr * fa, struct ipPacketHead * table, struct ip *iphead, struct sk_buff * skb)
{
	if (((ntohs(iphead->ip_off)&~IP_OFFSET) & IP_MF) == 0){
		table->MF = 0;
	}
printf("2 ");
	if (table->ipFra == NULL){
		table->ipFra = (struct ipFragment *)rte_malloc("ipFra", sizeof(struct ipFragment),0);
		if (table ->ipFra == NULL ){printf("Out of Mem1!\n");return ;}
		else{
			table->ipFra->next = NULL;
			table->ipFra->seq = NULL;
			table->ipFra->skb = skb;
			table->fraSeq = table->ipFra;
			table->ipFra->length = iphead -> ip_len;
			table->ipFra->offset = ntohs(iphead->ip_off) & IP_OFFSET;

			
		}
	}
	else{
		//����Ҫ������������
		//1.��¼�����ݰ�������˳��
		//2.�����ݰ�ƫ��λ����
		//3.��¼��ɺ����Ƿ���һ�������ķ�Ƭ����
		//4.�������ͽ������������ͽ����������ݰ����С�
		struct ipFragment * current, *pre,*newFrag;
		newFrag = (struct ipFragment *)rte_malloc("Fra", sizeof(struct ipFragment),0);
		if (newFrag == NULL){printf("Out of Mem2!\n");return ;}
		else{
			//here edit the new fragment
			//to do:edit the info.

			newFrag->skb = skb;
			newFrag->seq = NULL;
			newFrag->next = NULL;
			newFrag->offset = ntohs(iphead->ip_off) & IP_OFFSET;
			newFrag->length = iphead -> ip_len;
		}
		//1.record the sequence of packet.
		current = table->fraSeq;
		pre = current;
printf("3 ");
		while (current){
			pre = current;
			current = current->seq;
		}
printf("4 ");
fflush(stdout);
		pre->seq = newFrag;
		//2.sort by offset
		current = table->ipFra;
		pre = current;
		if(current -> offset == newFrag -> offset){
			//if find the same packet, just change the next, not change the coming sequence.
			if(current -> length < newFrag -> length)
			{
				newFrag -> next = current -> next;
				table -> ipFra = newFrag;
			}
		}
		else if (current->offset > newFrag->offset){
			newFrag->next = current;
			table->ipFra = newFrag;
		}
		else{
printf("5 ");
fflush(stdout);

			while (current && current->offset < newFrag->offset)
			{

				pre = current;
				current = current -> next;
				if(current -> offset == newFrag -> offset){
				//if find the same packet, just change the next, not change the coming sequence.
					if(current -> length < newFrag -> length)
					{
						newFrag -> next = current -> next;
						pre -> next = newFrag;
					}
				}
			}
printf("6 ");
fflush(stdout);
			pre->next = newFrag;
			newFrag->next = current;
		}
		//3.judge weather the fragment is complete.
		if (table->MF == 0){//get the last fragement, need to judge now.
//int num = 0;
			printf("MF =0.\n");
			current = table->ipFra;
			pre = current;
printf("7 ");
fflush(stdout);
			while (current -> next){
				if (current->offset + current->length > current->next->offset){
					pre = current;
					current = current->next;
					/*to do :debug!*/
					printf("In loop 7.\n");
 					//if(pre == current){printf("The same.\n");return;}
				}
				else
					break;
			}
			printf("In if.\n");
			if (pre->offset + pre->length >= current->offset){
				//Job done.
				//here has two things to do.
				//1.
				//return;
				IpImpl * impl = (IpImpl *)handle;
				struct ring_buf * ptr = (struct ring_buf *)rte_malloc("ring_buf",sizeof(struct ring_buf),0);
				void **obj = rte_malloc("rp",sizeof(void *)*2,0);
				printf("In 1.\n");
				if(ptr == NULL)OUTOFMEM
				ptr -> type = 1;
				ptr -> ptr = table -> ipFra;
				obj[0] = ptr;
				rte_ring_enqueue(impl -> r, &obj[0]);
				printf("In 2.\n");
				//��߿�����ΰѷ�Ƭ���Ͽ�
				if(table -> next){
					table -> next -> pre = table -> pre;
				}//here just for test ring
				if(table -> pre){
				table -> pre -> next = table -> next;
				}else
				{
					fa -> packets = NULL;
				}
				ptr = NULL;
				rte_ring_dequeue(impl -> r, &obj[0]);
				printf("In 3.\n");
				ptr = obj[0];
				printf("In ring IP: %d %p.\n",ptr -> type, ptr -> ptr);


			}else
			printf("Job not done!\n");
			//else the fragement not completed, just continue.
		}
printf("8 ");
fflush(stdout);
	}

}
//�����ݼ��뵽���ݰ������У����У����ݰ�������ip��idΪΨһ��ʶ
void addToAddr(void *handle, struct srcDstAddr * table, struct ip * iphead, struct sk_buff *skb){
printf("3 ");
	if (table->packets == NULL){//empty packet.
		table->packets = (struct ipPacketHead *)rte_malloc("packets", sizeof(struct ipPacketHead),0);
		if (table->packets == NULL){printf("Out of Mem3!\n");return ;}
		else{

			table->packets->next = NULL;
			table->packets->pre = NULL;
			table->packets->ipFra = NULL;
			table->packets->head = iphead;
			table->packets->MF = 1;
			adddToipFra(handle, table, table->packets, iphead, skb);
		}
	}
	else{
		struct ipPacketHead * current, *pre;
		current = table->packets;
		pre = current;
		while (current){
			if (current->head->ip_id == iphead->ip_id){//two fragment of one packet.
				adddToipFra(handle, table, current, iphead, skb);
				break;
			}
			else{
				pre = current;
				current = current->next;
			}
			if (current == NULL)
			{
				pre->next = (struct ipPacketHead *)rte_malloc("ipPacket", sizeof(struct ipPacketHead),0);
				if (pre->next == NULL){printf("Out of Mem4!\n");return ;}
				else{
					pre->next->pre = pre;
					pre->next->head = iphead;
					pre->next->ipFra = NULL;
					pre->next->next = NULL;
					pre->next->MF = 1;
					adddToipFra(handle, table, pre->next, iphead, skb);

				}
			}

		}
	}
}
//�����ݼ��뵽Hash���У����У�hash���еı�����ԴĿip��ΪΨһ��ʶ��
void addToHashTable(void *handle, struct hashtable * table, struct ip * iphead, struct sk_buff *skb){
printf("2 ");
	if (table->addr == NULL){
		table->addr = (struct srcDstAddr *)rte_malloc("srcaddr", sizeof(struct srcDstAddr),0);
		if (table->addr == NULL)
			{printf("Out of Mem5!\n");return ;}
		else{

			table->addr->Src = iphead->ip_src;
			table->addr->Dst = iphead->ip_dst;
			table->addr->next = NULL;
			table->addr->packets = NULL;
			addToAddr(handle, table->addr, iphead, skb);
		}
	}
	else{
		struct srcDstAddr * current, *pre;
		current = table->addr;
		pre = table->addr;
		while (current){
			if (current->Dst.s_addr == iphead->ip_src.s_addr && current->Src.s_addr == iphead->ip_dst.s_addr){
				//hit
				addToAddr(handle, current, iphead, skb);
				break;
			}
			else{
				pre = current;
				current = current->next;
			}
		}
		if (current == NULL){
			pre->next = (struct srcDstAddr *)rte_malloc("srcdst", sizeof(struct srcDstAddr),0);
			if (pre->next == NULL)
				{printf("Out of Mem6!\n");return ;}
			else{

				pre->next->Dst = iphead->ip_dst;
				pre->next->Src = iphead->ip_src;
				pre->next->next = NULL; 
				pre->next->packets = NULL; 
				addToAddr(handle, pre->next, iphead, skb);
			}

		}

	}
}
void ipDeFragment(void * handle, struct ip * iphead,struct sk_buff *skb){
	IpImpl * impl = (IpImpl *)handle;
	int index = addrtoHash( iphead->ip_src, iphead->ip_dst);
	int offset = ntohs(iphead ->ip_off);
	int flags = offset&~IP_OFFSET;
	offset &= IP_OFFSET;
	if(((flags & IP_MF) ==0)&&(offset ==0)){// no fragment.
		//printf("No fragment.\n");
	}
	else
	{
		printf("Fragment in %d.\n",index);
		fflush(stdout);
		addToHashTable(handle, &impl -> tables[index], iphead, skb);
	}
	//	tables[index].addr->packets->ipFra->info.ipHead = iphead;

		/*here need to add ip packet info */
		/*to do :add ipFragment head*/

}

void dpdk_ipDeFragment(void *handle, struct rte_mbuf *m){
	struct ip * iphead = (struct ip *)m;
	struct sk_buff s;
	s.data = (char *)(m - sizeof(struct ip));
	s.truesize = rte_pktmbuf_pkt_len(m) - sizeof(struct ip);
	ipDeFragment(handle, iphead, &s);
}

void initIpTable(struct hashtable* tables){
	int i = 0;
	for (i = 0; i < TABLESIZE; i++)
		tables[i]. addr = NULL;
}

//the following is the module interface

void init(Stream * pl, const char *name, void ** handle){

	IpImpl * impl = calloc(1,sizeof(IpImpl));
	if (!impl){
		printf("Out of Mem.\n");
		return ;
	}
	//point to func.
	pl -> init = init;
	pl -> addPacket = dpdk_ipDeFragment;
	//empty
	pl -> getPacket = NULL;
	pl -> getStream = NULL;
	pl -> realsePacket = NULL;
	pl -> checkTimeOut = NULL;
	pl -> showState = NULL;
	impl -> r = rte_ring_lookup(name);
	if(impl -> r == NULL){
		printf("Ring %s not found ,now creating a new ring.\n",name);
		impl -> r = rte_ring_create(name,4096, -1, 0);
		if(impl -> r == NULL){
			printf("Error in creating ring.\n");
			return ;
		}
		else
			printf("Done in creating ring.\n");
	}
	//init the module.
	initIpTable(impl -> tables); 
	*handle = impl; 
	printf("Init ip module done!\n");

}