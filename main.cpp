#include <unistd.h>
#include <iostream>
#include <SDL2/SDL.h>
#include "include/dahua/dhnetsdk.h"
#include "include/dahua/dhconfigsdk.h"
#include "include/dahua/dhplay.h"

#include "include/arcsoft/arcsoft_fsdk_face_detection.h"
#include "include/arcsoft/arcsoft_fsdk_face_recognition.h"
#include "include/arcsoft/merror.h"

#define APPID     "DWnUNSz9cH9CbcYoEyFfjEXUCL7jZ7yCRa6aqN4BwKxb"
#define DETECT_SDKKEY    "J1FrMM5Gho9njpmF2YJgicXhBUN9kErWyAgU2T2czgpx"
#define RECOGNIZE_SDKKEY    "J1FrMM5Gho9njpmF2YJgicYBq5Qnzhdhas463YYr6DWS"

#define WORKBUF_SIZE        (40*1024*1024)
#define MAX_FACE_NUM        (50)

using namespace std;

// PLAYSDK的空闲通道号
LONG gPlayPort = 0;
SDL_Texture *sdlTexture;
SDL_Renderer *sdlRenderer;
int screen_w = 500, screen_h = 500;
SDL_Window *screen;

MHandle hRecognizeEngine = NULL;
MHandle hDetectEngine = NULL;

//断线回调
void CALL_METHOD Disconnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
	cout << "Receive disconnect message, where ip:" << pchDVRIP <<
	    " and port:" << nDVRPort << " and login handle:" << lLoginID <<
	    endl;
}

// netsdk 实时回调函数
void CALL_METHOD fRealDataCB(LLONG lRealHandle, DWORD dwDataType,
			     BYTE * pBuffer, DWORD dwBufSize, LDWORD dwUser)
{
	// 把大华实时码流数据送到playsdk中
	PLAY_InputData(gPlayPort, pBuffer, dwBufSize);
	return;
}

int gnIndex = 0;
// playsdk 回调 yuv数据
void CALL_METHOD fDisplayCB(LONG nPort, char *pBuf, LONG nSize, LONG nWidth,
			    LONG nHeight, LONG nStamp, LONG nType,
			    void *pReserved)
{
	SDL_Rect sdlRect;
	// cout << "nSize:" << nSize << ", nWidth:" << nWidth << ", nHeight:" << nHeight << endl;

	//pBuf是数据指针 nSize是buf大小，通过这两个数据，可以取到yuv数据了
	SDL_UpdateTexture(sdlTexture, NULL, pBuf, nWidth);
	//FIX: If window is resize
	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
	SDL_RenderPresent(sdlRenderer);
}

void sdl_init(void) {
	Uint32 pixformat = 0;
	SDL_Thread *refresh_thread;

	const int pixel_w = 2592, pixel_h = 1520;

	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return;
	}
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                     screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n",
		       SDL_GetError());
		return;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	pixformat = SDL_PIXELFORMAT_IYUV;

	sdlTexture = SDL_CreateTexture(sdlRenderer, pixformat, SDL_TEXTUREACCESS_STREAMING,
		pixel_w, pixel_h);
}

void init_arcSoft(void){
	MByte *pDetectWorkMem = (MByte *)malloc(WORKBUF_SIZE);
	if (pDetectWorkMem == NULL) {
		fprintf(stderr, "fail to malloc workbuf\r\n");
		exit(0);
	}

	int ret = AFD_FSDK_InitialFaceEngine(APPID, DETECT_SDKKEY, pDetectWorkMem, WORKBUF_SIZE,
	                                     &hDetectEngine, AFD_FSDK_OPF_0_HIGHER_EXT, 16, MAX_FACE_NUM);
	if (ret != 0) {
		fprintf(stderr, "fail to AFD_FSDK_InitialFaceEngine(): 0x%x\r\n", ret);
		free(pDetectWorkMem);
		exit(0);
	}

	MByte *pRecognizeWorkMem = (MByte *)malloc(WORKBUF_SIZE);
	if (pRecognizeWorkMem == NULL) {
		fprintf(stderr, "fail to malloc engine work buffer\r\n");
		exit(0);
	}

	ret = AFR_FSDK_InitialEngine(APPID, RECOGNIZE_SDKKEY, pRecognizeWorkMem, WORKBUF_SIZE, &hRecognizeEngine);
	if (ret != 0) {
		fprintf(stderr, "fail to AFR_FSDK_InitialEngine(): 0x%x\r\n", ret);
		free(pRecognizeWorkMem);
		exit(0);
	}
}

int main()
{
	LLONG lLoginHandle = 0;
	char szIpAddr[32] = "192.168.1.108";
	char szUser[32] = "admin";
	char szPwd[32] = "1234abcd";
	int nPort = 37777;
	SDL_Event event;

	sdl_init();
	//初始化SDK资源,设置断线回调函数
	CLIENT_Init(Disconnect, 0);

	//获取播放库空闲端口
	PLAY_GetFreePort(&gPlayPort);
	PLAY_SetStreamOpenMode(gPlayPort, STREAME_REALTIME);
	PLAY_OpenStream(gPlayPort, NULL, 0, 900 * 1024);
	PLAY_SetDisplayCallBack(gPlayPort, fDisplayCB, NULL);
	PLAY_Play(gPlayPort, NULL);

	//登陆
	NET_DEVICEINFO_Ex stLoginInfo = { 0 };
	int nErrcode = 0;

	lLoginHandle =
	    CLIENT_LoginEx2(szIpAddr, nPort, szUser, szPwd,
			    (EM_LOGIN_SPAC_CAP_TYPE) 0, NULL, &stLoginInfo,
			    &nErrcode);
	if (0 == lLoginHandle) {
		cout << "Login device failed" << endl;
		cin >> szIpAddr;
		return -1;
	} else {
		cout << "Login device success" << endl;
	}

	//拉流
	LLONG lRealHandle = CLIENT_RealPlayEx(lLoginHandle, 0, NULL);
	if (0 == lRealHandle) {
		cout << "CLIENT_RealPlayEx fail!" << endl;
		sleep(100000);
		return -1;
	}
	cout << "CLIENT_RealPlayEx success!" << endl;

	//设置拉流回调
	CLIENT_SetRealDataCallBack(lRealHandle, fRealDataCB, 0);

	while (1) {
		SDL_WaitEvent(&event);
		if (event.type == SDL_WINDOWEVENT) {
			//If Resize
			SDL_GetWindowSize(screen, &screen_w, &screen_h);
		} else if (event.type == SDL_QUIT) {
			break;
		}
	}

	PLAY_Stop(gPlayPort);
	PLAY_CloseStream(gPlayPort);
	PLAY_ReleasePort(gPlayPort);

	//登出设备
	CLIENT_Logout(lLoginHandle);

	//清理SDK资源
	CLIENT_Cleanup();
	return 0;
}
