#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<fstream>
#include<string>
#include<ctime>
#include<queue>
#include<thread>
#include<winsock2.h>
#pragma comment(lib,"ws2_32.lib")

using namespace std;

//参数
#define send_IP "127.0.0.1"
#define send_Port 6000
#define recv_IP "127.0.0.1"
#define recv_Port 2345
SOCKET sockSend;//创建发送端socket，并绑定传输层协议UDP，数据报套接字，IPv4
SOCKADDR_IN addrSend;//定义发送端地址
SOCKADDR_IN addrRecv;//定义接收端地址
int addr_len = sizeof(sockaddr_in);
const int max_size = 1024;//一个数据包的最大数据量
const int SEND_TIMES = 10;//最大重传次数
const int WAIT_TIME = 0.2*CLOCKS_PER_SEC;//定时器等待时间，1s
int seq = 0;//发送序列号
char* file_buf;//发送文件缓冲区
int file_len = 0;//发送文件长度
int base = 1;//窗口底部
int nextseqnum = 1;//下一个要发送的序号
int N = 5;//窗口大小

//获取当前系统日期和时间
char* get_time()
{
	time_t now = time(0); // 把 now 转换为字符串形式 
	char* dt = ctime(&now);
	return dt;
}

//定义数据包
struct packet
{
	char flag;//标志位 '0':SYN;'1':ACK;'2':FIN
	bool is_final_file;//标记数据包是否为文件最后一组 true:是；false:否
	bool not_empty;//数据包非空标志 true:非空;false:空
	DWORD send_ip, recv_ip;
	u_short send_port, recv_port;
	int Seq;//序号
	int ACK;//应答序号
	int len;//长度
	u_short checksum;//16位的校验和
	char data[max_size];//存放数据

	packet(); //构造函数
};

//数据包初始化
packet::packet() {
	//把整个数据包设置为全零
	memset(this, 0, sizeof(packet));
	send_ip = DWORD(send_IP);
	recv_ip = DWORD(recv_IP);
	send_port = send_Port;
	recv_port = recv_Port;
	Seq = 0;
	ACK = 0;
	len = 0;
	checksum = 0;
}

//计算校验和
void set_checksum(packet& p)
{
	//校验和清零
	memset(&p.checksum, 0, sizeof(p.checksum));
	//整个数据包共有多少个16字节向上取整
	int count = (sizeof(packet) + 1) / 2;
	//指向第一个16位
	u_short* bit16 = (u_short*)(&p);
	u_long sum = 0;
	for (int i = 0; i < count; i++)
	{
		//实现反码求和
		sum += *(bit16++);
		if (sum & 0xffff0000)
		{
			sum -= 0xffff;
			sum += 1;
		}
	}
	p.checksum = ~sum;
	return;
}

//验证校验和
bool is_checksum(packet& p) {
	int count = (sizeof(packet) + 1) / 2;
	u_short* bit16 = (u_short*)(&p);
	u_long sum = 0;
	for (int i = 0; i < count; i++) {
		sum += *(bit16++);
		if (sum & 0xffff0000) {
			sum -= 0xffff;
			sum += 1;
		}
	}
	if ((sum == 0xffff))
		return true;
	else
		return false;
}

//基本发送函数，相比于sendto函数，增加序号，非空检验，校验和
bool my_sendto(packet& packet) {
	packet.not_empty = true;
	set_checksum(packet);
	int state;
	state = sendto(sockSend, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addrRecv, sizeof(sockaddr));
	if (state == SOCKET_ERROR)
		return false;
	else
		return true;
}

//基本接收函数，相比于recvfrom，增加校验和和空包检查
bool my_recvfrom(packet& packet) {
	memset(packet.data, 0, sizeof(packet.data));
	int state;
	//接收到的包存在packet里
	state = recvfrom(sockSend, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addrRecv, &addr_len);
	if (state == SOCKET_ERROR)
		return false;
	else
		return true;
}

//握手建连
bool send_Conn()//发送端，提出建连请求
{
	packet send_syn, recv_ack;
	bool state1, state2;

	//发送建连请求
	send_syn.flag = '0';//SYN
	send_syn.Seq = seq++;
	state1 = my_sendto(send_syn);
	if (state1)
		cout << "[log]" << get_time() << ">>>" << "发送连接请求成功" << endl;
	else
	{
		cout << "[log]" << get_time() << ">>>" << "发送连接请求失败" << endl;
		return false;
	}

	while (1)
	{
		//接收端回应
		state2 = my_recvfrom(recv_ack);
		if (state2)
		{
			cout << "[log]" << get_time() << ">>>" << "收到回应，连接成功！" << endl;
			return true;
		}
	}
}

//挥手断连
bool send_Clo()
{
	packet send_fin, recv_fin;
	bool state1, state2;

	//发送断连请求
	send_fin.flag = '2';
	send_fin.Seq = seq++;
	state1 = my_sendto(send_fin);
	if (state1)
		cout << "[log]" << get_time() << ">>>" << "发送断连请求成功!" << endl;
	else
	{
		cout << "[log]" << get_time() << ">>>" << "发送断连请求失败" << endl;
		return false;
	}

	while (1)
	{
		//接收端回应
		state2 = my_recvfrom(recv_fin);
		if (state2)
		{
			cout << "[log]" << get_time() << ">>>" << "收到回应，断开成功！" << endl;
			return true;
		}
	}
}

//读文件
bool read_file()
{
	cout << "[log]" << get_time() << ">>>" << "请输入文件路径:";
	string path; cin >> path;
	ifstream file;
	//打开文件
	file.open(path, ifstream::in | ios::binary);

	if (file)
	{
		cout << "[log]" << get_time() << ">>>" << "开始读取文件" << endl;

		//获取文件大小
		//将指针指向文件末尾，偏移0
		file.seekg(0, file.end);
		//获取指针当前位置
		file_len = file.tellg();
		//恢复指针为文件开始
		file.seekg(0, file.beg);
		cout << "[log]" << get_time() << ">>>" << "文件大小为 " << file_len << " Byte" << endl;    //字节为单位

		//读取文件并存入file_buf数组
		file_buf = new char[file_len + 1];
		file_buf[file_len] = '\0';
		file.read(file_buf, file_len);
		cout << "[log]" << get_time() << ">>>" << "文件读取成功!" << endl;

		//关闭文件
		file.close();
		return true;
	}
	else
	{
		cout << "[log]" << get_time() << ">>>" << "读取文件失败" << endl;
		return false;
	}
}

//重传函数(序号不增加)
bool re_sendto(packet& packet)
{
	packet.not_empty = true;
	set_checksum(packet);
	int state;
	state = sendto(sockSend, (char*)&packet, sizeof(packet), 0, (struct sockaddr*)&addrRecv, sizeof(sockaddr));
	if (state == SOCKET_ERROR)
		return false;
	else
		return true;
}

//发送缓存队列
queue<packet> message_queue;
//接收缓冲区
packet recv_packet;
//计时器
clock_t start;

//输出窗口日志
void GBN_log()
{
	cout << "当前窗口:[" << base << "-" << base + 4 << "]";
	cout << "  当前发送序号:" << nextseqnum << endl;
}

//发送线程
void send_handle(int packet_num)
{
	if (nextseqnum < base + N && nextseqnum <= packet_num)
	{
		//输出日志
		GBN_log();
		//要发送的包
		packet send_packet;
		//不是最后一个包
		if (nextseqnum != packet_num)
		{
			send_packet.is_final_file = false;
			send_packet.Seq = seq++;
			send_packet.len = max_size;
			for (int j = 0; j < send_packet.len; j++)
				send_packet.data[j] = file_buf[(nextseqnum - 1) * max_size + j];
			my_sendto(send_packet);
			//不确定对方是否收到，先加入缓存队列
			message_queue.push(send_packet);
			nextseqnum++;
		}
		//最后一个包
		else
		{
			send_packet.is_final_file = true;
			send_packet.Seq = seq++;
			send_packet.len = file_len - (nextseqnum - 1) * max_size;
			for (int j = 0; j < send_packet.len; j++)
				send_packet.data[j] = file_buf[(nextseqnum - 1) * max_size + j];
			my_sendto(send_packet);
			//不确定对方是否收到，先加入缓存队列
			message_queue.push(send_packet);
			nextseqnum++;
		}
	}
}

//接收线程
void recv_handle()
{
	//接收应答
	if (my_recvfrom(recv_packet))
	{
		if (recv_packet.flag == '1' && is_checksum(recv_packet))
		{
			//收到窗口底部的回应包，窗口可以向前移动一位
			if (base == recv_packet.ACK)
			{
				//cout << "序号" << base << "已被确认接收" << endl;

				base++;
				//窗口底部的数据包确认收到了，出队列一次
				message_queue.pop();
				//重新计时
				start = clock();
			}
			else
			{
				//cout << "序号" << recv_packet.ACK << "被重复确认，出现丢包！" << endl;
				//do nothing
			}
		}
		else;
		//接收到的回应包错误，do nothing
	}
}

void timeout()
{
	//超时重传,按顺序重传缓存队列中的数据包
	if (clock() - start > WAIT_TIME)
	{
		cout << "***超时，重传窗口内未确认数据包***" << endl;
		//重新计时
		start = clock();

		if (message_queue.size() != 0)
		{
			int t = message_queue.size();
			for (int i = 0; i < t; i++)
			{
				my_sendto(message_queue.front());
				message_queue.push(message_queue.front());
				message_queue.pop();
			}
		}
	}
}

void stop_wait_send_GBN()
{
	//发送文件
	//分组
	int packet_num = file_len / max_size + (file_len % max_size != 0);
	cout << "[log]" << get_time() << ">>>" << "需要发送的包的数量为" << packet_num << "，窗口大小为" << N << "，正在发送文件......" << endl;
	//循环发送数据包
	start = clock();
	while (base <= packet_num)
	{
		thread S_handle = thread(send_handle, packet_num);
		thread R_handle = thread(recv_handle);
		thread Time_handle = thread(timeout);
		S_handle.join();
		R_handle.join();
		Time_handle.join();
	}
}

int main()
{
	int state1, state2;  //存各种函数的返回值
	bool state3;

	WORD wVersionRequested = MAKEWORD(2, 2); //调用者希望使用的socket的最高版本
	WSADATA wsaData;  //可用的Socket的详细信息，通过WSAStartup函数赋值

	//初始化Socket DLL，协商使用的Socket版本；初始化成功则返回0，否则为错误的int型数字
	state1 = WSAStartup(wVersionRequested, &wsaData);
	if (state1 == 0)
		cout << "[log]" << get_time() << ">>>" << "发送端初始化Socket DLL成功！" << endl;
	else
		cout << "[log]" << get_time() << ">>>" << "发送端初始化Socket DLL失败！" << endl;

	//发送端socket及地址绑定
	sockSend = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	addrSend.sin_family = AF_INET;  //IPv4地址类型
	addrSend.sin_addr.s_addr = inet_addr(send_IP);  //发送端IP
	addrSend.sin_port = htons(send_Port);   //发送端的端口号
	state2 = bind(sockSend, (SOCKADDR*)&addrSend, sizeof(SOCKADDR));//绑定地址

	//设置发送端socket非阻塞
	u_long imode = 1;
	ioctlsocket(sockSend, FIONBIO, &imode);

	if (state2 == 0)
		cout << "[log]" << get_time() << ">>>" << "发送端绑定本地地址成功" << endl;
	else
		cout << "[log]" << get_time() << ">>>" << "发送端绑定本地地址失败！" << endl;

	//定义接收端地址
	addrRecv.sin_family = AF_INET;  //IPv4地址类型
	addrRecv.sin_addr.s_addr = inet_addr(recv_IP);  //接收端IP
	addrRecv.sin_port = htons(recv_Port);   //接收端的端口号

	clock_t s,e;
	//建立连接
	state3 = send_Conn();
	if (state3)
	{
		if (read_file())
		{
			//停等发送
			//stop_wait_send();

			clock_t s = clock();
			//GBN
			stop_wait_send_GBN();
			clock_t e = clock();
			//打印传输效率
			cout << "[log]" << get_time() << ">>>" << "传输文件时间为：" << (e - s) / CLOCKS_PER_SEC << "s" << endl;
			cout << "[log]" << get_time() << ">>>" << "吞吐率为:" << ((float)file_len) / ((e - s) / CLOCKS_PER_SEC) << " bytes/s " << endl << endl;
		}
		//断开连接
		send_Clo();
	}

	//结束使用Socket，释放Socket资源
	closesocket(sockSend);
	WSACleanup();

	return 0;
}