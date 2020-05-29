/*
	PSI semestral project
	Sender: Pavel Paklonski
	Receiver: Sofia Shchepetova
	CVUT FEL OI
	Summer 2019
	----------------------------
	This code is pretty awful and I am very sorry about it,
	but we just didn't have time to divide and make it better. At least it works.
*/

// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "zlib.h"
#include "zconf.h"
#include <winsock2.h>
#include "ws2tcpip.h"
#include "sha.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "zdll.lib")
#pragma comment(lib, "zlib.lib")

#define TARGET_IP "147.32.216.85"

#define CRC_LENGTH 10 // application uses CRC32
#define BUFFERS_LEN 1024
#define MAX_COUNTER_LEN 6 // UDP packet (even with using IPv6) is 4 294 967 295 bytes long maximum
#define SPEED 1.0
#define WINDOW_SIZE 5

#define TARGET_PORT 14001 // 1988
#define LOCAL_PORT 1703

SOCKET socketS;
struct sockaddr_in local;
struct sockaddr_in from;
int fromlen;
sockaddr_in addrDest; // to send
char tellSpeed[21];

void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

char *nameOfFile(char* buffer) {
	int nameSize = strlen(buffer) - 8 - CRC_LENGTH;
	char *name = (char*)malloc(nameSize + 1);
	int i;
	for (i = 0; i < nameSize; i++) {
		name[i] = buffer[i + 8];
	}
	name[i] = '\0';
	return name;
}

char* fileSize(char* buffer) {
	int sizeSize = strlen(buffer) - 4 - CRC_LENGTH;
	char *fileSizeChar = (char*)malloc(sizeSize + 1);
	int i;
	for (i = 0; i < sizeSize; i++)
		fileSizeChar[i] = buffer[i + 4];
	fileSizeChar[i] = '\0';
	return fileSizeChar;
}

uLong computingCRC(char* demoBuffer, int length) {
	uLong crc = crc32(0L, Z_NULL, 0);
	Bytef* bufferB = (Bytef*)demoBuffer;
	crc = crc32(crc, bufferB, length);
	return crc;
}

uLong readingSenderCRC(char *buffer_rx, int size) {
	char senderCRC[CRC_LENGTH];
	char *end;
	int h = 0; // idx of senderCRC
	for (int i = size; i < size + 10; i++) {
		senderCRC[h++] = buffer_rx[i];
	}
	uLong senderCRClong = crc32(0L, Z_NULL, 0);
	senderCRClong = strtoull(senderCRC, &end, 10);
	printf("sender's crc is %lu\n", senderCRClong);
	return senderCRClong;
}

int main()
{
	InitWinsock();

	fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;
	socketS = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0){
		printf("Binding error!\n");
	    getchar(); // wait for press Enter
		return 1;
	}

	char buffer_rx[BUFFERS_LEN+1];

	/*----------TO SEND-----------*/
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);
	/*-------------------------*/

	int count = 0, sizeOfFile = 0, numberOfPackets = 0, dataPackets = 1, sizeOfLastPacket = 0, dataSize = 0,
		hashTries = 0, hashEnter = 0;
	char *path;
	const char *filePath = "C:/Users/nephor/Desktop/UDP_Communication_Framework/";
	bool startOK = false, stopOK = false, nameOK = false, sizeOK = false, recievedSuccess = false;
	FILE *f;
	SHA_CTX ctx;
	int recievedPackets[WINDOW_SIZE] = {0};
	const int contentLen = BUFFERS_LEN - CRC_LENGTH - 5 - MAX_COUNTER_LEN;
	char demoBuffer[contentLen], counter[MAX_COUNTER_LEN], buffer[WINDOW_SIZE*contentLen + 1];
	unsigned char sendersHash[SHA_DIGEST_LENGTH], hash[SHA_DIGEST_LENGTH];

	do {
		buffer[WINDOW_SIZE*contentLen-1] = '\0';
		strncpy(buffer_rx, "", BUFFERS_LEN+1);
		printf("Waiting for datagram ...\n");
		if (recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR) {
			printf("Socket error!\n");
			getchar(); // wait for press Enter
			return 1;
		}
		else {
			if (strstr(buffer_rx, "DATA")) {
				printf("I am waiting for packet #%d\n", dataPackets);
				printf("%s\n", buffer_rx);
				// read packet's number
				for (int i = 0; i < MAX_COUNTER_LEN; i++) {
					counter[i] = buffer_rx[i];
				}
				int countPackets = atoi(counter);
				printf("it's #%d packet\n", countPackets);
				// compute data size
				if (countPackets > numberOfPackets) continue;
				if (countPackets == numberOfPackets) 
					dataSize = sizeOfLastPacket - CRC_LENGTH - 1;
				else 
					dataSize = contentLen + 4 + MAX_COUNTER_LEN;
				int length = 0;
				for (int i = 4 + MAX_COUNTER_LEN; i < dataSize; i++) {
					demoBuffer[i - 4 - MAX_COUNTER_LEN] = buffer_rx[i];
					length++;
				}
				/*----------------------CRC--------------------*/
				uLong crc = computingCRC(demoBuffer, length);
				printf("my crc is %lu\n", crc);
				uLong senderCRClong = readingSenderCRC(buffer_rx, dataSize);
				if (crc != senderCRClong) {
					printf("WRONG CRC\n\n");
					continue;
				}
				else {
					printf("CRC OK\n");
					/*--------------SENDING ACK NUMBER--------------*/
					char ackNumber[MAX_COUNTER_LEN+1];
					ackNumber[MAX_COUNTER_LEN] = '\0';
					char acknowledgement[MAX_COUNTER_LEN + CRC_LENGTH + 4];
					snprintf(ackNumber, 7, "%06d", countPackets);
					acknowledgement[MAX_COUNTER_LEN + CRC_LENGTH + 3] = '\0';
					strcpy(acknowledgement, "ACK");
					strcat(acknowledgement, ackNumber);
					printf("%s\n\n", acknowledgement);
					uLong crc = computingCRC(ackNumber, MAX_COUNTER_LEN);
					char crcChar[CRC_LENGTH+1];
					crcChar[CRC_LENGTH] = '\0';
					snprintf(crcChar, CRC_LENGTH + 1, "%010lu", crc);
					strcat(acknowledgement, crcChar);
					sendto(socketS, acknowledgement, strlen(acknowledgement) + 1, 0, (sockaddr*)&addrDest, sizeof(addrDest));

					// if it was the packet i was waiting for
					if (countPackets == dataPackets) {
						for (int i = 0; i < WINDOW_SIZE; i++) {
							// if I have this packet in buffer, delete it from numbers of bufferized packets
							// and write to the file
							if (recievedPackets[i] == dataPackets) {
								recievedPackets[i] = 0;
								printf("Packets to bufferize: ");
								for (int j = 0; j < WINDOW_SIZE; j++)
									printf("%d ", recievedPackets[j]);
								printf("\n\n");
								int h = 0;
								for (int j = i * contentLen; j < (i+1) * contentLen; j++, h++) {
									demoBuffer[h] = buffer[j];
								}
								printf("demoBuffer: %s\n", demoBuffer);
								break;
							}
						}
						fwrite(demoBuffer, 1, dataSize - 4 - MAX_COUNTER_LEN, f);
						SHA1_Update(&ctx, demoBuffer, dataSize - 4 - MAX_COUNTER_LEN);
						dataPackets++;
						// check if I already have some further packets in the buffer to write in the file
						while (1) {
							int counterTries = 0; // to break the loop
							int i;
							for (i = 0; i < WINDOW_SIZE; i++) {
								if (recievedPackets[i] == dataPackets) {
									recievedPackets[i] = 0;
									printf("Packets to bufferize: ");
									for (int i = 0; i < WINDOW_SIZE; i++)
										printf("%d ", recievedPackets[i]);
									printf("\n\n");
									int h = 0;
									for (int j = i *contentLen;  j < (i + 1) *contentLen; j++, h++) {
										demoBuffer[h] = buffer[j];
									}
									printf("demoBuffer: %s\n", demoBuffer);
									fwrite(demoBuffer, 1, dataSize - 4 - MAX_COUNTER_LEN, f);
									SHA1_Update(&ctx, demoBuffer, dataSize - 4 - MAX_COUNTER_LEN);
									//printf("SHA UPDATE DEMO BUFFER: %s\n", demoBuffer);
									dataPackets++;
								}
								else counterTries++;
							}
							if (counterTries == WINDOW_SIZE) break;
						}
					}
					// if I already have this packet
					else if (dataPackets > countPackets) {
						continue;
					}
					// if it is further packet
					else {
						bool goToWhile = false;
						// check if I already have it
						for (int i = 0; i < WINDOW_SIZE; i++) {
							if (recievedPackets[i] == countPackets) {
								printf("Packets to bufferize: ");
								for (int i = 0; i < WINDOW_SIZE; i++)
									printf("%d ", recievedPackets[i]);
								printf("\n\n");
								goToWhile = true;
							}
						}
						if (goToWhile) continue;
						// bufferize packet if I didn't have it
						int idx = 0;
						for (int i = 0; i < WINDOW_SIZE; i++, idx++) {
							if (recievedPackets[i] == 0) {
								recievedPackets[i] = countPackets;
								printf("Packets to bufferize: ");
								for (int i = 0; i < WINDOW_SIZE; i++)
									printf("%d ", recievedPackets[i]);
								printf("\n\n");
								char contentWithCounter[contentLen];
								for (int i = 0; i < contentLen; i++)
									contentWithCounter[i] = buffer_rx[i+4+MAX_COUNTER_LEN];
								int h = 0;
								for (int i = idx * contentLen; i < (idx + 1)*contentLen; i++, h++) {
									buffer[i] = contentWithCounter[h];
								}
								printf("BUFFER: %s\n", buffer);
								break;
							}
						}
					}
				}
			}

			else if (strstr(buffer_rx, "START")) {
				// to avoid repeating receiving
				if (startOK) {
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					sendto(socketS, tellSpeed, strlen(tellSpeed), 0, (sockaddr*)&addrDest, sizeof(addrDest));
					continue;
				}
				printf("%s\n", buffer_rx);
				/*----------------------CRC--------------------*/
				uLong crc = computingCRC("START", 5);
				uLong senderCRClong = readingSenderCRC(buffer_rx, 5);
				if (crc != senderCRClong) {
					printf("WRONG CRC\n\n");
					sendto(socketS, "FALSEFALSEFALSEFALSEFALSEFALSEFALSEFALSE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				else {
					SHA1_Init(&ctx);
					printf("shaInit: ");
					for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
						printf("%02x", hash[i]);
					}
					printf("\n");
					startOK = true;
					printf("CRC OK\n\n");
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					/*--------send prefered speed-------------*/
					char speed[6];
					speed[5] = '\0';
					snprintf(speed, 6, "%05f", SPEED);
					tellSpeed[20] = '\0';
					strcpy(tellSpeed, "SPEED");
					strcat(tellSpeed, speed);
					printf("%s\n", speed);
					printf("strlen(speed) is %d\n", strlen(speed));
					Bytef* bufferB = (Bytef*)speed;
					uLong crcSpeed = crc32(0L, Z_NULL, 0);
					crcSpeed = crc32(crcSpeed, bufferB, strlen(speed));
					printf("crc is %lu\n", crcSpeed);
					char crcChar[11];
					snprintf(crcChar, 11, "%010lu", crcSpeed);
					strcat(tellSpeed, crcChar);
					printf("buffer_rx is %s\n\n", tellSpeed);
					sendto(socketS, tellSpeed, strlen(tellSpeed), 0, (sockaddr*)&addrDest, sizeof(addrDest));
					/*----------------------------------------*/
				}
				continue;
			}

			else if (strstr(buffer_rx, "STOP")) {
				// to avoid repeating receiving
				if (stopOK) {
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					continue;
				}
				printf("%s\n", buffer_rx);
				/*----------------------CRC--------------------*/
				uLong crc = computingCRC("STOP", 4);
				uLong senderCRClong = readingSenderCRC(buffer_rx, 4);
				if (crc != senderCRClong) {
					printf("WRONG CRC\n\n");
					sendto(socketS, "FALSEFALSEFALSEFALSEFALSEFALSEFALSEFALSE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				else {
					stopOK = true;
					printf("CRC OK\n\n");
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				continue;
			}

			else if (strstr(buffer_rx, "SIZE")) {
				// to avoid repeating receiving
				if (sizeOK) {
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					continue;
				}
				// reading size of the file
				char* fileSizeChar = fileSize(buffer_rx);
				sizeOfFile = atoi(fileSizeChar);
				printf("SIZE OF FILE IS %d\n", sizeOfFile);
				numberOfPackets = sizeOfFile / contentLen + 1;
				sizeOfLastPacket = sizeOfFile % (contentLen)+15 + MAX_COUNTER_LEN;
				printf("size of the last packet is %d\n", sizeOfLastPacket);
				printf("number of packets is %d\n", numberOfPackets);
				dataSize = strlen(buffer_rx) - CRC_LENGTH;
				/*----------------------CRC--------------------*/
				uLong crc = computingCRC(fileSizeChar, strlen(fileSizeChar));
				uLong senderCRClong = readingSenderCRC(buffer_rx, dataSize);
				if (crc != senderCRClong) {
					printf("WRONG CRC\n\n");
					sendto(socketS, "FALSEFALSEFALSEFALSEFALSEFALSEFALSEFALSE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				else {
					sizeOK = true;
					printf("CRC OK\n\n");
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				free(fileSizeChar);
				continue;
			}

			else if (strstr(buffer_rx, "FILENAME")) {
				// to avoid repeating receiving
				if (nameOK) {
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					continue;
				}
				// reading name of the file
				char *filename = nameOfFile(buffer_rx);
				printf("NAME OF FILE IS %s\n", filename);
				dataSize = strlen(filename) + 8;
				path = (char*)malloc(strlen(filename) + strlen(filePath) + 1);
				strcpy(path, filePath);
				strcat(path, filename);
				/*----------------------CRC--------------------*/
				uLong crc = computingCRC(filename, strlen(filename));
				uLong senderCRClong = readingSenderCRC(buffer_rx, dataSize);
				if (crc != senderCRClong) {
					printf("WRONG CRC\n\n");
					sendto(socketS, "FALSEFALSEFALSEFALSEFALSEFALSEFALSEFALSE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				else {
					nameOK = true;
					printf("CRC OK\n\n");
					f = fopen(path, "wb");
					if (f == NULL) {
						printf("FILE NOT FOUND\n");
						exit(-1);
					}
					sendto(socketS, "TRUETRUETRUETRUETRUETRUETRUETRUETRUETRUE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				free(filename);
				continue;
			}

			else if (strstr(buffer_rx, "HASH")) {
				// if file is already recieved
				if (recievedSuccess) {
					printf("File is already saved, sending SUCCESS to stop timeout...\n");
					sendto(socketS, "SUCCESSSUCCESSSUCCESSSUCCESSSUCCESSSUCC", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					continue;
				}
				hashEnter++;
				printf("%s\n", buffer_rx);
				for (int i = 4; i < SHA_DIGEST_LENGTH + 4; i++) {
					sendersHash[i-4] = (unsigned char)buffer_rx[i];
				}
				// computing SHA
				if (hashEnter == 1) 
					SHA1_Final(hash, &ctx);
				printf("sha: ");
				for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
					printf("%02x", hash[i]);
				}
				printf("\n");
				// checking SHA
				int shaTrue = 0;
				for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
					if (hash[i] == sendersHash[i])
						shaTrue++;
				}
				// hash is correct
				if (shaTrue == SHA_DIGEST_LENGTH) {
					printf("File was received correctly.\n");
					sendto(socketS, "SUCCESSSUCCESSSUCCESSSUCCESSSUCCESSSUCC", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					recievedSuccess = true;
					continue;
				}
				// wrong hash, trying to receive it again
				else if (hashTries < 10) {
					printf("ERROR OF TRANSMISSION, TRYING TO RECEIVE HASH AGAIN\n\n");
					sendto(socketS, "FALSEFALSEFALSEFALSEFALSEFALSEFALSEFALSE", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					hashTries++;
					continue;
				}
				// wrong hash for 10 times
				else {
					printf("ERROR OF TRANSMISSION, TRYING TO RECEIVE THE WHOLE FILE AGAIN\n\n");
					sendto(socketS, "ERRORERRORERRORERRORERRORERRORERRORERROR", 41, 0, (sockaddr*)&addrDest, sizeof(addrDest));
					fclose(f);
					remove(path);
					// setting everything to the begining positions 
					count = 0;
					dataPackets = 1;
					hashTries = 0;
					hashEnter = 0;
					startOK = false, stopOK = false, nameOK = false, sizeOK = false;
					recievedSuccess = false;
					//memset(&ctx, 0, sizeof(SHA_CTX));
					memset(hash, 0, SHA_DIGEST_LENGTH);
					for (int i = 0; i < WINDOW_SIZE; i++)
						recievedPackets[i] = 0;
					free(path);
					path = NULL;
					continue;
				}
			}
			else if (strstr(buffer_rx, "FALSE")) {
				printf("CRC of speed was incorrect, sending speed again...\n\n");
				sendto(socketS, tellSpeed, strlen(tellSpeed), 0, (sockaddr*)&addrDest, sizeof(addrDest));
				continue;
			}
			else if (strstr(buffer_rx, "END")) {
				printf("End of communication.\n");
				break;
			}
		}
	} while (1);
	closesocket(socketS);
	fclose(f);
	free(path);

	getchar(); //wait for press Enter
	return 0;
}