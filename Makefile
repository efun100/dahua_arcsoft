all:
	g++ main.cpp -L./lib/dahua -L./lib/arcsoft -ldhnetsdk -ldhplay -larcsoft_fsdk_face_detection -larcsoft_fsdk_face_recognition -lSDL2 -o dahuaplay
