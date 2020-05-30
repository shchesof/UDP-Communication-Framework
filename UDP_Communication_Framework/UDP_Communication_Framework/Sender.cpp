#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <time.h>
#include <winsock2.h>
#include "ws2tcpip.h"
#include "zlib.h"
#include "zconf.h"
#include <openssl/sha.h>

#define TARGET_IP "127.0.0.1" //   for NetDerper: "127.0.0.1"

#define TIMEOUT_MS 10000
#define BUFFER_TX_LEN 1024
#define BUFFER_RX_LEN 40
#define SOCKET_NUM_WORD 6
#define WORD_DATA_LEN 4
#define CONTENT_LEN 1003
#define CRC_LENGTH 10
#define WINDOW_SIZE 5
#define TIMEOUT_FOR_SELECTIVE 1
#define STOP_AND_WAIT_MODE 0

SOCKET socketS;
struct sockaddr_in from;
struct sockaddr_in local;
sockaddr_in addrDest;
int fromlen;
int FULL_ERROR;

#define SENDER
#define TARGET_PORT 14000 // 1703		//        for NetDerper: 14000
#define LOCAL_PORT 1988

void InitWinsock() {

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void get_crc(char *crc_string, char *data, int data_size) {

	uLong crc = crc32(0L, Z_NULL, 0);
	Bytef* buf = (Bytef*)data;
	crc = crc32(crc, buf, data_size);
	snprintf(crc_string, CRC_LENGTH + 1, "%010lu", crc);
}

double get_speed(char *buffer_rx) {

	double speed;
	char *ptr;
	char sockets_per_second[6];
	char speed_crc_string[CRC_LENGTH + 1];

	if (strstr(buffer_rx, "SPEED")) {
		for (int i = 0; i < sizeof(sockets_per_second); i++) {
			sockets_per_second[i] = buffer_rx[i + 5];
		}
		sockets_per_second[5] = '\0';
		get_crc(speed_crc_string, sockets_per_second, sizeof(sockets_per_second) - 1);
		for (int i = 0; i < CRC_LENGTH; i++) {
			if (speed_crc_string[i] != buffer_rx[i + 10]) {
				return -1;
			}			
		}
		speed = strtod(sockets_per_second, &ptr);
		return speed;
	}
	else {
		return -1.0;
	}
}

boolean is_receive_speed(char *buffer_speed) {

	fd_set socks;
	struct timeval timeout;
	timeout.tv_sec = TIMEOUT_FOR_SELECTIVE;
	timeout.tv_usec = 0;
	FD_ZERO(&socks);
	FD_SET(socketS, &socks);
	int rv = select(socketS, &socks, NULL, NULL, &timeout);
	if (rv == SOCKET_ERROR) {
		printf("|---------- SOCKET ERROR! ----------|\n");
		exit(1);
	}
	else if (rv == 0) {
		printf("|------------- TIMEOUT! ------------|\n");
		sendto(socketS, "FALSEFALSEFALSEFALSEFALSEFALSEFALSE", 37, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		return false;
	}
	else {
		if (recvfrom(socketS, buffer_speed, BUFFER_RX_LEN + 1, 0, (sockaddr*)&from, &fromlen) != SOCKET_ERROR) {
			return true;
		}
	}
}

boolean is_send_success (char *buffer_rx, double time_for_socket) {
	
	fd_set socks;
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	FD_ZERO(&socks);
	FD_SET(socketS, &socks);
	int rv = select(socketS, &socks, NULL, NULL, &timeout);
	if (rv == SOCKET_ERROR) {
		printf("|---------- SOCKET ERROR! ----------|\n");
		exit(1);
	} else if (rv == 0) {
		printf("|------------- TIMEOUT! ------------|\n");
		return false;
	} else {
		if (recvfrom(socketS, buffer_rx, BUFFER_RX_LEN + 1, 0, (sockaddr*)&from, &fromlen) != SOCKET_ERROR) {
			if (strstr(buffer_rx, "TRUE")) {
				printf("|-------------- TRUE!---------------|\n");
			//	printf("TIME FOR SOCKET: %f.\n", time_for_socket * 1000);
				Sleep(time_for_socket * 1000);
				return true;
			}
			else if (strstr(buffer_rx, "FALSE")) {
				printf("|------------- FALSE! --------------|\n");
				return false;
			}
			else if (strstr(buffer_rx, "SUCCESS")) {
				printf("|------------ SUCCESS! -------------|\n");
				for (int i = 0; i < 5; i++) {
					sendto(socketS, "ENDENDENDENDENDENDENDENDEND", 28, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				FULL_ERROR = 0;
				return true;
			}
			else if (strstr(buffer_rx, "ERROR")) {
				printf("|---------- WRONG SHA1! ------------|\n");
				FULL_ERROR = 1;
				return true;
			}
			else {
				return false;
			}
		}
	}
}

void get_packet(int number, FILE *f, char *buffer_tx, int sizeData) {

	int start_point = number * CONTENT_LEN;
	char packet_number_word[SOCKET_NUM_WORD + 1];
	char data[CONTENT_LEN];
	char crc_string[CRC_LENGTH + 1];

	snprintf(packet_number_word, SOCKET_NUM_WORD + 1, "%06d", number + 1);

	fseek(f, start_point, SEEK_SET);
	fread(data, sizeof(char), sizeData, f);
	fseek(f, 0, SEEK_SET);
	get_crc(crc_string, data, sizeData);

	for (int i = 0; i < SOCKET_NUM_WORD + 1; i++) {
		buffer_tx[i] = packet_number_word[i];
	}
	strcat(buffer_tx, "DATA");
	for (int i = 0; i < sizeof(data); i++) {
		buffer_tx[i + WORD_DATA_LEN + SOCKET_NUM_WORD] = data[i];
	}
	for (int i = 0; i < sizeof(crc_string); i++) {
		buffer_tx[i + WORD_DATA_LEN + SOCKET_NUM_WORD + sizeData] = crc_string[i];
	}
}

void receive_ack(int **check_table_for_sockets, int number_of_sockets) {

	int ack;
	int success = 1;
	char ack_number[7];
	char buffer_rx[BUFFER_RX_LEN + 1];
	char crc_string[CRC_LENGTH + 1];

	int temp_counter = 0;
	int time_is_over = 0;

	fd_set socks;
	struct timeval timeout;
	timeout.tv_sec = TIMEOUT_FOR_SELECTIVE;
	timeout.tv_usec = 0;
	FD_ZERO(&socks);
	FD_SET(socketS, &socks);
	int rv;

	while (1) {
		rv = select(socketS, &socks, NULL, NULL, &timeout);
		if (rv == SOCKET_ERROR) {
			printf("|---------- SOCKET ERROR! ----------|\n");
			exit(1);
		}
		else if (rv == 0) {
			printf("|------------- TIMEOUT! ------------|\n");
			break;
		}
		else {
			if (recvfrom(socketS, buffer_rx, BUFFER_RX_LEN + 1, 0, (sockaddr*)&from, &fromlen) != SOCKET_ERROR) {
				if (strstr(buffer_rx, "ACK")) {
					for (int i = 0; i < sizeof(ack_number) - 1; i++) {
						ack_number[i] = buffer_rx[i + 3];
					}
					ack_number[6] = '\0';
					get_crc(crc_string, ack_number, sizeof(ack_number) - 1);
					for (int i = 0; i < CRC_LENGTH; i++) {
						if (crc_string[i] != buffer_rx[i + 9]) {
						//	printf("--------- CRC IS WRONG!!! ---------\n");
							success = 0;
							break;
						}
					}
					if (success == 1) {
						ack = atoi(ack_number);
						printf("ACK: %d.\n", ack);
						(*check_table_for_sockets)[ack - 1] = 1;
						temp_counter++;
					}
				}
				success = 1;
			}
		}
	}
}

int main() {

	InitWinsock();

	fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;

	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding error!\n");
		getchar(); //wait for press Enter
		return 1;
	}

	FULL_ERROR = 0;
	unsigned char hash[SHA_DIGEST_LENGTH];
	unsigned char buffer_tx_hash[SHA_DIGEST_LENGTH + WORD_DATA_LEN + 1];
	char socket_num_word[SOCKET_NUM_WORD + 1];
	char data[CONTENT_LEN];
	char crc_string[CRC_LENGTH + 1];
	char buffer_tx[BUFFER_TX_LEN];
	char buffer_rx[BUFFER_RX_LEN + 1];
	char size_string[10];
	char* fileheader = "FILENAME";
	char* sizeheader = "SIZE";
	char* startheader = "START";
	char* stopheader = "STOP";
	int size, last_data_size, last_socket;

	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	do {

		FILE *fr;
		SHA_CTX ctx;
		SHA1_Init(&ctx);
		char* dir = "C:/Users/Pavel/Desktop/UDP_Communication_Framework/";
		char* filename = "fun.jpg";
		char *path = (char *)malloc(strlen(dir) + strlen(filename) + 1);
		strcpy(path, dir);
		strcat(path, filename);
		if ((fr = fopen(path, "rb")) == NULL) {
			fprintf(stderr, "The file cannot be opened.\n");
			exit(1);
		}

		// FILE SIZE CALCULATOR
		fseek(fr, 0L, SEEK_END);
		size = ftell(fr);
		rewind(fr);
		int temp_size = size;
		int size_len = 0;
		while (temp_size > 0) {
			temp_size /= 10;
			size_len++;
		}
		_itoa(size, size_string, 10);

		// COOKING THE SOCKET WITH A FILENAME
		strncpy(buffer_tx, fileheader, BUFFER_TX_LEN);
		for (int i = 0; i < strlen(filename); i++) {
			buffer_tx[i + 8] = filename[i];
		}
		get_crc(crc_string, filename, strlen(filename));
		for (int i = 0; i < sizeof(crc_string); i++) {
			buffer_tx[i + strlen(fileheader) + strlen(filename)] = crc_string[i];
		}
		do {
			sendto(socketS, buffer_tx, strlen(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		} while (!is_send_success(buffer_rx, 0));
		printf("......... Sending FILENAME .........\n");

		// COOKING THE SOCKET WITH A SIZE
		strncpy(buffer_tx, sizeheader, BUFFER_TX_LEN);
		for (int i = 0; i < size_len; i++) {
			buffer_tx[i + 4] = size_string[i];
		}
		get_crc(crc_string, size_string, size_len);
		for (int i = 0; i < sizeof(crc_string); i++) {
			buffer_tx[i + strlen(sizeheader) + size_len] = crc_string[i];
		}
		do {
			sendto(socketS, buffer_tx, strlen(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		} while (!is_send_success(buffer_rx, 0));
		printf("........... Sending SIZE ...........\n");

		// COOKING THE SOCKET WITH "START"
		strncpy(buffer_tx, startheader, BUFFER_TX_LEN);
		get_crc(crc_string, startheader, strlen(startheader));
		for (int i = 0; i < sizeof(crc_string); i++) {
			buffer_tx[i + strlen(startheader)] = crc_string[i];
		}
		do {
			sendto(socketS, buffer_tx, strlen(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		} while (!is_send_success(buffer_rx, 0));
		printf(".......... Sending START ...........\n");

		// GETTING MAXIMUM SPEED
		char buffer_speed[BUFFER_RX_LEN];
		double time_for_socket;
		double sockets_per_second;
		boolean speed_is_got = false;

		while (!speed_is_got) {
			if (is_receive_speed(buffer_speed)) {
				sockets_per_second = get_speed(buffer_speed);
				if (sockets_per_second == -1.0) {
					sendto(socketS, "FALSEFALSEFALSEFALSEFALSEFALSEFALSE", 37, 0, (sockaddr*)&addrDest, sizeof(addrDest));
				}
				else {
					time_for_socket = 1 / sockets_per_second;
					speed_is_got = true;
				}
			}
		}

		// SENDING CONTENT
		int base = 0;
		int nextseqnum = WINDOW_SIZE;

		int number_of_sockets;		// add
		last_socket = size / CONTENT_LEN;
		number_of_sockets = last_socket + 1;		// add
		last_data_size = size % CONTENT_LEN;

		// to create an array with numbers of sockets
		int *check_table_for_sockets;		// add
		check_table_for_sockets = (int *)calloc(number_of_sockets, sizeof(char));
		for (int i = 0; i < number_of_sockets; i++) {
			*(check_table_for_sockets + i) = 0;
		}

		while (base < number_of_sockets) {

			for (int i = base; i < nextseqnum; i++) {
				if (*(check_table_for_sockets + i) == 0) {
					if (i == last_socket) {
						get_packet(i, fr, buffer_tx, last_data_size);
					}
					else {
						get_packet(i, fr, buffer_tx, CONTENT_LEN);
					}
					printf(".....Sending the packet #%d.....\n", i + 1);
					sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
				//	Sleep(time_for_socket * 1000);
					if (i == last_socket) {
						break;
					}
				}
			}

			int number_of_waiting_acknowledgements = number_of_sockets - base;

			if (number_of_waiting_acknowledgements < WINDOW_SIZE) {
				receive_ack(&check_table_for_sockets, number_of_waiting_acknowledgements);
			}
			else {
				receive_ack(&check_table_for_sockets, WINDOW_SIZE);
			}

			for (int i = 0; i < number_of_sockets; i++) {
				printf("%d ", *(check_table_for_sockets + i));
			}
			printf("\n");

			for (int i = base; i < nextseqnum; i++) {
				if (*(check_table_for_sockets + i) == 1) {
					base++;
					nextseqnum++;
				}
				else {
					break;
				}
			}
		}

		// GETTING SHA1 OF FILE
		char data_for_SHA1[CONTENT_LEN];
		for (int i = 0; i < number_of_sockets; i++) {
			fseek(fr, i * CONTENT_LEN, SEEK_SET);
			if (i == last_socket) {
				fread(data_for_SHA1, sizeof(char), last_data_size, fr);
				SHA1_Update(&ctx, data_for_SHA1, last_data_size);
			}
			else {
				fread(data_for_SHA1, sizeof(char), CONTENT_LEN, fr);
				SHA1_Update(&ctx, data_for_SHA1, CONTENT_LEN);
			}
			fseek(fr, 0, SEEK_SET);
		}

		/*
		
		if (STOP_AND_WAIT_MODE == 1) {

			int socket = 0;
			double time_for_socket;
			boolean is_last_socket = false;
			last_data_size = size % CONTENT_LEN;
			last_socket = size / CONTENT_LEN;
			time_for_socket = 1 / sockets_per_second;

			while (!is_last_socket) {
				snprintf(socket_num_word, SOCKET_NUM_WORD + 1, "%06d", socket + 1);
				if (socket == last_socket) {
					fread(data, sizeof(char), last_data_size, fr);
					get_crc(crc_string, data, last_data_size);
					SHA1_Update(&ctx, data, last_data_size);
					is_last_socket = true;
				}
				else {
					fread(data, sizeof(char), CONTENT_LEN, fr);
					get_crc(crc_string, data, CONTENT_LEN);
					SHA1_Update(&ctx, data, sizeof(data));
				}
				for (int i = 0; i < SOCKET_NUM_WORD + 1; i++) {
					buffer_tx[i] = socket_num_word[i];
				}
				strcat(buffer_tx, "DATA");
				for (int i = 0; i < sizeof(data); i++) {
					buffer_tx[i + WORD_DATA_LEN + SOCKET_NUM_WORD] = data[i];
				}
				if (socket == last_socket) {
					for (int i = 0; i < sizeof(crc_string); i++) {
						buffer_tx[i + WORD_DATA_LEN + SOCKET_NUM_WORD + last_data_size] = crc_string[i];
					}
				}
				else {
					for (int i = 0; i < sizeof(crc_string); i++) {
						buffer_tx[i + WORD_DATA_LEN + SOCKET_NUM_WORD + CONTENT_LEN] = crc_string[i];
					}
				}
				printf("............Sending FILE.............\n");

				do {
					sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
				} while (!is_send_success(buffer_rx, time_for_socket));
				memset(data, 0, CONTENT_LEN);
				socket++;
			}
		}
		
		*/

		// COOKING THE SOCKET WITH "STOP"
		strncpy(buffer_tx, stopheader, BUFFER_TX_LEN);
		get_crc(crc_string, stopheader, strlen(stopheader));
		for (int i = 0; i < sizeof(crc_string); i++) {
			buffer_tx[i + strlen(stopheader)] = crc_string[i];
		}
		do {
			sendto(socketS, buffer_tx, strlen(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		} while (!is_send_success(buffer_rx, 0));
		printf("........... Sending STOP ...........\n");

		SHA1_Final(hash, &ctx);

		// SENDING SHA
		buffer_tx_hash[0] = 'H';
		buffer_tx_hash[1] = 'A';
		buffer_tx_hash[2] = 'S';
		buffer_tx_hash[3] = 'H';
		for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
			buffer_tx_hash[i + 4] = hash[i];
		}

		printf("SHA: ");
		for (int i = 4; i < SHA_DIGEST_LENGTH; i++) {
			printf("%02x", buffer_tx_hash[i]);
		}
		printf("\n");


		int tries = 0;
		do {
			sendto(socketS, (const char *)buffer_tx_hash, sizeof(buffer_tx_hash), 0, (sockaddr*)&addrDest, sizeof(addrDest));
			tries++;
		} while (!is_send_success(buffer_rx, 0) && (tries < 30));

		fclose(fr);
		free(path);

	} while (FULL_ERROR == 1);

	closesocket(socketS);

	printf("MY CONGRATULATIONS!!!!!!!!!\n");
	getchar(); // wait for press 'Enter'
	return 0;
}
