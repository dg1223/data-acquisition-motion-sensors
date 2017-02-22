/* Written by Shamir Alavi (shamiralavi@cmail.carleton.ca)
Copyright (c) 2016
*/

/*
This program uses the SerialPort class to connect to the CC2650 SensorTag using
the CC2540 Bluetooth dongle manufactured by Texas Instruments.

The algorithm in 10 steps:
	1. Initialize GAP and GATT parameters.
	2. Create a SerialPort object.
	3. Configure port.
	4. Connect to the dongle using the GAP_initialize parameter (Write to dongle,
	   then read from it to empty the data buffer).
	5. Set connection intervals and slave latency for the dongle similarly.
	6. Send a "Device Discovery Request" to search for SensorTag. Search is done
	   by searching for the MAC address of the corresponding SensorTag.
	7. Once found (you get the MAC address 3 times), establish connection. When
	   connection is established successfully, SensorTag will stop advertising.
	8. Enable notification for the sensor you want to use. Currently, the code
	   below works for IR sensor only.
	9. Activate the sensor. As soon as you activate it, you should be able to see
	   the readings continuously if you put your read function in a while loop.
   10. Deactivate the sensor to deactivate reading.

GAP and GATT parameters were taken from TI's BTool software. Their online GATT
table is outdated. Please ues BTool to connect to SensorTag using the CC2540 dongle
to find the parameters.

Future work:
	1. Read 4 bytes at a time to correctly read from the IR sensor.
	2. Use the same algorithm to read from the IMU sensor. Please keep in mind that
	   the IMU sensor has more parameters for different settings.
	3. Convert data from IMU sensor to quaternions.
	4. Replace the hardcoded COM port number and MAC address with a search and select
	   function. The goal is to ask the user which COM port their dongle is connected
	   to and then find the SensorTag device(s), display their addresses and ask the 
	   user to select the one s/he wants to use.
	5. Write code to have the ability to connect to any sensor and read data.

	In short: Make an open source software similar to BLE Device Monitor without the GUI.
*/

//#define _AFXDLL

#include "afxcoll.h"
#include "SerialPort.h"
#include <iostream>
//#include "string.h"
#include <string>
#include <strsafe.h>
#include <windows.h>
#include <sstream>

using namespace std;

// This is a code snippet taken from the webpage of MSDN library
void ErrorExit(LPTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw);
}

string convertBufferToString(unsigned long long src) {
	string tempBuffer;
	std::stringstream stream;
	stream << std::hex << src;
	tempBuffer = stream.str();

	return tempBuffer;
}

// taken from TI SensorTag CC2650 wiki
float convertToRealData(unsigned short hexValue) {
	unsigned short swapped;
	float value;
	// swap bytes to perform endian conversion
	swapped = (hexValue << 8 | hexValue >> 8);
	// right shift by 2 bits and convert to decimal
	swapped = (swapped >> 2);
	value = swapped * 0.03125;		// this is the magic multiplication factor
									// http://processors.wiki.ti.com/index.php/CC2650_SensorTag_User's_Guide#IR_Temperature_Sensor
	return value;
}

int main() {

	// GAP
	unsigned short GAP_initialize[] = { 0x01,0x00,0xFE,0x26,0x08,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
										0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
										0x00,0x00,0x00 };												// Connect with CC2540 USB dongle
	unsigned short GAP_SetParam_MinInterval[] = { 0x01, 0x30, 0xFE, 0x03, 0x15, 0x50, 0x00 };
	unsigned short GAP_SetParam_MaxInterval[] = { 0x01, 0x30, 0xFE, 0x03, 0x16, 0x50, 0x00 };
	unsigned short GAP_SetParam_Latency[] = { 0x01, 0x30, 0xFE, 0x03, 0x1A, 0x00, 0x00 };
	unsigned short GAP_SetParam_Timeout[] = { 0x01, 0x30, 0xFE, 0x03, 0x19, 0xD0, 0x07 };
	unsigned short GAP_DeviceDiscoveryRequest[] = { 0x01, 0x04, 0xFE, 0x03, 0x02, 0x01, 0x00 };						// Search for SensorTag CC2650 (Limited Mode)
	unsigned short GAP_EstablishLinkRequest[] = { 0x01, 0x09, 0xFE, 0x09, 0x00, 0x00, 0x00, 0x04, 0xD2, 0xAE, 0xF8, 0xE6, 0xA0 };	// Connect with SensorTag
	unsigned short GAP_TerminateLinkRequest[] = { 0x01, 0x0A, 0xFE, 0x03, 0x00, 0x00, 0x13 };										// Terminate connection

	// GATT

	// IR sensor
	unsigned short GATT_IRTempOn[] = { 0x01, 0x92, 0xFD, 0x06, 0x00, 0x00, 0x22, 0x00, 0x01, 0x00 };		// enable notification (client charac. config: 01:00)
	unsigned short GATT_IRTempRead_ON[] = { 0x01, 0x92, 0xFD, 0x05, 0x00, 0x00, 0x24, 0x00, 0x01 };			// activate sensor (IR temp config: 01)
	unsigned short GATT_IRTempRead_OFF[] = { 0x01, 0x92, 0xFD, 0x05, 0x00, 0x00, 0x24, 0x00, 0x00 };		// deactivate sensor (IR temp config: 00)

	// IMU
	unsigned short GATT_MovementOn[] = { 0x01, 0x92, 0xFD, 0x06, 0x00, 0x00, 0x3A, 0x00, 0x01, 0x00 };		// enable notification for IMU
	unsigned short GATT_MovementPeriod[] = { 0x01, 0x92, 0xFD, 0x05, 0x00, 0x00, 0x3E, 0x00, 0x50 };		// movement sensor data readout frequency (input*10)ms
																											// write last byte: here, it is 50*10 = 500ms
	unsigned short GATT_MovementRead_ON[] = { 0x01, 0x92, 0xFD, 0x06, 0x00, 0x00, 0x3C, 0x00, 0x7F, 0x03 };	// 7F: all IMU values (Gyro, Acc, Mag);
																											// 0: WOM disabled; 3: 8G Acc range; Resultant bytes: 7F 03
	unsigned short GATT_MovementRead_OFF[] = { 0x01, 0x92, 0xFD, 0x06, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x03 };

	// Open and configure serial port
start:
	CSerialPort port;

	HANDLE hPort;
	hPort = port.OpenPort("COM8");

	port.ConfigurePort(115200, 8, 0, 0, 1);

	BOOL isTimeoutOk = 0;
	isTimeoutOk = port.SetCommunicationTimeouts(0, 0, 100, 0, 0);

	BOOL isWriteOk = 0;


	// Connect with CC2540 USB dongle
	int i = 0;
	int j = 1;
	int devAddrCounter = 0;
	int terminate = 0;
	int restart = 0;

	int posCounter = 0;

	BOOL isReadOk = 0;
	char userInput;

	// unsigned short is 2 bytes or 16 bits
	unsigned long long dataReceived;		// this stores the current reading from the SensorTag
	unsigned short dataReceived3;
	string buffer, allBuffer;
	
	string reversed0[10];
	string reversed1[16];
	string reversed2[10];
	//string reversed[] = { "0", "1",  "2",  "3",  "4",  "5",  "6",  "7", "8", "9" };
	//cout << "Reversed: " << reversed[9] << endl;

	while (i < 42) {
		port.WriteByte(GAP_initialize[i], 1);
		i++;
	}

	cout << "\n\nSuccessfully connected with the CC2540 USB dongle " << endl;
	while (j < 8) {
		port.ReadByte(dataReceived, 8);
		j++;
	}

	//cout << "Entire Datastream: " << allBuffer << endl;

	// Set connection intervals, slave latency and supervision timeout
	i = 0;
	cout << "\nSetting connection intervals and slave latency..." << endl;
	while (i < 21) {
		if (i < 7) {
			port.WriteByte(GAP_SetParam_MinInterval[i], 1);
			i++;
		}
		else if (i >= 7 && i < 14) {
			port.WriteByte(GAP_SetParam_MaxInterval[i], 1);
			i++;
		}
		else {
			port.WriteByte(GAP_SetParam_Latency[i], 1);
			i++;
		}
	}
	j = 1;
	while (j < 3) {							// Dongle sends exactly 9 bytes at this point
		port.ReadByte(dataReceived, 8);
		//cout << "Datastream (setparam interval-latency) " << j << "= " << dataReceived << endl;
		j++;
	}

	cout << "\nSetting supervision timeout..." << endl;
	i = 0;
	while (i < 7) {
		port.WriteByte(GAP_SetParam_Timeout[i], 1);
		i++;
	}
	j = 1;
	while (j < 3) {							// Dongle sends another 5 bytes at this point
		port.ReadByte(dataReceived, 8);
		//cout << "Datastream (setparam timeout) " << j << "= " << dataReceived << endl;
		j++;
	}


	// Scan for SensorTag
	cout << "\nDiscovering SensorTag..." << endl;
	i = 0;
	while (i < 7) {
		port.WriteByte(GAP_DeviceDiscoveryRequest[i], 1);
		i++;
	}

	int m = 0;
	BOOL readData = 0;
	BOOL movementDataFound = 0;
	j = 1;
	std::stringstream stream;
	string MAC = "a0e6f8aed204";										// MAC address for the sensortag
	while (1) {
		port.ReadByte(dataReceived, 8);
		//cout << "Datastream " << j << "= " << dataReceived << endl;
		if (j == 300 && devAddrCounter < 2) {
			cout << "SensorTag discovery timed out. Restarting the session..." << endl;
			cout << "_______________________________________________________" << endl;
			restart = 1;
			break;
		}

		stream << std::hex << dataReceived;
		buffer = stream.str();
		stream.str(std::string());										// clear stringstream buffer for next read
		//cout << "Datastream (string) " << j << "= " << buffer << endl;

		if (buffer.find(MAC) != std::string::npos) {					// if SensorTag MAC address is found
			devAddrCounter += 1;
			cout << "\nDevice Address counter = " << devAddrCounter << "\n" << endl;			
			if (devAddrCounter == 2) {

				// Establish connection with SensorTag	
				cout << "\nSensorTag found! Establishing connection..." << endl;
				i = 0;
				while (i < 13) {
					port.WriteByte(GAP_EstablishLinkRequest[i], 1);
					i++;
				}
				j = 0;
				cout << "Connection established..." << endl;
				cout << "Do you want to activate the Movement sensor? (y/n) ";
				cin >> userInput;
				if (userInput == 'y') {
					// Movement sensor ON
					cout << "\nActivating movement sensor..." << endl;
					i = 0;
					while (i < 10) {
						//port.WriteByte(GATT_IRTempOn[i], 1);
						port.WriteByte(GATT_MovementOn[i], 1);
						i++;
					}
					j = 1;
					while (j < 9) {
						port.ReadByte(dataReceived, 8);
						cout << "Datastream (movement ON) " << j << "= " << hex << dataReceived << endl;
						j++;
					}
					// set data transmission frequency
					cout << "\nSetting data transmission frequency to 500 milliseconds" << endl;
					i = 0;
					while (i < 9) {
						port.WriteByte(GATT_MovementPeriod[i], 1);
						i++;
					}
					j = 1;
					while (j < 5) {
						port.ReadByte(dataReceived, 8);
						cout << "Datastream (movement period) " << j << "= " << hex << dataReceived << endl;
						j++;
					}
					userInput = 'a';
					cout << "\nSensor activated..." << endl;
					cout << "\nDo you want to read from the Movement sensor? (Enter y/n) (Press SPACE to terminate after readout is done.)" << endl;
					cin >> userInput;
					if (userInput == 'y') {
						j = 1;
						i = 0;
						while (i < 10) {
							//port.WriteByte(GATT_IRTempRead_ON[i], 1);
							port.WriteByte(GATT_MovementRead_ON[i], 1);
							i++;
						}
						i = 0;
						while (1) {
							// Check if Spacebar is pressed (the user wants to end the program)
							if (GetAsyncKeyState(VK_SPACE)) {
								terminate += 1;
								while (i < 10) {
									//port.WriteByte(GATT_IRTempRead_OFF[i], 1);
									port.WriteByte(GATT_MovementRead_OFF[i], 1);
									i++;
								}
								i = 0;
								while (i < 7) {
									port.WriteByte(GAP_TerminateLinkRequest[i], 1);
									i++;
								}
								cout << "\nSensor deactivated!" << endl;
							}
							if (terminate > 0) {
								break;
							}
							// Read sensor output
							if (!readData) {
								cout << "\nReading 1st 14 bytes...\n" << endl;
								while (m < 14) {
									port.ReadByte(dataReceived, 8);
									cout << "Datastream (1st 14 bytes) " << m << "= " << hex << dataReceived << endl;
									m++;
								}
								cout << "" << endl;
								readData = 1;
							}
							
							port.ReadByte(dataReceived, 8);
							if (dataReceived == 21929590532) {
								movementDataFound = 1;
								for (int a = 0; a < 3; a++) {
									port.ReadByte(dataReceived, 8);
									if (a == 0) {
										//cout << "\nDatastream (1) " << "= " << hex << dataReceived << endl;
										buffer = convertBufferToString(dataReceived);

										// Little to Big Endian Conversion in pairs (1 byte = 1 pair = 2 chars)

										j = 0;
										posCounter = 1;

										for (i = 9; i >= 0; i--) {
											if (i % 2 == 1) {
												reversed0[posCounter] = buffer[i];
											}
											else {
												reversed0[j] = buffer[i];
												j += 2;
												posCounter = j + 1;
											}												
										}																			
									}

									else if (a == 1) {
										//cout << "\nDatastream (2) " << "= " << hex << dataReceived << endl;
										buffer = convertBufferToString(dataReceived);

										// Little to Big Endian Conversion in pairs (1 byte = 1 pair = 2 chars)

										j = 0;
										posCounter = 1;

										for (i = 15; i >= 0; i--) {
											if (i % 2 == 1) {
												reversed1[posCounter] = buffer[i];
											}
											else {
												reversed1[j] = buffer[i];
												j += 2;
												posCounter = j + 1;
											}
										}
									}

									else {
										//cout << "\nDatastream (3) " << "= " << hex << dataReceived << "\n" << endl;
										buffer = convertBufferToString(dataReceived);

										// Little to Big Endian Conversion in pairs (1 byte = 1 pair = 2 chars)

										j = 0;
										posCounter = 1;

										for (i = 15; i >= 6; i--) {
											if (i % 2 == 1) {
												reversed2[posCounter] = buffer[i];
											}
											else {
												reversed2[j] = buffer[i];
												j += 2;
												posCounter = j + 1;
											}
										}										
									}								
								}
								cout << "Reversed0: ";
								for (i = 0; i < 10; i++) {
									allBuffer.append(reversed0[i]);
									cout << reversed0[i];
								}
								cout << "\n";
								cout << "Reversed1: ";
								for (i = 0; i < 16; i++) {
									allBuffer.append(reversed1[i]);
									cout << reversed1[i];
								}
								cout << "\n";
								cout << "Reversed2: ";
								for (i = 0; i < 10; i++) {
									allBuffer.append(reversed2[i]);
									cout << reversed2[i];
								}
								cout << "\n";

								cout << "\nAfter Endianness conversion: " << endl;
								cout << "Gyro + Acc + Mag: " << allBuffer << endl;

								cout << "\nGyroscope readout: ";
								for (i = 0; i < 12; i++) {
									cout << allBuffer[i];
								}

								cout << "\nAccelerometer readout: ";
								for (i = 12; i < 24; i++) {
									cout << allBuffer[i];
								}

								cout << "\nMagnetometer readout: ";
								for (i = 24; i < 36; i++) {
									cout << allBuffer[i];
								}
								cout << "\n\n";

								movementDataFound = 0;
								allBuffer = "";
								for (i = 0; i < 10; i++) {
									reversed0[i].clear();
								}
								for (i = 0; i < 16; i++) {
									reversed1[i].clear();
								}
								for (i = 0; i < 10; i++) {
									reversed2[i].clear();
								}
								
							}

							/* THIS PORTION TESTS THE IR TEMPERATURE SENSOR */

							//port.ReadByte(dataReceived, 8);
							//cout << "Datastream (residual) " << "= "  << hex << dataReceived << endl;

							//cout << "" << endl;
							//port.ReadByte(dataReceived, 8);
							//cout << "Datastream (unnecessary 1) " << "= " << hex << dataReceived << endl;

							//port.ReadByte(dataReceived, 3);
							//cout << "Datastream (unnecessary 2) " << "= " << hex << dataReceived << endl;

							//port.ReadByte(dataReceived, 6);
							//cout << "\nDatastream (Gyroscope) " << "= " << hex << dataReceived << endl;

							//port.ReadByte(dataReceived, 6);
							//cout << "\nDatastream (Accelerometer) " << "= " << hex << dataReceived << endl;

							//port.ReadByte(dataReceived, 6);
							//cout << "\nDatastream (Magnetometer) " << "= " << hex << dataReceived << endl;

							//cout << "" << endl;
							//port.ReadByte(dataReceived, 3);
							//cout << "Datastream (residual) " << "= " << hex << dataReceived << endl;

							//m = 0;
							//while (m < 5) {
							//	port.ReadByte(dataReceived, 8);
							//	cout << "Datastream (latent phase) " << m << " = " << hex << dataReceived << endl;
							//	m++;
							//}

							/*
							if (dataReceived3 == 8454) {
								//swapped = (dataReceived << 8 | dataReceived >> 8);
								cout << "min temp = " << hex << dataReceived3 << endl;
								for (int a = 0; a < 3; a++) {									
									port.ReadByte3(dataReceived3, 2);
									//cout << "bytes read = " << port.ReadByte2(dataReceived, 2) << endl;
									//data[a] = dataReceived;
									if (a == 0) {
										value = convertToRealData(dataReceived3);
										cout << "Object temperature: "  << hex << dataReceived3 << endl;
										cout << "Object temperature: (real) " << value << endl;
									}
									else if (a == 1){
										value = convertToRealData(dataReceived3);
										cout << "Ambience temperature:  " << hex << dataReceived3 << endl;
										cout << "Ambience temperature (real): " << value << endl;
									}
									else {										
										cout << "max temp:  " << hex << dataReceived3 << endl;
										value = convertToRealData(dataReceived3);
										cout << "max temp (real):  " << value << endl;
									}
								}
								cout << "" << endl;
							}
							*/
							j++;
						}
					}
					else {
						cout << "\nQuitting program..." << endl;
						terminate += 1;
						i = 0;
						while (i < 9) {
							port.WriteByte(GATT_IRTempRead_OFF[i], 1);
							i++;
						}
						i = 0;
						while (i < 7) {
							port.WriteByte(GAP_TerminateLinkRequest[i], 1);
							i++;
						}
						port.ClosePort();
					}
				}
				else {
					cout << "\nQuitting program..." << endl;
					terminate += 1;
					i = 0;
					while (i < 7) {
						port.WriteByte(GAP_TerminateLinkRequest[i], 1);
						i++;
					}
					port.ClosePort();
				}
			}
		}
		if (terminate > 0) {
			port.ClosePort();
			break;
		}
		j++;
	}
	if (restart == 1) {
		port.ClosePort();
		goto start;		// Kind of cornered myself into using a goto statement because the sensor doesn't always respond on the first try
	}
}
