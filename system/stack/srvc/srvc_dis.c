/******************************************************************************
 *
 *  Copyright (C) 1999-2013 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "bt_target.h"
#include "bt_utils.h"
#include "gatt_api.h"
#include "gatt_int.h"
#include "srvc_eng_int.h"
#include "srvc_dis_int.h"

#if BLE_INCLUDED == TRUE

#define DIS_UUID_TO_ATTR_MASK(x)   (UINT16)(1 << ((x) - GATT_UUID_SYSTEM_ID))

#define DIS_MAX_NUM_INC_SVR       0
#define DIS_MAX_CHAR_NUM          9
#define DIS_MAX_ATTR_NUM          (DIS_MAX_CHAR_NUM * 2 + DIS_MAX_NUM_INC_SVR + 1)

#ifndef DIS_ATTR_DB_SIZE
#define DIS_ATTR_DB_SIZE      GATT_DB_MEM_SIZE(DIS_MAX_NUM_INC_SVR, DIS_MAX_CHAR_NUM, 0)
#endif

#define UINT64_TO_STREAM(p, u64) {*(p)++ = (UINT8)(u64);       *(p)++ = (UINT8)((u64) >> 8);*(p)++ = (UINT8)((u64) >> 16); *(p)++ = (UINT8)((u64) >> 24); \
                                    *(p)++ = (UINT8)((u64) >> 32); *(p)++ = (UINT8)((u64) >> 40);*(p)++ = (UINT8)((u64) >> 48); *(p)++ = (UINT8)((u64) >> 56);}

#define STREAM_TO_UINT64(u64, p) {u64 = (((UINT64)(*(p))) + ((((UINT64)(*((p) + 1)))) << 8) + ((((UINT64)(*((p) + 2)))) << 16) + ((((UINT64)(*((p) + 3)))) << 24) \
                                  + ((((UINT64)(*((p) + 4)))) << 32) + ((((UINT64)(*((p) + 5)))) << 40) + ((((UINT64)(*((p) + 6)))) << 48) + ((((UINT64)(*((p) + 7)))) << 56)); (p) += 8;}



static const UINT16  dis_attr_uuid[DIS_MAX_CHAR_NUM] =
{
    GATT_UUID_SYSTEM_ID,
    GATT_UUID_MODEL_NUMBER_STR,
    GATT_UUID_SERIAL_NUMBER_STR,
    GATT_UUID_FW_VERSION_STR,
    GATT_UUID_HW_VERSION_STR,
    GATT_UUID_SW_VERSION_STR,
    GATT_UUID_MANU_NAME,
    GATT_UUID_IEEE_DATA,
    GATT_UUID_PNP_ID
};

tDIS_CB dis_cb;
/*******************************************************************************
**   dis_valid_handle_range
**
**   validate a handle to be a DIS attribute handle or not.
*******************************************************************************/
BOOLEAN dis_valid_handle_range(UINT16 handle)
{
    if (handle >= dis_cb.service_handle && handle <= dis_cb.max_handle)
        return TRUE;
    else
        return FALSE;
}
/*******************************************************************************
**   dis_write_attr_value
**
**   Process write DIS attribute request.
*******************************************************************************/
UINT8 dis_write_attr_value(tGATT_WRITE_REQ * p_data, tGATT_STATUS *p_status)
{
    UNUSED(p_data);

    *p_status = GATT_WRITE_NOT_PERMIT;
    return SRVC_ACT_RSP;
}
/*******************************************************************************
**   DIS Attributes Database Server Request callback
*******************************************************************************/
UINT8 dis_read_attr_value (UINT8 clcb_idx, UINT16 handle, tGATT_VALUE *p_value,
                           BOOLEAN is_long, tGATT_STATUS *p_status)
{
    tDIS_DB_ENTRY   *p_db_attr = dis_cb.dis_attr;
    UINT8           *p = p_value->value, i, *pp;
    UINT16          offset = p_value->offset;
    UINT8           act = SRVC_ACT_RSP;
    tGATT_STATUS    st = GATT_NOT_FOUND;
    UNUSED(clcb_idx);

    for (i = 0; i < DIS_MAX_CHAR_NUM; i ++, p_db_attr ++)
    {
        if (handle == p_db_attr->handle)
        {
            if ((p_db_attr->uuid == GATT_UUID_PNP_ID || p_db_attr->uuid == GATT_UUID_SYSTEM_ID)&&
                is_long == TRUE)
            {
                st = GATT_NOT_LONG;
                break;
            }
            st = GATT_SUCCESS;

            switch (p_db_attr->uuid)
            {
                case GATT_UUID_MANU_NAME:
                case GATT_UUID_MODEL_NUMBER_STR:
                case GATT_UUID_SERIAL_NUMBER_STR:
                case GATT_UUID_FW_VERSION_STR:
                case GATT_UUID_HW_VERSION_STR:
                case GATT_UUID_SW_VERSION_STR:
                case GATT_UUID_IEEE_DATA:
                    pp = dis_cb.dis_value.data_string[p_db_attr->uuid - GATT_UUID_MODEL_NUMBER_STR];
                    if (pp != NULL)
                    {
                        if (strlen ((char *)pp) > GATT_MAX_ATTR_LEN)
                            p_value->len = GATT_MAX_ATTR_LEN;
                        else
                            p_value->len = (UINT16)strlen ((char *)pp);
                    }
                    else
                        p_value->len = 0;

                    if (offset > p_value->len)
                    {
                        st = GATT_INVALID_OFFSET;
                        break;
                    }
                    else
                    {
                        p_value->len -= offset;
                        pp += offset;
                        ARRAY_TO_STREAM(p, pp, p_value->len);
                        GATT_TRACE_EVENT("GATT_UUID_MANU_NAME len=0x%04x", p_value->len);
                    }
                    break;


                case GATT_UUID_SYSTEM_ID:
                    UINT64_TO_STREAM(p, dis_cb.dis_value.system_id); /* int_min */
                    p_value->len = DIS_SYSTEM_ID_SIZE;
                    break;

                case  GATT_UUID_PNP_ID:
                    UINT8_TO_STREAM(p, dis_cb.dis_value.pnp_id.vendor_id_src);
                    UINT16_TO_STREAM(p, dis_cb.dis_value.pnp_id.vendor_id);
                    UINT16_TO_STREAM(p, dis_cb.dis_value.pnp_id.product_id);
                    UINT16_TO_STREAM(p, dis_cb.dis_value.pnp_id.product_version);
                    p_value->len = DIS_PNP_ID_SIZE;
                    break;

            }
            break;
        }
    }
    *p_status = st;
    return act;
}

/*******************************************************************************
**
** Function         dis_gatt_c_read_dis_value_cmpl
**
** Description      Client read DIS database complete callback.
**
** Returns          void
**
*******************************************************************************/
void dis_gatt_c_read_dis_value_cmpl(UINT16 conn_id)
{
    tSRVC_CLCB *p_clcb =  srvc_eng_find_clcb_by_conn_id(conn_id);

    dis_cb.dis_read_uuid_idx = 0xff;

    srvc_eng_release_channel(conn_id);

    if (dis_cb.p_read_dis_cback && p_clcb)
    {
        GATT_TRACE_ERROR("dis_gatt_c_read_dis_value_cmpl: attr_mask = 0x%04x", p_clcb->dis_value.attr_mask);
        GATT_TRACE_EVENT("calling p_read_dis_cbackd");

        (*dis_cb.p_read_dis_cback)(p_clcb->bda, &p_clcb->dis_value);
        dis_cb.p_read_dis_cback=NULL;
    }

}

/*******************************************************************************
**
** Function         dis_gatt_c_read_dis_req
**
** Description      Read remote device DIS attribute request.
**
** Returns          void
**
*******************************************************************************/
BOOLEAN dis_gatt_c_read_dis_req(UINT16 conn_id)
{
    tGATT_READ_PARAM   param;

    memset(&param, 0, sizeof(tGATT_READ_PARAM));

    param.service.uuid.len       = LEN_UUID_16;
    param.service.s_handle       = 1;
    param.service.e_handle       = 0xFFFF;
    param.service.auth_req       = 0;

    while (dis_cb.dis_read_uuid_idx < DIS_MAX_CHAR_NUM)
    {
        param.service.uuid.uu.uuid16 = dis_attr_uuid[dis_cb.dis_read_uuid_idx];

        if (GATTC_Read(conn_id, GATT_READ_BY_TYPE, &param) == GATT_SUCCESS)
        {
            return(TRUE);
        }
        else
        {
            GATT_TRACE_ERROR ("Read DISInfo: 0x%04x GATT_Read Failed", param.service.uuid.uu.uuid16);
            dis_cb.dis_read_uuid_idx ++;
        }
    }

    dis_gatt_c_read_dis_value_cmpl(conn_id);

    return(FALSE);
}

/*******************************************************************************
**
** Function         dis_c_cmpl_cback
**
** Description      Client operation complete callback.
**
** Returns          void
**
*******************************************************************************/
void dis_c_cmpl_cback (tSRVC_CLCB *p_clcb, tGATTC_OPTYPE op,
                              tGATT_STATUS status, tGATT_CL_COMPLETE *p_data)
{
    UINT16      read_type = dis_attr_uuid[dis_cb.dis_read_uuid_idx];
    UINT8       *pp = NULL, *p_str;
    UINT16      conn_id = p_clcb->conn_id;

    GATT_TRACE_EVENT ("dis_c_cmpl_cback() - op_code: 0x%02x  status: 0x%02x  \
                        read_type: 0x%04x", op, status, read_type);

    if (op != GATTC_OPTYPE_READ)
        return;

    if (p_data != NULL && status == GATT_SUCCESS)
    {
        pp = p_data->att_value.value;

        switch (read_type)
        {
            case GATT_UUID_SYSTEM_ID:
                GATT_TRACE_EVENT ("DIS_ATTR_SYS_ID_BIT");
                if (p_data->att_value.len == DIS_SYSTEM_ID_SIZE)
                {
                    p_clcb->dis_value.attr_mask |= DIS_ATTR_SYS_ID_BIT;
                    /* save system ID*/
                    STREAM_TO_UINT64 (p_clcb->dis_value.system_id, pp);
                }
                break;

            case GATT_UUID_PNP_ID:
                if (p_data->att_value.len == DIS_PNP_ID_SIZE)
                {
                    p_clcb->dis_value.attr_mask |= DIS_ATTR_PNP_ID_BIT;
                    STREAM_TO_UINT8 (p_clcb->dis_value.pnp_id.vendor_id_src, pp);
                    STREAM_TO_UINT16 (p_clcb->dis_value.pnp_id.vendor_id, pp);
                    STREAM_TO_UINT16 (p_clcb->dis_value.pnp_id.product_id, pp);
                    STREAM_TO_UINT16 (p_clcb->dis_value.pnp_id.product_version, pp);
                }
                break;

            case GATT_UUID_MODEL_NUMBER_STR:
            case GATT_UUID_SERIAL_NUMBER_STR:
            case GATT_UUID_FW_VERSION_STR:
            case GATT_UUID_HW_VERSION_STR:
            case GATT_UUID_SW_VERSION_STR:
            case GATT_UUID_MANU_NAME:
            case GATT_UUID_IEEE_DATA:
                p_str = p_clcb->dis_value.data_string[read_type - GATT_UUID_MODEL_NUMBER_STR];
                if (p_str != NULL)
                    GKI_freebuf(p_str);
                if ((p_str = (UINT8 *)GKI_getbuf((UINT16)(p_data->att_value.len + 1))) != NULL)
                {
                    memset(p_str, 0, p_data->att_value.len + 1);
                    p_clcb->dis_value.attr_mask |= DIS_UUID_TO_ATTR_MASK (read_type);
                    memcpy(p_str, p_data->att_value.value, p_data->att_value.len);
                }
                break;

            default:
                    break;

                break;
        }/* end switch */
    }/* end if */

    dis_cb.dis_read_uuid_idx ++;

    dis_gatt_c_read_dis_req(conn_id);
}


/*******************************************************************************
**
** Function         DIS_SrInit
**
** Description      Initializa the Device Information Service Server.
**
*******************************************************************************/
tDIS_STATUS DIS_SrInit (tDIS_ATTR_MASK dis_attr_mask)
{
    tBT_UUID          uuid = {LEN_UUID_16, {UUID_SERVCLASS_DEVICE_INFO}};
    UINT16            i = 0;
    tGATT_STATUS      status;
    tDIS_DB_ENTRY        *p_db_attr = &dis_cb.dis_attr[0];

    if (dis_cb.enabled)
    {
        GATT_TRACE_ERROR("DIS already initalized");
        return DIS_SUCCESS;
    }

    memset(&dis_cb, 0, sizeof(tDIS_CB));

    dis_cb.service_handle = GATTS_CreateService (srvc_eng_cb.gatt_if , &uuid, 0, DIS_MAX_ATTR_NUM, TRUE);

    if (dis_cb.service_handle == 0)
    {
        GATT_TRACE_ERROR("Can not create service, DIS_Init failed!");
        return GATT_ERROR;
    }
    dis_cb.max_handle = dis_cb.service_handle + DIS_MAX_ATTR_NUM;

    while (dis_attr_mask != 0 && i < DIS_MAX_CHAR_NUM)
    {
        /* add Manufacturer name
        */
        uuid.uu.uuid16 = p_db_attr->uuid = dis_attr_uuid[i];
        p_db_attr->handle  = GATTS_AddCharacteristic(dis_cb.service_handle, &uuid, GATT_PERM_READ, GATT_CHAR_PROP_BIT_READ);
        GATT_TRACE_DEBUG ("DIS_SrInit:  handle of new attribute 0x%04 = x%d", uuid.uu.uuid16, p_db_attr->handle  );
        p_db_attr ++;
        i ++;
        dis_attr_mask >>= 1;
    }

    /* start service
    */
    status = GATTS_StartService (srvc_eng_cb.gatt_if, dis_cb.service_handle, GATT_TRANSPORT_LE_BR_EDR);

    dis_cb.enabled = TRUE;

    return (tDIS_STATUS) status;
}
/*******************************************************************************
**
** Function         DIS_SrUpdate
**
** Description      Update the DIS server attribute values
**
*******************************************************************************/
tDIS_STATUS DIS_SrUpdate(tDIS_ATTR_BIT dis_attr_bit, tDIS_ATTR *p_info)
{
    UINT8           i = 1;
    tDIS_STATUS     st = DIS_SUCCESS;

    if (dis_attr_bit & DIS_ATTR_SYS_ID_BIT)
    {
        dis_cb.dis_value.system_id = p_info->system_id;
    }
    else if (dis_attr_bit & DIS_ATTR_PNP_ID_BIT)
    {
        dis_cb.dis_value.pnp_id.vendor_id         = p_info->pnp_id.vendor_id;
        dis_cb.dis_value.pnp_id.vendor_id_src     = p_info->pnp_id.vendor_id_src;
        dis_cb.dis_value.pnp_id.product_id        = p_info->pnp_id.product_id;
        dis_cb.dis_value.pnp_id.product_version   = p_info->pnp_id.product_version;
    }
    else
    {
        st = DIS_ILLEGAL_PARAM;

        while (dis_attr_bit && i < (DIS_MAX_CHAR_NUM -1 ))
        {
            if (dis_attr_bit & (UINT16)(1 << i))
            {
                if (dis_cb.dis_value.data_string[i - 1] != NULL)
                    GKI_freebuf(dis_cb.dis_value.data_string[i]);
/* coverity[OVERRUN-STATIC] False-positive : when i = 8, (1 << i) == DIS_ATTR_PNP_ID_BIT, and it will never come down here
CID 49902: Out-of-bounds read (OVERRUN_STATIC)
Overrunning static array "dis_cb.dis_value.data_string", with 7 elements, at position 7 with index variable "i".
*/
                if ((dis_cb.dis_value.data_string[i - 1] = (UINT8 *)GKI_getbuf((UINT16)(p_info->data_str.len + 1))) != NULL)
                {
                    memset(dis_cb.dis_value.data_string[i - 1], 0, p_info->data_str.len + 1); /* make sure null terminate */
                    memcpy(dis_cb.dis_value.data_string[i - 1], p_info->data_str.p_data, p_info->data_str.len);
                    st = DIS_SUCCESS;
                }
                else
                    st = DIS_NO_RESOURCES;

                break;
            }
            i ++;
        }
    }
    return st;
}
/*******************************************************************************
**
** Function         DIS_ReadDISInfo
**
** Description      Read remote device DIS information.
**
** Returns          void
**
*******************************************************************************/
BOOLEAN DIS_ReadDISInfo(BD_ADDR peer_bda, tDIS_READ_CBACK *p_cback)
{
    UINT16             conn_id;

    /* For now we only handle one at a time */
    if (dis_cb.dis_read_uuid_idx != 0xff)
        return(FALSE);

    if (p_cback == NULL)
        return(FALSE);

    dis_cb.p_read_dis_cback = p_cback;
    /* Mark currently active operation */
    dis_cb.dis_read_uuid_idx = 0;

    GATT_TRACE_EVENT ("DIS_ReadDISInfo() - BDA: %08x%04x  cl_read_uuid: 0x%04x",
                      (peer_bda[0]<<24)+(peer_bda[1]<<16)+(peer_bda[2]<<8)+peer_bda[3],
                      (peer_bda[4]<<8)+peer_bda[5], dis_attr_uuid[dis_cb.dis_read_uuid_idx]);


    GATT_GetConnIdIfConnected(srvc_eng_cb.gatt_if, peer_bda, &conn_id, BT_TRANSPORT_LE);

    /* need to enhance it as multiple service is needed */
    srvc_eng_request_channel(peer_bda, SRVC_ID_DIS);

    if (conn_id == GATT_INVALID_CONN_ID)
    {
        return GATT_Connect(srvc_eng_cb.gatt_if, peer_bda, TRUE, BT_TRANSPORT_LE);
    }

    return dis_gatt_c_read_dis_req(conn_id);

}
#endif  /* BLE_INCLUDED */


