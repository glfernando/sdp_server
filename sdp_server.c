#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>


sdp_session_t *register_service()
{
	uint32_t service_uuid_int[] = { 0, 0, 0, 0xABCD };
	uint8_t rfcomm_channel = 11;
	const char *service_name = "Edison Test";
	const char *service_dsc = "Edison Experimental";
	const char *service_prov = "glfernando";
	sdp_session_t *session = 0;
	bdaddr_t bdaddr_local = {{0, 0, 0, 0xff, 0xff, 0xff}};

	uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid;
	sdp_list_t *l2cap_list = 0, 
		   *rfcomm_list = 0,
		   *root_list = 0,
		   *proto_list = 0, 
		   *access_proto_list = 0;
	sdp_data_t *channel = 0, *psm = 0;

	sdp_record_t *record = sdp_record_alloc();

	// set the general service ID
	sdp_uuid128_create( &svc_uuid, &service_uuid_int );
	sdp_set_service_id( record, svc_uuid );

	// make the service record publicly browsable
	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root_list = sdp_list_append(0, &root_uuid);
	sdp_set_browse_groups( record, root_list );

	// set l2cap information
	sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
	l2cap_list = sdp_list_append( 0, &l2cap_uuid );
	proto_list = sdp_list_append( 0, l2cap_list );

	// set rfcomm information
	sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
	channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
	rfcomm_list = sdp_list_append( 0, &rfcomm_uuid );
	sdp_list_append( rfcomm_list, channel );
	sdp_list_append( proto_list, rfcomm_list );

	// attach protocol information to service record
	access_proto_list = sdp_list_append( 0, proto_list );
	sdp_set_access_protos( record, access_proto_list );

	// set the name, provider, and description
	sdp_set_info_attr(record, service_name, service_prov, service_dsc);

	// connect to the local SDP server, register the service record, and disconnect
	session = sdp_connect( BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
	if (!session) {
		printf("session NULL , err %s\n", strerror(errno));
	}

	sdp_record_register(session, record, 0);

	// cleanup
	sdp_data_free( channel );
	sdp_list_free( l2cap_list, 0 );
	sdp_list_free( rfcomm_list, 0 );
	sdp_list_free( root_list, 0 );
	sdp_list_free( access_proto_list, 0 );

	return session;
}

int main(int argc, char **argv)
{
	struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
	char buf[1024] = { 0 };
	char str[1024] = { 0 };
	int s, client, bytes_read;
	sdp_session_t *session;
	socklen_t opt = sizeof(rem_addr);
	int fd = open("/sys/kernel/debug/gpio_debug/gpio40/current_value", O_RDWR);

	if (fd < 0) {
		printf("Error opening gpio fd %d\n", fd);
		goto out;
	}

	session = register_service();
	s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = *BDADDR_ANY;
	loc_addr.rc_channel = (uint8_t) 11;

	bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
	listen(s, 1);

	do {
		client = accept(s, (struct sockaddr *)&rem_addr, &opt);
		ba2str( &rem_addr.rc_bdaddr, buf );

		fprintf(stderr, "accepted connection from %s\n", buf);

		memset(buf, 0, sizeof(buf));

		while (read(client, buf, sizeof(buf)) > 0 ) {
			//printf("received [%s]\n", buf);

			if (*buf == '0')
				write(fd, "low", 3);
			else if (*buf == '1')
				write(fd, "high", 4);
			else
				break;
		}
		close(client);

	} while (*buf);

	close(s);
	sdp_close(session);

out:
	return 0;
}
