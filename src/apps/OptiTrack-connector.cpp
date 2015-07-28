/*

SampleClient.cpp

This program connects to a NatNet server, receives a data stream, and writes that data stream
to an ascii file.  The purpose is to illustrate using the NatNetClient class.

Usage [optional]:

SampleClient [ServerIP] [LocalIP] [OutputFilename]

[ServerIP]			IP address of the server (e.g. 192.168.0.107) ( defaults to local machine)
[OutputFilename]	Name of points file (pts) to write out.  defaults to Client-output.pts

*/

#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <winsock2.h>
#include <string>

#include "NatNetTypes.h"
#include "NatNetClient.h"
#include <boost/algorithm/string.hpp>
#include <Eigen/Eigen>
#include <Eigen/Geometry>


#include "../IO/TCPClient.h"

#define UNREAL_TRANSFORM

#pragma warning( disable : 4996 )

void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData);		// receives data from the server
void __cdecl MessageHandler(int msgType, char* msg);		            // receives NatNet error mesages
void resetClient();
int CreateClient(int iConnectionType);

unsigned int MyServersDataPort = 3130;
unsigned int MyServersCommandPort = 3131;

NatNetClient* theClient;
AR::IO::TCPClient::Ptr tcpClient;

char szMyIPAddress[128] = "";
char szServerIPAddress[128] = "";

int _tmain(int argc, _TCHAR* argv[])
{
	int iResult;
	int iConnectionType = ConnectionType_Multicast;

	// parse command line args
	if (argc != 2)
	{ 
		printf("Error! Please run with the following arguments:\n");
		printf("%s pose-server_ip:port\n", argv[0]);
		return 1;
	}

	//Parsing args
	std::string tcp_remote_ip;
	int tcp_remote_port;
	{
		std::vector<std::string> tokens;
		boost::split(tokens, argv[1], boost::is_any_of(":"));

		if (tokens.size() != 2)
		{
			printf("Error! Please provide the remote address in the format: x.x.x.x:p\n"); 
			return 1;
		}

		tcp_remote_ip = tokens[0];
		tcp_remote_port = std::stoi(tokens[1], nullptr);
	}


	tcpClient = AR::IO::TCPClient::Create();
	tcpClient->connect(tcp_remote_ip, tcp_remote_port, false);


	// Create NatNet Client
	iResult = CreateClient(iConnectionType);
	if (iResult != ErrorCode_OK)
	{
		printf("Error initializing client.  See log for details.  Exiting");
		return 1;
	}
	else
	{
		printf("Client initialized and ready.\n");
	}


	// send/receive test request
	printf("[SampleClient] Sending Test Request\n");
	void* response;
	int nBytes;
	iResult = theClient->SendMessageAndWait("TestRequest", &response, &nBytes);
	if (iResult == ErrorCode_OK)
	{
		printf("[SampleClient] Received: %s", (char*)response);
	}


	// Retrieve Data Descriptions from server
	printf("\n\n[SampleClient] Requesting Data Descriptions...");
	sDataDescriptions* pDataDefs = NULL;
	int nBodies = theClient->GetDataDescriptions(&pDataDefs);
	if (!pDataDefs)
	{
		printf("[SampleClient] Unable to retrieve Data Descriptions."); 
		return 1;
	}
	else
	{
		printf("[SampleClient] Received %d Data Descriptions:\n", pDataDefs->nDataDescriptions);
		for (int i = 0; i < pDataDefs->nDataDescriptions; i++)
		{
			printf("Data Description # %d (type=%d)\n", i, pDataDefs->arrDataDescriptions[i].type);
			if (pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
			{
				// RigidBody
				sRigidBodyDescription* pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
				printf("RigidBody Name : %s\n", pRB->szName);
				printf("RigidBody ID : %d\n", pRB->ID);
				printf("RigidBody Parent ID : %d\n", pRB->parentID);
				printf("Parent Offset : %3.2f,%3.2f,%3.2f\n", pRB->offsetx, pRB->offsety, pRB->offsetz);
			}
			else
			{
				printf("Unknown data type.");
				// Unknown
			}
		}
	}

	// Ready to receive marker stream!
	printf("\nClient is connected to server and listening for data...\n");
	int c;
	bool bExit = false;
	while (c = _getch())
	{
		switch (c)
		{
		case 'q':
			bExit = true;
			break;
		case 'r':
			resetClient();
			break;
		case 'p':
			sServerDescription ServerDescription;
			memset(&ServerDescription, 0, sizeof(ServerDescription));
			theClient->GetServerDescription(&ServerDescription);
			if (!ServerDescription.HostPresent)
			{
				printf("Unable to connect to server. Host not present. Exiting.");
				return 1;
			}
			break;
		case 'f':
			{
				sFrameOfMocapData* pData = theClient->GetLastFrameOfData();
				printf("Most Recent Frame: %d", pData->iFrame);
			}
			break;


		default:
			break;
		}
		if (bExit)
			break;
	}

	// Done - clean up.
	theClient->Uninitialize();
	tcpClient->disconnect();

	return ErrorCode_OK;
}

// Establish a NatNet Client connection
int CreateClient(int iConnectionType)
{
	// release previous server
	if (theClient)
	{
		theClient->Uninitialize();
		delete theClient;
	}

	// create NatNet client
	theClient = new NatNetClient(iConnectionType);

	// print version info
	unsigned char ver[4];
	theClient->NatNetVersion(ver);
	printf("NatNet Sample Client (NatNet ver. %d.%d.%d.%d)\n", ver[0], ver[1], ver[2], ver[3]);

	// Set callback handlers
	theClient->SetMessageCallback(MessageHandler);
	theClient->SetVerbosityLevel(Verbosity_Debug);
	theClient->SetDataCallback(DataHandler, theClient);	// this function will receive data from the server

	// Init Client and connect to NatNet server
	// to use NatNet default port assigments
	int retCode = theClient->Initialize(szMyIPAddress, szServerIPAddress);
	// to use a different port for commands and/or data:
	//int retCode = theClient->Initialize(szMyIPAddress, szServerIPAddress, MyServersCommandPort, MyServersDataPort);
	if (retCode != ErrorCode_OK)
	{
		printf("Unable to connect to server.  Error code: %d. Exiting", retCode);
		return ErrorCode_Internal;
	}
	else
	{
		// print server info
		sServerDescription ServerDescription;
		memset(&ServerDescription, 0, sizeof(ServerDescription));
		theClient->GetServerDescription(&ServerDescription);
		if (!ServerDescription.HostPresent)
		{
			printf("Unable to connect to server. Host not present. Exiting.");
			return 1;
		}
		printf("[SampleClient] Server application info:\n");
		printf("Application: %s (ver. %d.%d.%d.%d)\n", ServerDescription.szHostApp, ServerDescription.HostAppVersion[0],
			ServerDescription.HostAppVersion[1], ServerDescription.HostAppVersion[2], ServerDescription.HostAppVersion[3]);
		printf("NatNet Version: %d.%d.%d.%d\n", ServerDescription.NatNetVersion[0], ServerDescription.NatNetVersion[1],
			ServerDescription.NatNetVersion[2], ServerDescription.NatNetVersion[3]);
		printf("Client IP:%s\n", szMyIPAddress);
		printf("Server IP:%s\n", szServerIPAddress);
		printf("Server Name:%s\n\n", ServerDescription.szHostComputerName);
	}

	return ErrorCode_OK;

}

// DataHandler receives data from the server
void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData)
{
	NatNetClient* pClient = (NatNetClient*)pUserData;

	int i = 0;

	printf("FrameID : %d\n", data->iFrame);
	printf("Timestamp :  %3.2lf\n", data->fTimestamp);
	printf("Latency :  %3.2lf\n", data->fLatency);

	// FrameOfMocapData params
	bool bIsRecording = data->params & 0x01;
	bool bTrackedModelsChanged = data->params & 0x02;
	if (bIsRecording)
		printf("RECORDING\n");
	if (bTrackedModelsChanged)
		printf("Models Changed.\n");


	// timecode - for systems with an eSync and SMPTE timecode generator - decode to values
	int hour, minute, second, frame, subframe;
	bool bValid = pClient->DecodeTimecode(data->Timecode, data->TimecodeSubframe, &hour, &minute, &second, &frame, &subframe);
	// decode to friendly string
	char szTimecode[128] = "";
	pClient->TimecodeStringify(data->Timecode, data->TimecodeSubframe, szTimecode, 128);
	printf("Timecode : %s\n", szTimecode);

	// Rigid Bodies
	printf("Rigid Bodies [Count=%d]\n", data->nRigidBodies);
	std::stringstream ss;
	bool any = false;
	for (i = 0; i < data->nRigidBodies; i++)
	{
		// params
		// 0x01 : bool, rigid body was successfully tracked in this frame
		bool bTrackingValid = data->RigidBodies[i].params & 0x01;

		if (bTrackingValid)
		{
			auto &rb = data->RigidBodies[i];

			float
				x = rb.x,
				y = rb.y,
				z = rb.z,
				qw = rb.qw,
				qx = rb.qx,
				qy = rb.qy,
				qz = rb.qz;
#ifdef UNREAL_TRANSFORM
#define PI 3.141592
			{
				typedef Eigen::AngleAxisd Axis;
				typedef Eigen::Vector3d Vec;
				auto UX = Vec::UnitX();
				auto UY = Vec::UnitY();
				auto UZ = Vec::UnitZ();

				auto q = Eigen::Quaterniond(qw, qx, qy, qz);

				/* Here we 'add' an angular offset */{
					Eigen::Quaterniond
						a(Eigen::AngleAxisd(PI, UX)),
						b(Eigen::AngleAxisd(PI, UY)),
						c(Eigen::AngleAxisd(PI, UZ));

					q = q * a;
				}
				/* Here we invert the rotation axes */{
					auto euler = q.toRotationMatrix().eulerAngles(2, 0, 1);
					auto flip =
						Axis((euler[0]), UX) *
						Axis(-(euler[1]), UY) *
						Axis(-(euler[2]), UZ);

					q = Eigen::Quaterniond(flip);
				}

				float
					tx = x,
					ty = y,
					tz = z;
				x = -tz;
				y = tx;
				z = ty;

				qw = q.w();
				qx = q.x();
				qy = q.y();
				qz = q.z();
			}
#endif


			ss
				<< x << " " << y << " " << z << " "
				<< qw << " " << qx << " " << qy << " " << qz << " "
				<< data->fTimestamp << " " << rb.ID << "\n";

			any = true;
		}

		 
		printf("Rigid Body [ID=%d  Error=%3.2f  Valid=%d]\n", data->RigidBodies[i].ID, data->RigidBodies[i].MeanError, bTrackingValid);
		//printf("\tx\ty\tz\tqx\tqy\tqz\tqw\n");
		printf("\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\n",
			data->RigidBodies[i].x,
			data->RigidBodies[i].y,
			data->RigidBodies[i].z,
			data->RigidBodies[i].qx,
			data->RigidBodies[i].qy,
			data->RigidBodies[i].qz,
			data->RigidBodies[i].qw);
	}

	printf("Sending data...\n");
	if (any && tcpClient)
	{
		if (tcpClient->write(ss.str()))
			printf("Sending data... OK\n");
		else
			printf("Sending data... SOCKET CLOSED\n");
	}
	else
		printf("Sending data... NOPE\n");
}

// MessageHandler receives NatNet error/debug messages
void __cdecl MessageHandler(int msgType, char* msg)
{
	printf("\n%s\n", msg);
}


void resetClient()
{
	int iSuccess;

	printf("\n\nre-setting Client\n\n.");

	iSuccess = theClient->Uninitialize();
	if (iSuccess != 0)
		printf("error un-initting Client\n");

	iSuccess = theClient->Initialize(szMyIPAddress, szServerIPAddress);
	if (iSuccess != 0)
		printf("error re-initting Client\n");


}

