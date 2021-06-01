
#define VESC_DEBUG

#include "vesc.h"


dataPackage data_vesc1;
dataPackage data_vesc2;
Custom_dataPackage Telemetry_data;
Max_vals vesc_dual_extrem;
nunchuckPackage nunchuck; 
volatile vesc_rev_msg vesc_uart2_container; // used in ISR

extern UART_HandleTypeDef huart2;

int RPM_VESC = 0;





int packSendPayload(uint8_t * payload, int lenPay) {

	uint16_t crcPayload = crc16(payload, lenPay);
	int count = 0;
	uint8_t messageSend[256];

	if (lenPay <= 256)
	{
		messageSend[count++] = 2;
		messageSend[count++] = lenPay;
	}
	else
	{
		messageSend[count++] = 3;
		messageSend[count++] = (uint8_t)(lenPay >> 8);
		messageSend[count++] = (uint8_t)(lenPay & 0xFF);
	}

	memcpy(&messageSend[count], payload, lenPay);

	count += lenPay;
	messageSend[count++] = (uint8_t)(crcPayload >> 8);
	messageSend[count++] = (uint8_t)(crcPayload & 0xFF);
	messageSend[count++] = 3;
	messageSend[count] = '\0';

	if(HAL_UART_Transmit(&VESC_UART_PERIPH,messageSend,count,50) == HAL_OK)
	{
		return count ;
	}
	else
	{
		return -1;
	}

	

}


bool processReadPacket(uint8_t * message,dataPackage * data ) {

	COMM_PACKET_ID packetId;
	int32_t ind = 0;

	packetId = (COMM_PACKET_ID)message[0];
	message++; // Removes the packetId from the actual message (payload)

	switch (packetId){
		case COMM_GET_VALUES: // Structure defined here: https://github.com/vedderb/bldc/blob/43c3bbaf91f5052a35b75c2ff17b5fe99fad94d1/commands.c#L164
			ind = 4; // Skip the first 4 bytes 
			data->avgMotorCurrent 	= buffer_get_float32(message, 100.0, &ind);
			data->avgInputCurrent 	= buffer_get_float32(message, 100.0, &ind);
			ind += 8; // Skip the next 8 bytes
			data->dutyCycleNow 		= buffer_get_float16(message, 1000.0, &ind);
			data->rpm 				= buffer_get_int32(message, &ind);
			data->inpVoltage 		= buffer_get_float16(message, 10.0, &ind);
			data->ampHours 			= buffer_get_float32(message, 10000.0, &ind);
			data->ampHoursCharged 	= buffer_get_float32(message, 10000.0, &ind);
			ind += 8; // Skip the next 8 bytes 
			data->tachometer 		= buffer_get_int32(message, &ind);
			data->tachometerAbs 		= buffer_get_int32(message, &ind);
			return true;

		break;

		default:
			return false;
		break;
	}
}

bool getVescValues(void) {
	uint8_t command[1] = { COMM_GET_VALUES };
	packSendPayload(command, 1); // send request

	while(vesc_uart2_container.vesc_msg_received != 1) //waiting for response  TODO: add timeout
	{

	}


		bool read = processReadPacket((uint8_t *)vesc_uart2_container.data,&data_vesc1); //returns true if sucessfull
		vesc_uart2_container.vesc_msg_received = 0; // reset packet state

		return read;

}

bool getVescValuesfwd(uint8_t canID ) {
	uint8_t index = 0;
	uint8_t command[3];
	command[index++] =  COMM_FORWARD_CAN ; //Forwarding CAN
	command[index++] = canID;                //Sending CAN id
	command[index++] =  COMM_GET_VALUES ;  //Requesting Values
	packSendPayload(command,3);

	while(vesc_uart2_container.vesc_msg_received != 1) //waiting for response  TODO: add timeout
	{

	}


	bool read = processReadPacket((uint8_t *)vesc_uart2_container.data,&data_vesc2); //returns true if sucessfull
	memset((uint8_t *)vesc_uart2_container.data,0,100);
	vesc_uart2_container.vesc_msg_received = 0; // reset packet state

	return read;


}

void setNunchuckValues(void) {
	int32_t ind = 0;
	uint8_t payload[11];

	
	payload[ind++] = COMM_SET_CHUCK_DATA;
	payload[ind++] = nunchuck.valueX;
	payload[ind++] = nunchuck.valueY;
	buffer_append_bool(payload, nunchuck.lowerButton, &ind);
	buffer_append_bool(payload, nunchuck.upperButton, &ind);
	
	// Acceleration Data. Not used, Int16 (2 byte)
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;
	payload[ind++] = 0;

	

	packSendPayload(payload, 11);
}

void VescUartSetRPM(int32_t rpm) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_RPM ;
	buffer_append_int32(payload, rpm , &index);
	packSendPayload(payload, 5);
}

void VescUartSetRPMfwd(int32_t rpm,uint8_t canID) {
	int32_t index = 0;
	uint8_t payload[7];
	payload[index++] =  COMM_FORWARD_CAN ; //Forwarding CAN
	payload[index++] = canID;
	payload[index++] = COMM_SET_RPM ;
	buffer_append_int32(payload, rpm , &index);
	packSendPayload(payload, 7);
}

int unpackMessage(uint8_t * message,uint8_t lenMes,uint8_t * payload)
{

	uint8_t endMessage = 0 ;
	uint8_t lenPayload = 0 ;

	uint16_t crcMessage = 0;
	uint16_t crcPayload = 0;

	switch (message[0])
	{
		case 2: //short frame <255
			endMessage = message[1] + 5; //Payload size + 2 for sice + 3 for CRC and End.
			lenPayload = message[1];
			#ifdef VESC_DEBUG

			#endif
			if(message[endMessage - 1] == 3)
			{
				#ifdef VESC_DEBUG

				#endif
				return -4;
			}
			else
			{
				#ifdef VESC_DEBUG

				#endif
				return -3;
			}
		break;

		case 3: // long frame >255
			#ifdef VESC_DEBUG
			//lenPayload = 0; // To remove warning unused

			#endif
			return -5;

		break;

		default: // invalid message
			#ifdef VESC_DEBUG

			#endif
			return -2;

		break;
	}

	// now we will extract the payload then check CRC

	// Rebuild crc:
	crcMessage = message[(lenMes) - 3] << 8;
	crcMessage &= 0xFF00;
	crcMessage += message[(lenMes) - 2];


	// Extract payload:
	memcpy(payload, &message[2], message[1]);

	crcPayload = crc16(payload, message[1]);

	if(crcPayload == crcMessage)
	{
		#ifdef VESC_DEBUG

		#endif
		return 0;
	}
	else
	{
		#ifdef VESC_DEBUG

		#endif
		return -1;
	}
	#ifdef VESC_DEBUG

	#endif



}






