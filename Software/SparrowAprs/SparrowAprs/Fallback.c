/*
	If the board experinces continued crashed, enter CW fallback mode

	In this mode, we will go to ap redefined freq and transmit CW at a 50% duty cycle

*/

void FallBackEnter(void)
{
	// Reconfigure clock. Us HSI so we are not reliant on the HSE

	// Reconfigure radio to the fallback freq

	// Transmit CW
}