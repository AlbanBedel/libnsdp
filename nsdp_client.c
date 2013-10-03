#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <event.h>

#include "nsdp_socket.h"
#include "nsdp_packet.h"

typedef int (*nsdp_client_on_response_f)(nsdp_packet_t *response,
                                         void *context);

typedef struct nsdp_client_request {
  struct list_head			list;
  unsigned				timeout;
  unsigned				retry_count;
  unsigned				send_count;
  nsdp_socket_addr_t			in_addr;
  nsdp_packet_t				packet;

  nsdp_client_on_response_f		on_response;
  void					*context;
} nsdp_client_request_t;

typedef struct nsdp_client {
  struct event_base			*ev_base;

  nsdp_socket_t				socket;
  nsdp_mac_t				mac;

  unsigned				client_port;
  unsigned				server_port;

  nsdp_seq_no_t				seq_no;

  struct event				*recv_event;
  struct event				*request_timeout_event;

  struct list_head			request;
} nsdp_client_t;


nsdp_client_request_t*
nsdp_client_request_new(nsdp_op_t op, nsdp_mac_t server_mac,
                        nsdp_socket_addr_t* in_addr,
                        nsdp_client_on_response_f on_response,
                        void* context)
{
  nsdp_client_request_t* req;

  if (!on_response)
    return NULL;

  req = calloc(1, sizeof(*req));
  if (!req)
    return NULL;

  INIT_LIST_HEAD(&req->list);
  req->timeout = 5;
  req->retry_count = 3;
  if (in_addr)
    memcpy(&req->in_addr, in_addr, sizeof(*in_addr));
  else {
    nsdp_socket_addr_set_broadcast(&req->in_addr);
    nsdp_socket_addr_set_port(&req->in_addr, 63322);
  }

  nsdp_packet_init(&req->packet);
  req->packet.op = op;
  memcpy(req->packet.server_mac, server_mac, sizeof(nsdp_mac_t));
  req->on_response = on_response;
  req->context = context;

  return req;
}

void nsdp_client_request_free(nsdp_client_request_t* req)
{
  if (!req)
    return;
  list_del(&req->list);
  nsdp_packet_uninit(&req->packet);
}

nsdp_client_request_t* nsdp_client_pending_request(nsdp_client_t *client)
{
  if (!client || list_empty(&client->request))
    return NULL;
  return list_first_entry(&client->request, nsdp_client_request_t, list);
}

int nsdp_client_send_request(nsdp_client_t *client,
                             nsdp_client_request_t* req)
{
  uint8_t buffer[1500];
  struct timeval tout = {};
  int len, err;

  if (!client || !req)
    return -EINVAL;

  req->packet.seq_no = client->seq_no;
  req->send_count += 1;

  len = nsdp_packet_write(&req->packet, buffer, sizeof(buffer));
  if (len < 0)
    return len;

  //fprintf(stderr, "Sending request with seq no %u\n", req->packet.seq_no);
  err = nsdp_socket_sendto(client->socket, buffer, len, &req->in_addr);
  if (err < 0)
    return err;

  // Add the timeout
  tout.tv_sec = req->timeout;
  event_add(client->request_timeout_event, &tout);

  return 0;
}

int nsdp_client_send_pending_request(nsdp_client_t *client)
{
  return nsdp_client_send_request(client,
                                  nsdp_client_pending_request(client));
}

int nsdp_client_add_request(nsdp_client_t *client,
                            nsdp_client_request_t* req)
{
  int do_send = 0;
  if (!client || !req)
    return -EINVAL;
  do_send = list_empty(&client->request);
  memcpy(req->packet.client_mac, client->mac, sizeof(nsdp_mac_t));
  list_add_tail(&req->list, &client->request);
  return do_send ? nsdp_client_send_pending_request(client) : 0;
}

static void nsdp_client_recv(int sock, short what, void *arg)
{
  nsdp_client_t *client = arg;
  nsdp_client_request_t *request;
  nsdp_packet_t response;
  uint8_t data[1500];
  int len, err;

  len = recv(sock, data, sizeof(data), 0);
  if (len < 0) {
    fprintf(stderr, "Failed to receive a packet: %s\n", strerror(errno));
    return;
  }

  // Ignore if there is no request
  request = nsdp_client_pending_request(client);
  if (!request)
    return;

  nsdp_packet_init(&response);
  err = nsdp_packet_read(&response, data, len);
  if (err < 0) {
    fprintf(stderr, "Failed to read packet: %s\n", strerror(-err));
    return;
  }

  // TODO: Check the MAC if we are connected

  if ((request->packet.op == NSDP_OP_READ_REQUEST &&
       response.op != NSDP_OP_READ_RESPONSE) ||
      (request->packet.op == NSDP_OP_WRITE_REQUEST &&
       response.op != NSDP_OP_WRITE_RESPONSE)) {
    fprintf(stderr, "Got packet with a bad op\n");
    return;
  }

  if (response.seq_no != client->seq_no) {
    fprintf(stderr, "Got packet with a bad seq no\n");
    return;
  }

  // Cancel the request timeout
  event_del(client->request_timeout_event);

  // Deliver
  if (request->on_response(&response, request->context))
    nsdp_client_request_free(request);
  else
    request->send_count = 0;
  client->seq_no += 1;

  // Submit the next request, or resubmit the same
  nsdp_client_send_pending_request(client);
}

static void nsdp_client_request_timeout(int sock, short what, void *arg)
{
  nsdp_client_t *client = arg;
  nsdp_client_request_t *request;

  request = nsdp_client_pending_request(client);
  if (!request)
    return;

  // Resend if the retry count has been exceeded yet
  if (request->send_count < request->retry_count) {
    nsdp_client_send_request(client, request);
    return;
  }

  // Deliver the timeout
  if (request->on_response(NULL, request->context))
    nsdp_client_request_free(request);
  else
    request->send_count = 0;
  client->seq_no += 1;

  // Submit the next request
  nsdp_client_send_pending_request(client);
}

int nsdp_client_init(nsdp_client_t *client,
                     struct event_base *ev_base,
                     const char* mac,
                     const char* iface,
                     unsigned client_port,
                     unsigned server_port)
{
  int err;

  if (!client || !ev_base || (!iface && !mac))
    return -EINVAL;

  memset(client, 0, sizeof(*client));

  client->ev_base = ev_base;
  client->client_port = client_port ? client_port : 63321;
  client->server_port = server_port ? server_port : client->client_port+1;
  client->seq_no = random();
  INIT_LIST_HEAD(&client->request);

  if (mac)
    err = nsdp_property_type_mac.from_text(mac, client->mac,
                                           sizeof(client->mac));
  else
    err = nsdp_iface_get_mac(iface, client->mac);
  if (err < 0)
    return err;

  if ((err = nsdp_socket_open(iface, NULL, client->client_port,
                              &client->socket)) < 0)
    return err;

  client->recv_event = event_new(client->ev_base, client->socket,
                                 EV_READ | EV_PERSIST,
                                 nsdp_client_recv, client);
  client->request_timeout_event = event_new(client->ev_base, -1, 0,
                                            nsdp_client_request_timeout,
                                            client);
  event_add(client->recv_event, NULL);
  return 0;
}

int nsdp_client_run(nsdp_client_t *client, int timeout)
{
  if (!client)
    return -EINVAL;
  if (timeout >= 0) {
    struct timeval tv = { .tv_sec = timeout };
    event_base_loopexit(client->ev_base, &tv);
  }
  return event_base_dispatch(client->ev_base);
}

int nsdp_client_read_property(nsdp_client_t *client,
                              nsdp_mac_t server_mac,
                              nsdp_socket_addr_t* in_addr,
                              nsdp_client_on_response_f on_response,
                              void *context, ...)
{
  va_list ap;
  nsdp_client_request_t* req =
    nsdp_client_request_new(NSDP_OP_READ_REQUEST,
                            server_mac, in_addr,
                            on_response, context);

  if (!req)
    return -ENOMEM;

  // add the properties to req->packet
  va_start(ap, context);
  while (1) {
    int type = va_arg(ap, int);
    if (type == NSDP_PROPERTY_NONE || type == NSDP_PROPERTY_TERMINATOR)
      break;
    nsdp_packet_add_property(&req->packet, nsdp_property_new(type, 0));
  }
  va_end(ap);
  nsdp_packet_add_properties_terminator(&req->packet);

  return nsdp_client_add_request(client, req);
}

int nsdp_client_write_property(nsdp_client_t *client,
                               nsdp_mac_t server_mac,
                               nsdp_socket_addr_t* in_addr,
                               nsdp_client_on_response_f on_response,
                               void *context,
                               unsigned type, unsigned size, const void* data)
{
  nsdp_client_request_t* req =
    nsdp_client_request_new(NSDP_OP_WRITE_REQUEST,
                            server_mac, in_addr,
                            on_response, context);
  nsdp_packet_add_property(&req->packet, nsdp_property_from_data(type, size,
                                                                 data));
  nsdp_packet_add_properties_terminator(&req->packet);
  return nsdp_client_add_request(client, req);
}

static int nsdp_client_on_scan_response(nsdp_packet_t *response,
                                        void *context)
{
  nsdp_client_t *client = context;
  nsdp_property_t *property;
  char value[512];

  if (!response) {
    printf("Scan timeout!\n");
    event_base_loopbreak(client->ev_base);
    return 1;
  }

  printf("Got scan response from %02x:%02x:%02x:%02x:%02x:%02x\n",
         response->server_mac[0], response->server_mac[1],
         response->server_mac[2], response->server_mac[3],
         response->server_mac[4], response->server_mac[5]);

  nsdp_packet_for_each_property(response, property) {
    const struct nsdp_property_desc* desc = nsdp_property_get_desc(property);
    if (desc) {
      int len = nsdp_property_to_txt(property, value, sizeof(value));
      if (len >= 0)
        printf("  %s: %s\n", desc->name, value);
      else
        printf("  %s: (not yet printable)\n", desc->name);
    } else
      printf("  %04x: (not yet printable)\n", property->tag);
  }

  // don't resend the request
  return 1;
}

int nsdp_client_do_scan(nsdp_client_t* client, int argc, char*const* argv)
{
  nsdp_mac_t all_mac = {};
  int i;
  for (i = 0 ; i < 3 ; i += 1) {
    nsdp_client_read_property(client, all_mac, NULL,
                         nsdp_client_on_scan_response,
                         client,
                         NSDP_PROPERTY_MODEL,
                         NSDP_PROPERTY_HOSTNAME,
                         NSDP_PROPERTY_IP,
                         NSDP_PROPERTY_NETMASK,
                         NSDP_PROPERTY_GATEWAY,
                         NSDP_PROPERTY_DHCP,
                         NSDP_PROPERTY_FIRMWARE_VERSION,
                         NSDP_PROPERTY_PORT_COUNT,
                         NSDP_PROPERTY_TERMINATOR);
    nsdp_client_run(client, 10);
  }
  return 0;
}

static int nsdp_client_on_read_response(nsdp_packet_t *response,
                                        void *context)
{
  nsdp_client_t *client = context;
  nsdp_property_t *property;
  char value[512];
  int count = 0;

  if (!response) {
    printf("Timed out!\n");
    event_base_loopbreak(client->ev_base);
    return 1;
  }

  nsdp_packet_for_each_property(response, property) {
    const struct nsdp_property_desc* desc;
    if (property->tag == NSDP_PROPERTY_TERMINATOR)
      break;
    count += 1;
    desc = nsdp_property_get_desc(property);
    if (desc) {
      int len = nsdp_property_to_txt(property, value, sizeof(value));
      if (len >= 0)
        printf("%s: %s\n", desc->desc, value);
      else
        printf("%s: (not yet printable)\n", desc->desc);
    } else
      printf("0x%04x: (not yet printable)\n", property->tag);
  }

  if (count == 0)
    printf("Empty response\n");

  event_base_loopbreak(client->ev_base);
  return 1;
}

int nsdp_client_do_read(nsdp_client_t* client, int argc, char*const* argv)
{
  nsdp_client_request_t* req;
  nsdp_mac_t mac;
  int i;

  if (argc < 2) {
    fprintf(stderr,
            "Usage: nsdp_client [OPTS] -i INTERFACE read MAC PROP...\n");
    return 1;
  }

  if (nsdp_property_type_mac.from_text(argv[0], mac, sizeof(mac)) < 0) {
    fprintf(stderr, "Failed to parse MAC: %s\n", argv[0]);
    return 1;
  }

  req = nsdp_client_request_new(NSDP_OP_READ_REQUEST,
                                mac, NULL,
                                nsdp_client_on_read_response, client);
  if (!req) {
    fprintf(stderr, "Failed to create request\n");
    return 1;
  }

  for (i = 1 ; i < argc ; i += 1) {
    const struct nsdp_property_desc* desc = nsdp_get_property_desc(argv[i]);
    int tag = desc ? desc->tag : strtol(argv[i], NULL, 0);
    if (tag <= 0) {
      fprintf(stderr, "Unknown tag type: %s\n", argv[i]);
      return 1;
    }
    nsdp_packet_add_property(&req->packet, nsdp_property_new(tag, 0));
  }
  nsdp_packet_add_properties_terminator(&req->packet);

  nsdp_client_add_request(client, req);
  return nsdp_client_run(client, -1);
}

static int nsdp_client_on_write_response(nsdp_packet_t *response,
                                         void *context)
{
  // Just dump the response for now
  return nsdp_client_on_read_response(response, context);
}

int nsdp_client_do_write(nsdp_client_t* client, int argc, char*const* argv)
{
  nsdp_client_request_t* req;
  nsdp_mac_t mac;
  int i;

  if (argc < 3) {
    fprintf(stderr,
            "Usage: nsdp_client [OPTS] -i INTERFACE write MAC PROP VAL...\n");
    return 1;
  }

  if (nsdp_property_type_mac.from_text(argv[0], mac, sizeof(mac)) < 0) {
    fprintf(stderr, "Failed to parse MAC: %s\n", argv[0]);
    return 1;
  }

  req = nsdp_client_request_new(NSDP_OP_WRITE_REQUEST,
                                mac, NULL,
                                nsdp_client_on_write_response, client);
  if (!req) {
    fprintf(stderr, "Failed to create request\n");
    return 1;
  }

  for (i = 1 ; i+1 < argc ; i += 2) {
    const struct nsdp_property_desc* desc = nsdp_get_property_desc(argv[i]);
    struct nsdp_property* property;
    if (!desc) {
      fprintf(stderr, "Unknown tag: %s\n", argv[i]);
      return 1;
    }
    property = nsdp_property_from_txt(desc, argv[i+1]);
    if (!property) {
      fprintf(stderr, "Failed to parse value of tag %s: %s\n",
              argv[i], argv[i+1]);
      return 1;
    }
    nsdp_packet_add_property(&req->packet, property);
  }
  nsdp_packet_add_properties_terminator(&req->packet);

  nsdp_client_add_request(client, req);
  return nsdp_client_run(client, -1);
}

int nsdp_drop_privileges(void)
{
  int err = 0;
  if (!err && getuid() != geteuid())
    err = seteuid(getuid());
  if (!err && getgid() != getegid())
    err = setegid(getgid());
  return err;
}

void usage(int ret)
{
  printf("Usage: nsdp_client [OPTS] -i INTERFACE [scan|read|write] ...\n");
  exit(ret);
}

int main(int argc, char*const* argv)
{
  nsdp_client_t client;
  struct event_base *ev_base;
  char* mac = NULL;
  char* iface = NULL;
  unsigned client_port = 0;
  unsigned server_port = 0;
  char* action;
  int (*do_action)(nsdp_client_t* client, int argc, char*const* argv);
  int opt, err;

  srandom(time(NULL));

  while ((opt = getopt(argc, argv, "hm:i:c:s:")) >= 0) {
    switch (opt) {
    case '?':
    case 'h':
      usage(opt != 'h');
      /* no return */
    case 'm':
      mac = optarg;
      break;
    case 'i':
      iface = optarg;
      break;
    case 'c':
      client_port = atoi(optarg);
      break;
    case 's':
      server_port = atoi(optarg);
      break;
    }
  }

  if (optind >= argc)
    usage(1);

  action = argv[optind];
  optind += 1;

  if (!strcmp(action, "scan"))
    do_action = nsdp_client_do_scan;
  else if (!strcmp(action, "read"))
    do_action = nsdp_client_do_read;
  else if (!strcmp(action, "write"))
    do_action = nsdp_client_do_write;
  else if (!strcmp(action, "help"))
    usage(0);
  else
    usage(1);

  ev_base = event_base_new();
  if (!ev_base) {
    fprintf(stderr, "Failed to get event base\n");
    return 1;
  }

  err = nsdp_client_init(&client, ev_base, mac, iface,
                         client_port, server_port);
  if (err) {
    fprintf(stderr, "Failed to init client: %s\n",
            strerror(-err));
    return 1;
  }

  err = nsdp_drop_privileges();
  if (err) {
    fprintf(stderr, "Failed to drop privileges: %s\n",
            strerror(-err));
    return 1;
  }

  return do_action(&client, argc-optind, argv+optind);
}
