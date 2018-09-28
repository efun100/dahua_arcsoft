#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>


#include <SDL2/SDL.h>
#include "include/dahua/dhnetsdk.h"
#include "include/dahua/dhconfigsdk.h"
#include "include/dahua/dhplay.h"

#include "include/arcsoft/arcsoft_fsdk_face_detection.h"
#include "include/arcsoft/arcsoft_fsdk_face_recognition.h"
#include "include/arcsoft/merror.h"

#include <opencv2/opencv.hpp>

#define APPID     "DWnUNSz9cH9CbcYoEyFfjEXUCL7jZ7yCRa6aqN4BwKxb"
#define DETECT_SDKKEY    "J1FrMM5Gho9njpmF2YJgicXhBUN9kErWyAgU2T2czgpx"
#define RECOGNIZE_SDKKEY    "J1FrMM5Gho9njpmF2YJgicYBq5Qnzhdhas463YYr6DWS"

#define WORKBUF_SIZE        (40*1024*1024)
#define MAX_FACE_NUM        (50)

using namespace std;
using namespace cv;

// PLAYSDK的空闲通道号
LONG gPlayPort = 0;
SDL_Texture *sdlTexture;
SDL_Renderer *sdlRenderer;
int screen_w = 500, screen_h = 500;
SDL_Window *screen;

MHandle hRecognizeEngine = NULL;
MHandle hDetectEngine = NULL;
LLONG lLoginHandle = 0;

struct modelInfo {
	char name[100];
	vector<AFR_FSDK_FACEMODEL> faceModels;
};

AFR_FSDK_FACEMODEL getAFR_FSDK_FACEMODELFromMat(Mat img)
{
	AFR_FSDK_FACEMODEL faceModel = { 0 };
	ASVLOFFSCREEN modelImg = { 0 };

	// model = imread("liuyifei.jpg");
	cout << img.cols << endl;
	cout << img.rows << endl;

	modelImg.u32PixelArrayFormat = ASVL_PAF_RGB24_B8G8R8;
	modelImg.i32Width = img.cols;
	modelImg.i32Height = img.rows;
	modelImg.ppu8Plane[0] = img.data;
	if (!modelImg.ppu8Plane[0]) {
		fprintf(stderr, "fail to fu_ReadFile: %s\r\n", strerror(errno));
		exit(0);
	}

	modelImg.pi32Pitch[0] = modelImg.i32Width * 3;

	LPAFD_FSDK_FACERES faceResults;
	int ret = AFD_FSDK_StillImageFaceDetection(hDetectEngine, &modelImg, &faceResults);
	if (ret != 0) {
		fprintf(stderr, "fail to AFD_FSDK_StillImageFaceDetection(): 0x%x\r\n", ret);
		exit(0);
	}

	if (faceResults->nFace != 1) {
		cout << "faceResult != 1" << endl;
		exit(0);
	}

	AFR_FSDK_FACEINPUT faceResult;
	faceResult.lOrient = AFR_FSDK_FOC_0;
	faceResult.rcFace.left = faceResults->rcFace[0].left;
	faceResult.rcFace.top = faceResults->rcFace[0].top;
	faceResult.rcFace.right = faceResults->rcFace[0].right;
	faceResult.rcFace.bottom = faceResults->rcFace[0].bottom;
	AFR_FSDK_FACEMODEL LocalFaceModels = { 0 };
	ret = AFR_FSDK_ExtractFRFeature(hRecognizeEngine, &modelImg, &faceResult, &LocalFaceModels);
	if (ret != 0) {
		fprintf(stderr, "fail to AFR_FSDK_ExtractFRFeature in Image A\r\n");
		exit(0);
	}
	faceModel.lFeatureSize = LocalFaceModels.lFeatureSize;
	faceModel.pbFeature = (MByte*)malloc(faceModel.lFeatureSize);
	memcpy(faceModel.pbFeature, LocalFaceModels.pbFeature, faceModel.lFeatureSize);
	return faceModel;
}

vector<modelInfo> modelInfos;

void init_models()
{
	// Mat model;
	// model = imread("model.jpg");
	// faceModels1 = getAFR_FSDK_FACEMODELFromMat(model);

	DIR *dfd;
	char *pathname = "../models";
	char name[MAX_PATH];
	struct dirent *dp;
	if ((dfd = opendir(pathname)) == NULL) {
		printf("dir_order: can't open %s\n %s", pathname, strerror(errno));
		return;
	}

	while ((dp = readdir(dfd)) != NULL) {
		if (strncmp(dp->d_name, ".", 1) == 0)
			continue; /* 跳过当前文件夹和上一层文件夹以及隐藏文件*/
		if (strlen(pathname) + strlen(dp->d_name) + 2 > sizeof(name)) {
			printf("dir_order: name %s %s too long\n", pathname, dp->d_name);
		} else {
			struct stat s_buf;
			memset(name, 0, sizeof(name));
			sprintf(name, "%s/%s", pathname, dp->d_name);
			cout << name << endl;
			stat(name, &s_buf);

			DIR *subDfd = opendir(name);
			if (subDfd != NULL) {
				struct dirent *subdp;
				struct modelInfo m;
				strcpy(m.name, dp->d_name);
				while ((subdp = readdir(subDfd)) != NULL) {
					cout << subdp->d_name << endl;
					if (strncmp(subdp->d_name, ".", 1) == 0)
						continue; /* 跳过当前文件夹和上一层文件夹以及隐藏文件*/
					if (!strstr(subdp->d_name, ".jpg")) {
						cout << __LINE__ << endl;
						continue;
					}

					char imgname[MAX_PATH];
					memset(imgname, 0 , sizeof(imgname));
					if (strlen(name) + strlen(subdp->d_name) + 2 > sizeof(imgname)) {
						printf("%s: name %s %s too long\n", __func__, name, dp->d_name);
					} else {
						sprintf(imgname, "%s/%s", name, subdp->d_name);
						cout << imgname << endl;
						Mat img = imread(imgname);
						if (!img.data) {
							continue;
						}
						m.faceModels.push_back(getAFR_FSDK_FACEMODELFromMat(img));
					}
				}
				cout << "models.size:" << modelInfos.size() << endl;
				modelInfos.push_back(m);
				closedir(subDfd);
			}
		}
	}
	closedir(dfd);
}

//断线回调
void CALL_METHOD Disconnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
	cout << "Receive disconnect message, where ip:" << pchDVRIP << " and port:" << nDVRPort << " and login handle:" <<
	     lLoginID << endl;
}

// netsdk 实时回调函数
void CALL_METHOD fRealDataCB(LLONG lRealHandle, DWORD dwDataType, BYTE * pBuffer, DWORD dwBufSize, LDWORD dwUser)
{
	// 把大华实时码流数据送到playsdk中
	PLAY_InputData(gPlayPort, pBuffer, dwBufSize);
	return;
}

int gnIndex = 0;
char lastKey = 0;
bool captureFlag = false;
// playsdk 回调 yuv数据
void CALL_METHOD fDisplayCB(LONG nPort, char *pBuf, LONG nSize, LONG nWidth, LONG nHeight, LONG nStamp, LONG nType,
                            void *pReserved)
{
	SDL_Rect sdlRect;
#if 1
	Mat yuvImg(nHeight * 3 / 2, nWidth, CV_8UC1, pBuf);
	Mat img;
	cvtColor(yuvImg, img, CV_YUV2BGR_I420);

	ASVLOFFSCREEN inputImg = { 0 };
	inputImg.u32PixelArrayFormat = ASVL_PAF_RGB24_B8G8R8;
	inputImg.i32Width = img.cols;
	inputImg.i32Height = img.rows;
	inputImg.ppu8Plane[0] = img.data;

	inputImg.pi32Pitch[0] = inputImg.i32Width * 3;
	LPAFD_FSDK_FACERES faceResults;
	int ret = AFD_FSDK_StillImageFaceDetection(hDetectEngine, &inputImg, &faceResults);
	if (ret != 0) {
		fprintf(stderr, "fail to AFD_FSDK_StillImageFaceDetection(): 0x%x\r\n", ret);
	}
	for (int i = 0; i < faceResults->nFace; i++) {
		printf("face %d:(%d,%d,%d,%d)\r\n", i,
		       faceResults->rcFace[i].left, faceResults->rcFace[i].top,
		       faceResults->rcFace[i].right, faceResults->rcFace[i].bottom);

		ASVLOFFSCREEN faceImg = { 0 };
		faceImg.u32PixelArrayFormat = ASVL_PAF_RGB24_B8G8R8;
		faceImg.i32Width = img.cols;
		faceImg.i32Height = img.rows;
		faceImg.ppu8Plane[0] = img.data;
		if (!faceImg.ppu8Plane[0]) {
			fprintf(stderr, "fail to fu_ReadFile: %s\r\n", strerror(errno));
			exit(0);
		}

		faceImg.pi32Pitch[0] = faceImg.i32Width * 3;

		AFR_FSDK_FACEMODEL faceModels2 = { 0 };
		AFR_FSDK_FACEMODEL LocalFaceModels = { 0 };
		AFR_FSDK_FACEINPUT faceResult;
		faceResult.lOrient = AFR_FSDK_FOC_0;
		faceResult.rcFace.left = faceResults->rcFace[i].left;
		faceResult.rcFace.top = faceResults->rcFace[i].top;
		faceResult.rcFace.right = faceResults->rcFace[i].right;
		faceResult.rcFace.bottom = faceResults->rcFace[i].bottom;
		ret = AFR_FSDK_ExtractFRFeature(hRecognizeEngine, &faceImg, &faceResult, &LocalFaceModels);
		if (ret != 0) {
			fprintf(stderr, "fail to AFR_FSDK_ExtractFRFeature in Image B\r\n");
			continue;
		}

		if (captureFlag) {
			char tmpName[100] = {0};
			sprintf(tmpName, "%d.jpg", time(NULL));
			imwrite(tmpName, img);
			cout << "capture success:" << tmpName << endl;
			exit(0);
		}

		faceModels2.lFeatureSize = LocalFaceModels.lFeatureSize;
		faceModels2.pbFeature = (MByte*)malloc(faceModels2.lFeatureSize);
		memcpy(faceModels2.pbFeature, LocalFaceModels.pbFeature, faceModels2.lFeatureSize);

		MFloat fSimilScore = 0.0f;
		int similarIndex = 0;
		float similarScore = 0.0;
		for (int j = 0; j < modelInfos.size(); j++) {
			for (int k = 0; k < modelInfos[j].faceModels.size(); k++) {
				ret = AFR_FSDK_FacePairMatching(hRecognizeEngine, &modelInfos[j].faceModels[k], &faceModels2, &fSimilScore);
				if (fSimilScore > 0.7 && fSimilScore > similarScore) {
					similarScore = fSimilScore;
					similarIndex = j;
				}
			}
		}
		printf("fSimilScore == %f, modelInfos.size = %d\r\n", fSimilScore, modelInfos.size());
		char text[100] = {0};
		if (similarScore > 0.75) {
			sprintf(text, "%s %f", modelInfos[similarIndex].name, similarScore);
			putText(img, text, Point(faceResults->rcFace[i].left, faceResults->rcFace[i].top),
			        FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 255), 2);
		}
		rectangle(img, cvPoint(faceResults->rcFace[i].left, faceResults->rcFace[i].top),
		          cvPoint(faceResults->rcFace[i].right, faceResults->rcFace[i].bottom), cvScalar(0, 0, 255), 3, 4, 0);
	}
	namedWindow("dahua", CV_WINDOW_NORMAL);
	imshow("dahua", img);
	char keynum = waitKey(1);
	switch (keynum) {
	case '+':
	case -85:
		if (lastKey != '+')
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_ADD_CONTROL, 2, 2, 2, false);
		lastKey = '+';
		break;
	case '-':
	case -83:
		if (lastKey != '-')
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_DEC_CONTROL, 2, 2, 2, false);
		lastKey = '-';
		break;
	case 'Q':	//left
		if (lastKey != 'Q')
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_LEFT_CONTROL, 2, 2, 2, false);
		lastKey = 'Q';
		break;
	case 'S':	//right
		if (lastKey != 'S')
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_RIGHT_CONTROL, 2, 2, 2, false);
		lastKey = 'S';
		break;
	case 'R':	//up
		if (lastKey != 'R')
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_UP_CONTROL, 2, 2, 2, false);
		lastKey = 'R';
		break;
	case 'T':	//down
		if (lastKey != 'T')
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_DOWN_CONTROL, 2, 2, 2, false);
		lastKey = 'T';
		break;
	case 'q':
		cout << "q" << endl;
		exit(0);
	case -1:
		//stop
		// CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_DOWN_CONTROL, 2, 2, 2, true);
		switch (lastKey) {
		case '+':
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_ADD_CONTROL, 2, 2, 2, true);
			break;
		case '-':
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_DEC_CONTROL, 2, 2, 2, true);
			break;
		case 'Q':	//left
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_LEFT_CONTROL, 2, 2, 2, true);
			break;
		case 'S':	//right
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_RIGHT_CONTROL, 2, 2, 2, true);
			break;
		case 'R':	//up
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_UP_CONTROL, 2, 2, 2, true);
			break;
		case 'T':	//down
			CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_DOWN_CONTROL, 2, 2, 2, true);
			break;
		default:
			break;
		}
		lastKey = -1;
		break;
	case 'c':
		captureFlag = true;
		break;
	default:
		printf("%d\n", keynum);
		break;
	}
	return;
#endif
#if 0
	// cout << "nSize:" << nSize << ", nWidth:" << nWidth << ", nHeight:" << nHeight << endl;
	ASVLOFFSCREEN inputImg = { 0 };
	inputImg.u32PixelArrayFormat = ASVL_PAF_I420;
	inputImg.i32Width = nWidth;
	inputImg.i32Height = nHeight;
	inputImg.ppu8Plane[0] = (MUInt8 *)pBuf;

	inputImg.pi32Pitch[0] = inputImg.i32Width;
	inputImg.pi32Pitch[1] = inputImg.i32Width / 2;
	inputImg.pi32Pitch[2] = inputImg.i32Width / 2;
	inputImg.ppu8Plane[1] = inputImg.ppu8Plane[0] + inputImg.pi32Pitch[0] * inputImg.i32Height;
	inputImg.ppu8Plane[2] = inputImg.ppu8Plane[1] + inputImg.pi32Pitch[1] * inputImg.i32Height / 2;

	LPAFD_FSDK_FACERES faceResults;
	int ret = AFD_FSDK_StillImageFaceDetection(hDetectEngine, &inputImg, &faceResults);
	if (ret != 0) {
		cout << "face detect failed" << endl;
	}
	for (int i = 0; i < faceResults->nFace; i++) {
		printf("face %d:(%d,%d,%d,%d)\r\n", i, faceResults->rcFace[i].left, faceResults->rcFace[i].top,
		       faceResults->rcFace[i].right, faceResults->rcFace[i].bottom);

		AFR_FSDK_FACEMODEL LocalFaceModels = { 0 };
		AFR_FSDK_FACEINPUT faceResult;
		faceResult.lOrient = AFR_FSDK_FOC_0;
		faceResult.rcFace.left = faceResults->rcFace[i].left;
		faceResult.rcFace.top = faceResults->rcFace[i].top;
		faceResult.rcFace.right = faceResults->rcFace[i].right;
		faceResult.rcFace.bottom = faceResults->rcFace[i].bottom;
		ret = AFR_FSDK_ExtractFRFeature(hRecognizeEngine, &inputImg, &faceResult, &LocalFaceModels);
		if (ret != 0) {
			fprintf(stderr, "fail to AFR_FSDK_ExtractFRFeature in Image B\r\n");
			continue;
		}

		MFloat fSimilScore = 0.0f;
		int similarIndex = 0;
		float similarScore = 0.0;
		for (int j = 0; j < modelInfos.size(); j++) {
			for (int k = 0; k < modelInfos[j].faceModels.size(); k++) {
				ret = AFR_FSDK_FacePairMatching(hRecognizeEngine, &modelInfos[j].faceModels[k], &LocalFaceModels, &fSimilScore);
				if (fSimilScore > 0.7 && fSimilScore > similarScore) {
					similarScore = fSimilScore;
					similarIndex = j;
				}
			}
		}
		printf("fSimilScore == %f\r\n", fSimilScore);
		char text[100] = {0};
		if (similarScore > 0.75) {
			sprintf(text, "%s %f", modelInfos[similarIndex].name, similarScore);
			printf("name = %s\n", text);
		}
	}

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
#endif
}

void init_sdl(void)
{
	Uint32 pixformat = 0;
	SDL_Thread *refresh_thread;

	const int pixel_w = 2592, pixel_h = 1520;

	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return;
	}
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w,
	                          screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	pixformat = SDL_PIXELFORMAT_IYUV;

	sdlTexture = SDL_CreateTexture(sdlRenderer, pixformat, SDL_TEXTUREACCESS_STREAMING, pixel_w, pixel_h);
}

void init_arcSoft(void)
{
	MByte *pDetectWorkMem = (MByte *)malloc(WORKBUF_SIZE);
	if (pDetectWorkMem == NULL) {
		fprintf(stderr, "fail to malloc workbuf\r\n");
		exit(0);
	}

	int ret = AFD_FSDK_InitialFaceEngine(APPID, DETECT_SDKKEY, pDetectWorkMem, WORKBUF_SIZE, &hDetectEngine,
	                                     AFD_FSDK_OPF_0_HIGHER_EXT, 16, MAX_FACE_NUM);
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
	char szIpAddr[32] = {0};
	char szUser[32] = {0};
	char szPwd[32] = {0};
	int nPort = 37777;
	LLONG lRealHandle;
	SDL_Event event;

	// init_sdl();
	init_arcSoft();
	init_models();
	//初始化SDK资源,设置断线回调函数
	CLIENT_Init(Disconnect, 0);

	//获取播放库空闲端口
	PLAY_GetFreePort(&gPlayPort);
	PLAY_SetStreamOpenMode(gPlayPort, STREAME_REALTIME);
	PLAY_OpenStream(gPlayPort, NULL, 0, 900 * 1024);
	PLAY_SetDisplayCallBack(gPlayPort, fDisplayCB, NULL);
	PLAY_Play(gPlayPort, NULL);

	cout << "Input device ip address:" << endl;
	cin >> szIpAddr;

	cout << "Input device port:" << endl;
	cin >> nPort;

	cout << "Input username:" << endl;
	cin >> szUser;

	cout << "Input password:" << endl;
	cin >> szPwd;

	//登陆
	NET_DEVICEINFO_Ex stLoginInfo = { 0 };
	int nErrcode = 0;

	lLoginHandle = CLIENT_LoginEx2(szIpAddr, nPort, szUser, szPwd, (EM_LOGIN_SPAC_CAP_TYPE) 0, NULL, &stLoginInfo,
	                               &nErrcode);
	if (0 == lLoginHandle) {
		cout << "Login device failed" << endl;
		goto end;
	} else {
		cout << "Login device success" << endl;
	}

	//拉流
	lRealHandle = CLIENT_RealPlayEx(lLoginHandle, 0, NULL);
	if (0 == lRealHandle) {
		cout << "CLIENT_RealPlayEx fail!" << endl;
		return -1;
	}
	cout << "CLIENT_RealPlayEx success!" << endl;

	//设置拉流回调
	CLIENT_SetRealDataCallBack(lRealHandle, fRealDataCB, 0);

	while (1) {
		sleep(100);
		continue;
		SDL_WaitEvent(&event);
		switch (event.type) {
		case SDL_WINDOWEVENT:
			//If Resize
			SDL_GetWindowSize(screen, &screen_w, &screen_h);
			break;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			case SDLK_LEFT:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_LEFT_CONTROL, 2, 2, 2, false);
				break;
			case SDLK_RIGHT:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_RIGHT_CONTROL, 2, 2, 2, false);
				break;
			case SDLK_UP:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_UP_CONTROL, 2, 2, 2, false);
				break;
			case SDLK_DOWN:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_DOWN_CONTROL, 2, 2, 2, false);
				break;
			case SDLK_MINUS:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_DEC_CONTROL, 2, 2, 2, false);
				break;
			case SDLK_EQUALS:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_ADD_CONTROL, 2, 2, 2, false);
				break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.sym) {
			case SDLK_LEFT:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_LEFT_CONTROL, 2, 2, 2, true);
				break;
			case SDLK_RIGHT:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_RIGHT_CONTROL, 2, 2, 2, true);
				break;
			case SDLK_UP:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_UP_CONTROL, 2, 2, 2, true);
				break;
			case SDLK_DOWN:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_DOWN_CONTROL, 2, 2, 2, true);
				break;
			case SDLK_MINUS:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_DEC_CONTROL, 2, 2, 2, true);
				break;
			case SDLK_EQUALS:
				CLIENT_DHPTZControlEx2(lLoginHandle, 0, DH_PTZ_ZOOM_ADD_CONTROL, 2, 2, 2, true);
				break;
			}
			break;
		case SDL_QUIT:
			goto end;
		}
	}

end:
	PLAY_Stop(gPlayPort);
	PLAY_CloseStream(gPlayPort);
	PLAY_ReleasePort(gPlayPort);

	//登出设备
	CLIENT_Logout(lLoginHandle);

	//清理SDK资源
	CLIENT_Cleanup();
	return 0;
}
