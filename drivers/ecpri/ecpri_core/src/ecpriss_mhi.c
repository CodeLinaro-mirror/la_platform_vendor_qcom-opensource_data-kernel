/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpriss_core.h"
#include "ecpriss_netlink.h"
#include "ecpriss_workqueue.h"
#include "ecpriss_debugfs.h"
#include "ecpriss_log.h"
#include "ecpriss_mhi.h"
#include <linux/notifier.h>
#include <linux/panic_notifier.h>
#include <linux/msm_mhi_dev.h>


static	ecpriss_mhi_client_ctx_t *ctx = NULL;
static uint32_t seq_num = 0;

/* Static Array Initilizer for Instantiating MHI Channels for Control Communication 
 * Initilizer is an array of VF having two MHI Control Channel Each one for RX and
 * one for TX*/

/*Ecpriss MHI Channel Attributes*/

	/*MHI Client Handle*/
	/*Channel ID*/
	/* Channel Direction*/
	/* Channel State */
	/* Channel VF ID*/
	/*Max Channel Payload Size */
	/* Work Struct for handling Req/Rsp*/
	/* Work Struct for Handling Channel State*/
	/* Max Packets Supported */
	/* Buffer for Channel read Operations*/
	/* Input Req handler */
	/* State Change Handler*/
	/*Ctx */
	/*Read Req Cache*/
	/*Read req Counter*/
	/*Rsp Cache*/
	/* Write curr Counter */
	/* Num Pkts Received on Channel */
	/* Num Bytes Rcvd on Channel */
	/*Channel Mutex*/



static struct ecpriss_mhi_vf_ctx_s ecpriss_mhi_vf_table[] = {
	{
		VF_5,
		{
			{
				0,
				MHI_CLIENT_IP_SW_2_IN,
				ECPRISS_MHI_CH_DIR_OUT,
				{ECPRISS_MHI_CH_STATE_INIT},
				VF_5,
				ECPRISS_MHI_MAX_PKT_SIZE,
				{0},
				{0},
				ECPRISS_MHI_MAX_PKTS,
				NULL,
				ecpriss_mhi_client_cb,
				ecpriss_mhi_ch_state_cb,
				NULL,
				{0},
				0,
				{0},
				0,
				0,
				0,
				{0}
			},

			{
				0,
				MHI_CLIENT_IP_SW_2_OUT,
				ECPRISS_MHI_CH_DIR_IN,
				{ECPRISS_MHI_CH_STATE_INIT},
				VF_5,
				ECPRISS_MHI_MAX_PKT_SIZE,
				{0},
				{0},
				ECPRISS_MHI_MAX_PKTS,
				NULL,
				ecpriss_mhi_client_cb,
				ecpriss_mhi_ch_state_cb,
				NULL,
				{0},
				0,
				{0},
				0,
				0,
				0,
				{0}
			}
		}
	},

	{
		VF_8,
		{
			{
				0,
				MHI_CLIENT_IP_SW_2_IN,
				ECPRISS_MHI_CH_DIR_OUT,
				{ECPRISS_MHI_CH_STATE_INIT},
				VF_8,
				ECPRISS_MHI_MAX_PKT_SIZE,
				{0},
				{0},
				ECPRISS_MHI_MAX_PKTS,
				NULL,
				ecpriss_mhi_client_cb,
				ecpriss_mhi_ch_state_cb,
				NULL,
				{0},
				0,
				{0},
				0,
				0,
				0,
				{0}
			},

			{

				0,
				MHI_CLIENT_IP_SW_2_OUT,
				ECPRISS_MHI_CH_DIR_IN,
				{ECPRISS_MHI_CH_STATE_INIT},
				VF_8,
				ECPRISS_MHI_MAX_PKT_SIZE,
				{0},
				{0},
				ECPRISS_MHI_MAX_PKTS,
				NULL,
				ecpriss_mhi_client_cb,
				ecpriss_mhi_ch_state_cb,
				NULL,
				{0},
				0,
				{0},
				0,
				0,
				0,
				{0}
			}
		}
	},

	{
		VF_9,
		{
			{
				0,
				MHI_CLIENT_IP_SW_2_IN,
				ECPRISS_MHI_CH_DIR_OUT,
				{ECPRISS_MHI_CH_STATE_INIT},
				VF_9,
				ECPRISS_MHI_MAX_PKT_SIZE,
				{0},
				{0},
				ECPRISS_MHI_MAX_PKTS,
				NULL,
				ecpriss_mhi_client_cb,
				ecpriss_mhi_ch_state_cb,
				NULL,
				{0},
				0,
				{0},
				0,
				0,
				0,
				{0}
			},

			{

				0,
				MHI_CLIENT_IP_SW_2_OUT,
				ECPRISS_MHI_CH_DIR_IN,
				{ECPRISS_MHI_CH_STATE_INIT},
				VF_9,
				ECPRISS_MHI_MAX_PKT_SIZE,
				{0},
				{0},
				ECPRISS_MHI_MAX_PKTS,
				NULL,
				ecpriss_mhi_client_cb,
				ecpriss_mhi_ch_state_cb,
				NULL,
				{0},
				0,
				{0},
				0,
				0,
				0,
				{0}
			}
		}
	},
};



void ecpriss_mhi_ch_state_cb(struct mhi_dev_client_cb_data *cb_data)
{
	struct work_struct *work = NULL;

	if(cb_data) {

		struct ecpriss_mhi_ch_attr_s *ch_info =
			(struct ecpriss_mhi_ch_attr_s *)cb_data->user_data;


		if(!ch_info || !ctx) return;

		ECPRILOGDBG("ecpriss_mhi_ch_state_cb: VF: %d CH: ID: %d\n",
				ch_info->vf_id, ch_info->ch_id);

		if((atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_INIT ||
					atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_OPEN ||
					atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_CONNECTED) &&
				(cb_data->ctrl_info == MHI_STATE_DISCONNECTED )) {
			ECPRILOGERR("Setting Channel State: Disconnected\n");

			atomic_set(&ch_info->ch_state, ECPRISS_MHI_CH_STATE_DISCONNECTED) ;
		}


		if((atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_INIT ||
					atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_CLOSE ||
					atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_DISCONNECTED) &&
				(cb_data->ctrl_info == MHI_STATE_CONNECTED )) {

			ECPRILOGERR("Setting Channel State: Connected\n");
			atomic_set(&ch_info->ch_state, ECPRISS_MHI_CH_STATE_CONNECTED);
		}



		work = 	&ch_info->state_work;

		if(work && ctx->mhi_chan_st_wq) {

			queue_work(ctx->mhi_chan_st_wq, work);
		}
		else {
			return;
		}

	}

}


void ecpriss_mhi_client_cb(struct mhi_dev_client_cb_reason *reason)
{

	int vf_id = -1;
	int ch_id = -1;

	int wq_idx = -1;

	struct work_struct *work = NULL;
	struct workqueue_struct *wq = NULL;

	if(reason){

		if(reason->reason == MHI_DEV_TRE_AVAILABLE) {

			vf_id = reason->vf_id;
			ch_id = reason->ch_id;


			if(vf_id != VF_5 && vf_id != VF_8 && vf_id != VF_9){

				ECPRILOGDBG("ecpriss_mhi_client_cb:Invalid VF ID vf_id:%d\n",
						vf_id);
				return;
			}
			if(ch_id != MHI_CLIENT_IP_SW_2_OUT &&
					ch_id != MHI_CLIENT_IP_SW_2_IN){

				ECPRILOGDBG("ecpriss_mhi_client_cb:Invalid Ch ID ch_id:%d\n",
						ch_id);
				return;
			}


			/* For Out channels we get replenish indications here
			 * need to skip them for out channels
			 */

			if(ch_id % 2 != 0 )
				return;

			wq_idx = ecpriss_mhi_get_wq_idx_from_vfid(vf_id);

			if(wq_idx == -1)
				return;

			if(ctx){

				ECPRILOGDBG("ecpriss_mhi_client_cb: vf_id:%d ch_id:%d \n",
				reason->vf_id,reason->ch_id);


				wq = ctx->mhi_wq[wq_idx];

				work = &ctx->mhi_vf_info[wq_idx].ch_info[ECPRISS_MHI_CH_DIR_IN].work;

				queue_work(wq, work);


			}
		}

	}

}


int ecpriss_mhi_get_wq_idx_from_vfid(int vf_id)
{
	int wq_idx = -1;

	ECPRILOGDBG("ecpriss_mhi_get_wq_idx_from_vfid: vf_id %d\n",vf_id);

	switch(vf_id){

		case VF_5:
			wq_idx = 0;
			break;

		case VF_8:
			wq_idx = 1;
			break;

		case VF_9:
			wq_idx = 2;
			break;

		default:
			wq_idx =-1;

	}

	ECPRILOGDBG("ecpriss_mhi_get_wq_idx_from_vfid:wq_idx %d\n",wq_idx);

	return wq_idx;

}

void ecpriss_mhi_gen_rsp_util(csm_lte_ethdev_msg_s *req, csm_lte_ethdev_msg_s *rsp,
		csm_lte_ethdev_result_e status)
{

	if(!req || !rsp)
		return ;

	memset(rsp, 0, sizeof(csm_lte_ethdev_msg_s));

	rsp->hdr.version = 0;
	rsp->hdr.cmd_type = CSM_LTE_ETHDEV_CMD_TYPE_RESP;
	rsp->hdr.cmd_id = req->hdr.cmd_id;
	rsp->hdr.seq_num = req->hdr.seq_num;
	rsp->hdr.eth_link_id = req->hdr.eth_link_id;
	rsp->hdr.result = status;
	rsp->hdr.len = sizeof(csm_lte_ethdev_msg_hdr_s);

	return;

}


void ecpriss_mhi_process_get_mac_addr(csm_lte_ethdev_msg_s *req,
		struct ecpriss_mhi_ch_attr_s *ch_info)
{

	int pnum = -1;
	int linkid = -1;
	int vf_idx = -1;
	int index = -1;
	int ret = -1;

	struct ecpriss_mhi_ch_attr_s *out_ch_info = NULL;

	csm_lte_ethdev_mac_s mac_addr;

	csm_lte_ethdev_msg_s *rsp = NULL;

	pnum = req->hdr.eth_link_id / (ECPRISS_MAX_PORTS + 1);

	if(req->hdr.eth_link_id > 11)
	{
		ECPRILOGERR("Invalid link_id %d recvd\n",req->hdr.eth_link_id);
		return;
	}

	linkid = req->hdr.eth_link_id % 4;

	vf_idx = ecpriss_mhi_get_wq_idx_from_vfid(ch_info->vf_id);

	if(vf_idx == -1)
		return;

	index = ((vf_idx * ECPRISS_MAX_NR_MAC_PER_PORT) + linkid + ECPRISS_MAX_NR_MAC_PER_PORT);

	ret = ecpriss_qudp_get_lte_mac_addr(pnum, index, &mac_addr);

	if(ret < 0) {

		ECPRILOGERR("ecpriss_qudp_get_lte_mac_addr: Failed\n");
		return;
	}


	ECPRILOGDBG("ecpriss_mhi_process_get_mac_addr:Port:%d Index:%d\n",
			pnum, index);

	rsp = (csm_lte_ethdev_msg_s *)kmalloc(sizeof(csm_lte_ethdev_msg_s), GFP_KERNEL);

	if(rsp == NULL) {
		ECPRILOGERR("ecpriss_mhi_process_get_mac_addr: Failed\n");
		return;
	}
	ecpriss_mhi_gen_rsp_util(req, rsp, CSM_LTE_ETHDEV_RES_SUCCESS);

	memcpy(&rsp->payload.mac_addr, &mac_addr, sizeof(mac_addr));

	out_ch_info = &ctx->mhi_vf_info[vf_idx].ch_info[ECPRISS_MHI_CH_DIR_OUT];

	ecpriss_mhi_send_rsp((void *)rsp, sizeof(csm_lte_ethdev_msg_s), out_ch_info);

	return;

}

void ecpriss_mhi_process_set_mac_addr(csm_lte_ethdev_msg_s *req,
		struct ecpriss_mhi_ch_attr_s *ch_info)
{

	int pnum = -1;
	int linkid = -1;
	int vf_idx = -1;
	int index = -1;
	int ret = -1;

	struct ecpriss_mhi_ch_attr_s *out_ch_info = NULL;

	csm_lte_ethdev_msg_s *rsp = NULL;

	csm_lte_ethdev_mac_s mac_addr;

	pnum = req->hdr.eth_link_id / (ECPRISS_MAX_PORTS + 1);
	
	if(req->hdr.eth_link_id > 11)
	{
		ECPRILOGERR("Invalid link_id %d recvd\n",req->hdr.eth_link_id);
		return;
	}

	linkid = req->hdr.eth_link_id % 4;

	vf_idx = ecpriss_mhi_get_wq_idx_from_vfid(ch_info->vf_id);

	if(vf_idx == -1)
		return;

	index = ((vf_idx * ECPRISS_MAX_NR_MAC_PER_PORT) + linkid + ECPRISS_MAX_NR_MAC_PER_PORT);

	memcpy(&mac_addr, &req->payload.mac_addr, sizeof(mac_addr));

	ret = ecpriss_qudp_set_lte_mac_addr(pnum, index, &mac_addr);

	if(ret < 0) {

		ECPRILOGERR("ecpriss_qudp_set_lte_mac_addr: Failed\n");
		return;
	}

	ECPRILOGDBG("ecpriss_mhi_process_set_mac_addr:Port:%d Index:%d\n",
			pnum, index);

	rsp = (csm_lte_ethdev_msg_s *)kmalloc(sizeof(csm_lte_ethdev_msg_s), GFP_KERNEL);

	if(rsp == NULL) {
		ECPRILOGERR("ecpriss_mhi_process_set_mac_addr: Failed\n");
		return;
	}

	ecpriss_mhi_gen_rsp_util(req, rsp, CSM_LTE_ETHDEV_RES_SUCCESS);

	out_ch_info = &ctx->mhi_vf_info[vf_idx].ch_info[ECPRISS_MHI_CH_DIR_OUT];

	ecpriss_mhi_send_rsp((void *)rsp, sizeof(csm_lte_ethdev_msg_s), out_ch_info);


	return;

}
void ecpriss_mhi_process_get_link_state(csm_lte_ethdev_msg_s *req,
		struct ecpriss_mhi_ch_attr_s *ch_info)
{

	int pnum = -1;
	int linkid = -1;
	int ptype = -1;
	int vf_idx = -1;

	struct ecpriss_mhi_ch_attr_s *out_ch_info = NULL;

	csm_lte_ethdev_state_s state ;

	csm_lte_ethdev_msg_s *rsp = NULL;

	pnum = req->hdr.eth_link_id / (ECPRISS_MAX_PORTS + 1);

	linkid = req->hdr.eth_link_id % 4;

	ptype = ETH_ECPRISS_PORT_TYPE_FH;

	state.link_state = ecpriss_get_link_state(ptype, pnum, linkid);

	ECPRILOGDBG("ecpriss_mhi_process_get_link_state:Port:%d Link:%d State:%d\n",
			pnum, linkid, state);

	rsp = (csm_lte_ethdev_msg_s *)kmalloc(sizeof(csm_lte_ethdev_msg_s), GFP_KERNEL);

	if(rsp == NULL) {
		ECPRILOGERR("ecpriss_mhi_process_get_link_state: Malloc failed\n");
		return;
	}

	ecpriss_mhi_gen_rsp_util(req, rsp, CSM_LTE_ETHDEV_RES_SUCCESS);

	rsp->payload.link_state = state;

	vf_idx = ecpriss_mhi_get_wq_idx_from_vfid(ch_info->vf_id);

	if(vf_idx == -1)
		return;

	out_ch_info = &ctx->mhi_vf_info[vf_idx].ch_info[ECPRISS_MHI_CH_DIR_OUT];

	ecpriss_mhi_send_rsp((void *)rsp, sizeof(csm_lte_ethdev_msg_s), out_ch_info);

	return;

}

void ecpriss_mhi_process_get_link_rate(csm_lte_ethdev_msg_s *req,
		struct ecpriss_mhi_ch_attr_s *ch_info)
{

	int pnum = -1;
	int linkid = -1;
	int ptype = -1;

	int vf_idx = -1;

	struct ecpriss_mhi_ch_attr_s *out_ch_info = NULL;

	csm_lte_ethdev_rate_s l_rate ;

	csm_lte_ethdev_msg_s *rsp = NULL;

	pnum = req->hdr.eth_link_id / (ECPRISS_MAX_PORTS + 1);

	linkid = req->hdr.eth_link_id % 4 ;

	ptype = ETH_ECPRISS_PORT_TYPE_FH;

	l_rate.link_rate = ecpriss_get_link_rate(ptype, pnum, linkid);

	ECPRILOGDBG("ecpriss_mhi_process_get_link_rate:Port:%d Link:%d link_rate:%d\n",
			pnum, linkid, l_rate.link_rate);

	rsp = (csm_lte_ethdev_msg_s *)kmalloc(sizeof(csm_lte_ethdev_msg_s), GFP_KERNEL);

	if(rsp == NULL) {
		ECPRILOGERR("ecpriss_mhi_process_get_link_rate: Malloc failed\n");
		return;
	}

	ecpriss_mhi_gen_rsp_util(req, rsp, CSM_LTE_ETHDEV_RES_SUCCESS);

	rsp->payload.link_rate = l_rate;

	vf_idx = ecpriss_mhi_get_wq_idx_from_vfid(ch_info->vf_id);

	if(vf_idx == -1)
		return;

	out_ch_info = &ctx->mhi_vf_info[vf_idx].ch_info[ECPRISS_MHI_CH_DIR_OUT];

	ecpriss_mhi_send_rsp((void *)rsp, sizeof(csm_lte_ethdev_msg_s), out_ch_info);

	return;

}


void ecpriss_mhi_process_async_link_state(int port, int link, int state)
{


	ecpriss_mhi_process_async_link_state_util(port, link, state, VF_5);
	ecpriss_mhi_process_async_link_state_util(port, link, state, VF_8);
	ecpriss_mhi_process_async_link_state_util(port, link, state, VF_9);

	return;

}

void ecpriss_mhi_process_async_link_state_util(int port, int link, int state, int vf_id)
{
	int vf_idx = -1;

	struct ecpriss_mhi_ch_attr_s *out_ch_info = NULL;

	csm_lte_ethdev_state_s ch_link_state ;

	csm_lte_ethdev_msg_s *rsp = NULL;

	vf_idx = ecpriss_mhi_get_wq_idx_from_vfid(vf_id);

	if(vf_idx == -1)
		return;

	if(!ctx)
		return;

	if(!ctx->mhi_vf_info)
		return;

	out_ch_info = &ctx->mhi_vf_info[vf_idx].ch_info[ECPRISS_MHI_CH_DIR_OUT];

	if(atomic_read(&out_ch_info->ch_state) != ECPRISS_MHI_CH_STATE_OPEN){
		return;
	}

	rsp = (csm_lte_ethdev_msg_s *)kmalloc(sizeof(csm_lte_ethdev_msg_s), GFP_KERNEL);

	if(rsp == NULL) {
		ECPRILOGERR("ecpriss_mhi_process_async_link_state_util: Malloc failed\n");
		return;
	}

	ch_link_state.link_state = state;

	ECPRILOGDBG("ecpriss_mhi_process_async_link_state_util:VF:%d port:%d Link:%d State:%d\n",
			vf_id, port, link, state);


	rsp->hdr.version = 0;
	rsp->hdr.cmd_type = CSM_LTE_ETHDEV_CMD_TYPE_IND;
	rsp->hdr.cmd_id = CSM_LTE_ETHDEV_CMD_SET_LINK_STATE;
	rsp->hdr.seq_num = seq_num;
	rsp->hdr.eth_link_id = (link + (port*(ECPRISS_MAX_PORTS + 1)));
	rsp->hdr.len = sizeof(csm_lte_ethdev_msg_hdr_s);

	rsp->payload.link_state = ch_link_state;

	ecpriss_mhi_send_rsp((void *)rsp, sizeof(csm_lte_ethdev_msg_s), out_ch_info);
}

void ecpriss_mhi_process_request(void *buf, int len, struct ecpriss_mhi_ch_attr_s *ch_info)
{
	csm_lte_ethdev_msg_s *req = NULL;

	if(buf == NULL || ch_info == NULL) return;


	req = (csm_lte_ethdev_msg_s *)buf;

	if(req  == NULL) return;

	/* We keep a cache of req received updating the same here */

	memcpy(&ch_info->req[(ch_info->curr_req_idx++) % ECPRISS_MAX_REQ_CACHE_CNT],
			req, sizeof(csm_lte_ethdev_msg_s));

	switch(req->hdr.cmd_id) {

		case CSM_LTE_ETHDEV_CMD_GET_LINK_STATE:
			ecpriss_mhi_process_get_link_state(req, ch_info);
			break;

		case CSM_LTE_ETHDEV_CMD_GET_LINK_RATE:
			ecpriss_mhi_process_get_link_rate(req, ch_info);
			break;

		case CSM_LTE_ETHDEV_CMD_GET_MAC_ADDR:
			ecpriss_mhi_process_get_mac_addr(req, ch_info);
			break;

		case CSM_LTE_ETHDEV_CMD_SET_MAC_ADDR:
			ecpriss_mhi_process_set_mac_addr(req, ch_info);
			break;

		default:
			break;


	}

}

void ecpriss_mhi_work_fn(struct work_struct *work)
{

	struct mhi_req req;

	int bytes_avail = -1;

	struct ecpriss_mhi_ch_attr_s *ch_info = container_of(work,
			struct ecpriss_mhi_ch_attr_s, work);


	if(!ch_info || (atomic_read(&ch_info->ch_state) != ECPRISS_MHI_CH_STATE_OPEN))
	{
		ECPRILOGERR("ecpriss_mhi_work_fn:Invalid ch info or not enabled\n");
		return;
	}

	memset(&req, 0 , sizeof(req));

	req.chan = ch_info->ch_id;
	req.client = ch_info->handle;
	req.mode = DMA_SYNC;
	req.buf = ch_info->buf;
	req.len = ch_info->max_pkt_size;
	req.vf_id = ch_info->vf_id;
	req.transfer_len = 0;

	ECPRILOGDBG("ecpriss_mhi_wk_fn: Ch_id: %d Vf_id: %d max_pk_size: %d\n ",
			ch_info->ch_id,ch_info->vf_id,ch_info->max_pkt_size);

	do {

		bytes_avail = mhi_dev_read_channel(&req);

		if (bytes_avail < 0) {
			ECPRILOGERR("ecpriss_mhi_work_fn: Failed to read with err:%d\n",
					bytes_avail);
			return;
		}

		if (bytes_avail == 0) {

			return;
		}

		ch_info->num_bytes += bytes_avail;
		ch_info->num_pkts++;

		ecpriss_mhi_process_request(req.buf, req.transfer_len, ch_info);


	} while (bytes_avail > 0);


}

void ecpriss_mhi_state_work_fn(struct work_struct *work)
{
	int rc = 0;

	struct ecpriss_mhi_ch_attr_s *ch_info = container_of(work,
			struct ecpriss_mhi_ch_attr_s, state_work);

	if(!ch_info )
	{
		ECPRILOGERR("ecpriss_mhi_work_fn:Invalid ch info \n");
		return;
	}


	if(atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_CONNECTED) {


		rc =  mhi_dev_vf_open_channel(ch_info->vf_id, ch_info->ch_id,
				&ch_info->handle,
				ch_info->event_notifier);


		if (rc < 0 && rc != -EEXIST) {

			ECPRILOGERR("ecpriss_mhi_chan_init:mhi_open_channel failed:%d\n",
					rc);
			return;
		}


		atomic_set(&ch_info->ch_state, ECPRISS_MHI_CH_STATE_OPEN);

		ECPRILOGINFO("ecpriss_mhi_state_wk_fn:Channel Open Success VF: %d Ch: %d\n",
				ch_info->vf_id, ch_info->ch_id);

	}
	if(atomic_read(&ch_info->ch_state) == ECPRISS_MHI_CH_STATE_DISCONNECTED) {

		mhi_dev_close_channel(ch_info->handle);

		atomic_set(&ch_info->ch_state, ECPRISS_MHI_CH_STATE_CLOSE);

		ECPRILOGINFO("ecpriss_mhi_state_wk_fn:Channel close Success VF: %d Ch: %d\n",
				ch_info->vf_id, ch_info->ch_id);

	}
}


int ecpriss_mhi_ctx_init(void **mhi_ctx)
{
	int i = 0;

	char buf[128];

	if(mhi_ctx == NULL) {

		ECPRILOGERR("ecpriss_mhi_ctx_init: Invalid params\n");
		return -EINVAL;

	}

	ctx = kmalloc(sizeof(ecpriss_mhi_client_ctx_t), GFP_KERNEL);

	if(ctx == NULL) {

		ECPRILOGERR("ecpriss_mhi_ctx_init: Ctx Mem Alloc failed\n");
		return -1;
	}

	memset(ctx, 0, sizeof(ecpriss_mhi_client_ctx_t));

	spin_lock_init(&ctx->ctx_lock);

	for(i = 0; i < ECPRISS_MHI_MAX_VF; i ++) {

		memset(buf, 0, sizeof(buf));

		snprintf(buf, sizeof(buf),"ecpriss_mhi_workq_%d", i);

		ctx->mhi_wq[i] = create_singlethread_workqueue(buf);

		if(ctx->mhi_wq[i] == NULL){

			ECPRILOGERR("ecpriss_mhi_ctx_init:MHI Wq Init Failed %d \n", i);
			goto error;
		}
	}

	ctx->mhi_chan_st_wq = create_singlethread_workqueue("ecpriss_mhi_ch_st_wq");

	if(ctx->mhi_chan_st_wq == NULL) {

		ECPRILOGERR("ecpriss_mhi_ctx_init:ecpriss_mhi_ch_st_wq failed %d \n", i);
		goto error;

	}

	ctx->mhi_vf_info = ecpriss_mhi_vf_table;

	*mhi_ctx = ctx;


	return ecpriss_mhi_vf_init_all();

error:
	for(i = 0; i < ECPRISS_MHI_MAX_VF; i ++) {

		if(ctx->mhi_wq[i] != NULL){

			flush_workqueue(ctx->mhi_wq[i]);
			destroy_workqueue(ctx->mhi_wq[i]);
			ctx->mhi_wq[i] = NULL;
		}
	}

	if(ctx->mhi_chan_st_wq != NULL){

		flush_workqueue(ctx->mhi_chan_st_wq);
		destroy_workqueue(ctx->mhi_chan_st_wq);
		ctx->mhi_chan_st_wq = NULL;

	}

	if(ctx) {
		kfree(ctx);
		ctx = NULL;
	}
	return -1;

}

int ecpriss_mhi_ctx_deinit(void *mhi_ctx)
{
	int i = 0;

	if(mhi_ctx == NULL || ctx == NULL) {

		ECPRILOGERR("Invalid params\n");
		return -EINVAL;
	}

	ecpriss_mhi_vf_deinit_all();

	for(i = 0; i < ECPRISS_MHI_MAX_VF; i ++) {

		if(ctx->mhi_wq[i] != NULL){

			flush_workqueue(ctx->mhi_wq[i]);
			destroy_workqueue(ctx->mhi_wq[i]);
			ctx->mhi_wq[i] = NULL;
		}
	}

	if(ctx->mhi_chan_st_wq != NULL){

		flush_workqueue(ctx->mhi_chan_st_wq);
		destroy_workqueue(ctx->mhi_chan_st_wq);
		ctx->mhi_chan_st_wq = NULL;

	}

	if(ctx) {
		kfree(ctx);
		ctx = NULL;
	}
	return -1;

}

int ecpriss_mhi_chan_init(struct ecpriss_mhi_ch_attr_s * ch_info)
{

	int rc = 0;

	if(ch_info == NULL){
		ECPRILOGERR("ecpriss_mhi_chan_init:mhi_ctx is NULL \n");
		return -EINVAL;
	}


	INIT_WORK(&ch_info->work, ecpriss_mhi_work_fn);
	INIT_WORK(&ch_info->state_work, ecpriss_mhi_state_work_fn);


	ch_info->buf = kmalloc(ch_info->max_pkt_size, GFP_KERNEL);

	if(ch_info->buf == NULL)
	{
		ECPRILOGERR("ecpriss_mhi_chan_init: Memory Allocation failed for ch:%d\n",
				ch_info->ch_id);
		return -ENOMEM;
	}

	mutex_init(&ch_info->ch_mtx);

	rc = mhi_vf_register_state_cb(ch_info->state_notifier,
			(void *)ch_info, ch_info->ch_id,
			ch_info->vf_id);


	if (rc < 0) {

		ECPRILOGERR("ecpriss_mhi_chan_init:mhi_open_channel failed:%d\n",
				rc);
		goto error;
	}

	ECPRILOGINFO("ecpriss_mhi_chan_init: registered State Cb Success for ch:%d vf:%d\n",ch_info->ch_id, ch_info->vf_id);


	return 0;

error:
	if(ch_info->buf != NULL) {

		kfree(ch_info->buf);
		ch_info->buf = NULL;
	}
	return -1;

}


int ecpriss_mhi_chan_deinit(struct ecpriss_mhi_ch_attr_s *ch_info)
{

	if(ch_info == NULL){
		ECPRILOGERR("ecpriss_mhi_chan_init:mhi_ctx is NULL \n");
		return -EINVAL;
	}

	mhi_dev_close_channel(ch_info->handle);

	if(ch_info->buf != NULL) {

		kfree(ch_info->buf);
		ch_info->buf = NULL;
	}
	return 0;

}


int ecpriss_mhi_vf_init_all(void)
{

	int rc = 0, i = 0, j = 0;

	struct ecpriss_mhi_vf_ctx_s *vf = NULL;
	struct ecpriss_mhi_ch_attr_s *ch_info = NULL;

	for (i = 0; i < ARRAY_SIZE(ecpriss_mhi_vf_table); i++) {

		vf = &ecpriss_mhi_vf_table[i];

		for(j = 0 ; j < ECPRISS_MAX_MHI_CHANNEL_VF ; j++) {

			ch_info = &vf->ch_info[j];

			rc  = ecpriss_mhi_chan_init(ch_info);

		}


	}

	return 0;

}

int ecpriss_mhi_vf_deinit_all(void)
{

	int rc = 0, i = 0, j = 0;

	struct ecpriss_mhi_vf_ctx_s *vf = NULL;
	struct ecpriss_mhi_ch_attr_s *ch_info = NULL;

	for (i = 0; i < ARRAY_SIZE(ecpriss_mhi_vf_table); i++) {

		vf = &ecpriss_mhi_vf_table[i];

		for(j = 0 ; j < ECPRISS_MAX_MHI_CHANNEL_VF ; j++) {

			ch_info = &vf->ch_info[j];

			rc  = ecpriss_mhi_chan_deinit(ch_info);

		}

	}

	return 0;

}
int ecpriss_mhi_send_rsp(void *buf, int len,
		struct ecpriss_mhi_ch_attr_s *ch_info)
{
	int bytes_written = 0;

	struct mhi_req req;

	int retry_count = ECPRISS_MAX_MHI_SEND_RETRY_CNT;

	int err = -1;

	if (!ch_info || atomic_read(&ch_info->ch_state) != ECPRISS_MHI_CH_STATE_OPEN) {
		ECPRILOGERR("ecpriss_mhi_send_rsp: Channel not Open\n");
		err = -ENOTCONN;
		goto error;
	}

	if(buf == NULL) {
		err = -EINVAL;
		goto error;
	}

	req.buf = buf;
	req.len = len;
	req.chan = ch_info->ch_id;
	req.mode = DMA_SYNC;
	req.client = ch_info->handle;
	req.vf_id = ch_info->vf_id;

	/* These retries are needed if we don't have any
	 * buffers available from host to write*/

	do {

		bytes_written = mhi_dev_write_channel(&req);

		retry_count--;

		msleep(ECPRISS_MAX_MHI_SEND_RETRY_DELAY);

	} while (bytes_written == 0 && retry_count);

	ECPRILOGDBG("MHI SND RESP: Bytes to send %d  and bytes Written %d\n", req.len, bytes_written);


	if(bytes_written != len) {

		ECPRILOGERR("ecpriss_mhi_send_rsp: Failed with error: %d ch_id:%d \n",
				bytes_written, ch_info->ch_id);
		goto error;

	}

	ch_info->num_bytes += bytes_written;
	ch_info->num_pkts++;

	/* We keep a cache of responses sent out updating the same here */

	memcpy(&ch_info->rsp[(ch_info->curr_rsp_idx++) % ECPRISS_MAX_REQ_CACHE_CNT],
			buf, sizeof(csm_lte_ethdev_msg_s));

	ECPRILOGINFO("ecpriss_mhi_send_rsp: Success for vf: %d ch:%d\n",
			ch_info->vf_id, ch_info->ch_id);

	seq_num++;

	if(buf){

		kfree(buf);
		buf = NULL;
	}

	return 0;

error:
	if(buf){

		kfree(buf);
		buf = NULL;
	}

	return err;

}




