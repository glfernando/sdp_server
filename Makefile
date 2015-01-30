build: sdp_server.c
	${CC}  -o sdp_server sdp_server.c -lbluetooth

clean:
	rm -f sdp_server
