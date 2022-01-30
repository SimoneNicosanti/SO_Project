compile :
	gcc `pkg-config --cflags gtk+-3.0` -rdynamic clientMain.c clientLog.c clientMessage.c -o client.out `pkg-config --libs gtk+-3.0`

resource :
	gcc `pkg-config --cflags gtk+-3.0` -rdynamic clientResource.c clientMain.c clientLog.c clientMessage.c -o client_res.out `pkg-config --libs gtk+-3.0` -D RES