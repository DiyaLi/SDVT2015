#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include "ros/ros.h"
#include <geometry_msgs/Twist.h>
#include <math.h>

int sock;
sockaddr_storage addrDest = {};

int resolvehelper(const char* hostname, int family, const char* service, sockaddr_storage* pAddr)
{
    int result;
    addrinfo* result_list = NULL;
    addrinfo hints = {};
    hints.ai_family = family;
    hints.ai_socktype = SOCK_DGRAM; // without this flag, getaddrinfo will return 3x the number of addresses (one for each socket type).
    result = getaddrinfo(hostname, service, &hints, &result_list);
    if (result == 0)
    {
        //ASSERT(result_list->ai_addrlen <= sizeof(sockaddr_in));
        memcpy(pAddr, result_list->ai_addr, result_list->ai_addrlen);
        freeaddrinfo(result_list);
    }

    return result;
}

int checksum(const char* msg)
{
	size_t len = strlen(msg);
	int b[len];
	int sum = 0;
	for (int i = 0; i < len; i++)
	{
		b[i] = (int)msg[i];
		sum = sum + b[i];
	}
	int m = sum % 256;
	return m;
}

void makeStr(char* msg, int enable, int motion, int steer, int throttle, int shift, int brake)
{
	char* msg1 = new char[1000];
	sprintf(msg1,":AB|N0|D%d|D0|D%d|D%d|D0|D%d|D%d|D%d|C",steer,throttle,shift,brake,enable,motion);
	int m = checksum(msg1);
	sprintf(msg,"[%s%x]",msg1,m);
	delete[] msg1;
	return;
}

void startConnection() {
	int result = 0;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    char szIP[100];

    sockaddr_in addrListen = {}; // zero-int, sin_port is 0, which picks a random port for bind.
    addrListen.sin_family = AF_INET;
    result = bind(sock, (sockaddr*)&addrListen, sizeof(addrListen));
    if (result == -1)
    {
       int lasterror = errno;
       std::cout << "error: " << lasterror;
       exit(1);
    }
    result = resolvehelper("192.168.1.201", AF_INET, "1009", &addrDest);
    if (result != 0)
    {
       int lasterror = errno;
       std::cout << "error: " << lasterror;
       exit(1);
    }
}

void chatterCallback(const geometry_msgs::Twist cmd_vel) {
    char* msg = new char[1000];
    size_t msg_length;
	double vx = cmd_vel.linear.x;
	double vth = -cmd_vel.angular.z;
	int throttle = 800;
	int shift = 3;
	int brake = 0;
	double dRadius = vx / vth;
	if (vx < 0) {
		dRadius = -1*dRadius;
		shift = 1;
	}
	if (vx < 0.02 && vx > -0.02) {
		throttle = 0;
		brake = 100;
	}
	double steer1 = 824.588 * pow(dRadius, -0.7438);
    int enable = 1;
    int motion = 1;
	int steer = (int) steer1;
	if (steer > 350) {steer = 350;}
	if (steer < -350) {steer = -350;}
    makeStr(msg, enable, motion, steer, throttle, shift, brake);
    msg_length = strlen(msg);
	int result = sendto(sock, msg, msg_length, 0, (sockaddr*)&addrDest, sizeof(addrDest));
    delete[] msg;
}

int main(int argc, char **argv) {
    ros::init(argc,argv,"listener");
    ros::NodeHandle n;
    startConnection();
    ros::Subscriber sub = n.subscribe("move_base/cmd_vel", 1000, chatterCallback);
    ros::spin();
	close(sock);
	return 0;
}
