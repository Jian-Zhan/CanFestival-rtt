#include <rtthread.h>
#include <rtdevice.h>

#include "canfestival.h"
#include "timers_driver.h"

#define CANFESTIVAL_CAN_BAUD ((void*)CAN1MBaud)

static rt_device_t can_dev = RT_NULL;
static CO_Data * OD_Data = RT_NULL;
static struct rt_semaphore can_rx_sem;

static rt_err_t can_rx_callback(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&can_rx_sem);
    return RT_EOK;
}

static void can_rx_thread_entry(void *not_used)
{
    int i;
    rt_err_t res;
    struct rt_can_msg rxmsg;
    Message co_msg;

    rt_device_set_rx_indicate(can_dev, can_rx_callback);

    while (1)
    {
        rxmsg.hdr = -1; // disable HDR filtering
        rt_sem_take(&can_rx_sem, RT_WAITING_FOREVER);
        rt_device_read(can_dev, 0, &rxmsg, sizeof(rxmsg));

        co_msg.cob_id = rxmsg.id;
        co_msg.rtr = rxmsg.rtr;
        co_msg.len = rxmsg.len;
        memcpy(co_msg.data, rxmsg.data, rxmsg.len);

        EnterMutex();
        canDispatch(OD_Data, &co_msg);
        LeaveMutex();
    }
}

CAN_PORT canOpen(s_BOARD *board, CO_Data * d)
{
    rt_err_t res;
    rt_thread_t can_rx_thread;

    OD_Data = d;

    can_dev = rt_device_find(CANFESTIVAL_CAN_DEVICE_NAME);
    RT_ASSERT(can_dev);

    res = rt_device_open(can_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(res == RT_EOK);

    res = rt_device_control(can_dev, RT_CAN_CMD_SET_BAUD, CANFESTIVAL_CAN_BAUD);
    RT_ASSERT(res == RT_EOK);

    res = rt_sem_init(&can_rx_sem, "cf_can_rx_sem", 0, RT_IPC_FLAG_FIFO);
    RT_ASSERT(res == RT_EOK);

    can_rx_thread = rt_thread_create("cf_can_rx", can_rx_thread_entry, RT_NULL, 1024, 25, 10);
    if (can_rx_thread != RT_NULL) {
        rt_thread_startup(can_rx_thread);
    }
    else {
        rt_kprintf("create can_rx thread failed!\n");
    }

    res = rt_device_control(can_dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_NORMAL);
    RT_ASSERT(res == RT_EOK);

    return NULL;
}

UNS8 canSend(CAN_PORT notused, Message *m)
{
    struct rt_can_msg msg;
    rt_size_t size;

    msg.hdr = -1; // disable HDR filtering
    msg.id = m->cob_id;
    msg.ide = RT_CAN_STDID;
    msg.rtr = m->rtr;
    msg.len = m->len;
    memcpy(msg.data, m->data, m->len);

    RT_ASSERT(can_dev);
    size = rt_device_write(can_dev, 0, &msg, sizeof(msg));
    if (size == 0) {
        rt_kprintf("can dev write data failed!\n");
        return 0xFF;
    }
    else {
        return 0;
    }
}
