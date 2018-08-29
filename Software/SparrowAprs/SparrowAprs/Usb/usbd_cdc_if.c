#include "usbd_cdc_if.h"
#include "../Usb.h"

static int8_t TEMPLATE_Init(void);
static int8_t TEMPLATE_DeInit(void);
static int8_t TEMPLATE_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t TEMPLATE_Receive(uint8_t* pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_CDC_Template_fops = 
{
  TEMPLATE_Init,
  TEMPLATE_DeInit,
  TEMPLATE_Control,
  TEMPLATE_Receive
};

#define USB_BUFFER_SIZE		64
static uint8_t bufferTx[USB_BUFFER_SIZE];
static uint8_t bufferRx[USB_BUFFER_SIZE];

extern USBD_HandleTypeDef USBD_Device;

// Fake line encoding info so that the PC can decide it's properly configured us as a serial device
static uint32_t fakeBaud = 115200;
static uint8_t fakeParity = 0;
static uint8_t fakeFormat = 0;
static uint8_t fakeDatetype = 0;

static int8_t TEMPLATE_Init(void)
{
	USBD_CDC_SetTxBuffer(&USBD_Device, bufferTx, 0);
	USBD_CDC_SetRxBuffer(&USBD_Device, bufferRx);
	return USBD_OK;
}

static int8_t TEMPLATE_DeInit(void)
{ 
	return USBD_OK;
}

static int8_t TEMPLATE_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{ 
	switch (cmd)
	{
		case CDC_SEND_ENCAPSULATED_COMMAND:
			break;

		case CDC_GET_ENCAPSULATED_RESPONSE:
			break;

		case CDC_SET_COMM_FEATURE:
			break;

		case CDC_GET_COMM_FEATURE:
			break;

		case CDC_CLEAR_COMM_FEATURE:
			break;

		// Dummy store line coding from the host
		case CDC_SET_LINE_CODING:
			fakeBaud = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
			fakeFormat = pbuf[4];
			fakeParity = pbuf[5];
			fakeDatetype = pbuf[6];
			break;

		// Dummy readback whatever the host sent previously
		case CDC_GET_LINE_CODING:
			pbuf[0] = (uint8_t)(fakeBaud);
			pbuf[1] = (uint8_t)(fakeBaud >> 8);
			pbuf[2] = (uint8_t)(fakeBaud >> 16);
			pbuf[3] = (uint8_t)(fakeBaud >> 24);
			pbuf[4] = fakeFormat;
			pbuf[5] = fakeParity;
			pbuf[6] = fakeDatetype;  
			break;

		case CDC_SET_CONTROL_LINE_STATE:
			break;

		case CDC_SEND_BREAK:
			break;    
    
		default:
			break;
	}
	return USBD_OK;
}

static int8_t TEMPLATE_Receive(uint8_t* Buf, uint32_t *Len)
{

	// We can now get the next packet
	USBD_CDC_ReceivePacket(&USBD_Device);

	return USBD_OK;
}