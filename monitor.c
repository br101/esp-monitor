#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <user_config.h>
#include <user_interface.h>

#include "monitor.h"
#include "wlan_parser.h"
#include "node.h"
#include "util/util.h"
#include "esp_promisc.h"

static const int channel_time = 500; /* ms */
static os_timer_t channel_timer;
static int ch;

LIST_HEAD(nodes);

struct config conf;

static struct packet_info pkt;

void ICACHE_FLASH_ATTR monitor_rx(uint8 *buf, uint16 len) {
	struct sniffer_buf* sb;
	struct sniffer_buf2* sb2;
	struct RxControl* rxc = NULL;
	uint8_t* frame = NULL;
	int frame_len = 0;

	if (len == 12) {
		// unreliable, no MAC, no header length, no AMPDU
		// has: rssi, FEC_CODING
		rxc = (struct RxControl*)buf;
		DEBUG("--- RX ???? RSSI %d\n", rxc->rssi);
		os_printf("~");
	}
	else if (len == 128) {
		// MGMT: 112 bytes of header & data
		sb2 = (struct sniffer_buf2*)buf;
		DEBUG("--- RX MGMT cnt %d len %d RSSI %d\n",
			  sb2->cnt, sb2->len, sb2->rx_ctrl.rssi);
		os_printf(",");
		rxc = &sb2->rx_ctrl;
		frame = sb2->buf;
		frame_len = 112;
	}
	else {
		// DATA: 36 bytes of header
		sb = (struct sniffer_buf*)buf;
		DEBUG("--- RX DATA cnt %d len %d RSSI %d\n",
			  sb->cnt, sb->lenseq[0].length, sb->rx_ctrl.rssi);
		os_printf(".");
		rxc = &sb->rx_ctrl;
		frame = sb->buf;
		frame_len = 36;
	}

	if (rxc != NULL && frame != NULL && frame_len != 0) {
		os_memset(&pkt, 0, sizeof(pkt));
		pkt.phy_signal = rxc->rssi;

		parse_80211_header((unsigned char*)frame, frame_len, &pkt);
		node_update(&pkt);
	}
}

void ICACHE_FLASH_ATTR nodes_print(void) {
	struct node_info* n;
	int i = 1;
	list_for_each(&nodes, n, list) {
		os_printf("%d. -%d CH%d " MAC_FMT, i++,
			  n->phy_sig_count > 0 ? (int)n->phy_sig_sum/n->phy_sig_count : 0,
			  n->wlan_channel,
			  MAC_PAR(n->wlan_src));

		if (n->wlan_mode & WLAN_MODE_AP)
			os_printf(" AP");
		if (n->wlan_mode & WLAN_MODE_IBSS)
			os_printf(" AD");
		if (n->wlan_mode & WLAN_MODE_STA)
			os_printf(" ST");
		if (n->wlan_mode & WLAN_MODE_PROBE)
			os_printf(" PR");
		if (n->wlan_mode & WLAN_MODE_4ADDR)
			os_printf(" 4A");
		os_printf("\n");
	}
	os_printf("\n");
}

static void timer_func(__attribute__((unused)) void *timer_arg) {
	os_printf("\n\nSeen nodes:\n");
	nodes_print();
	
	if (++ch > 11)
		ch = 1;

	wifi_set_channel(ch);
	os_printf("Channel: %d\n", wifi_get_channel());

	node_timeout();
}

void ICACHE_FLASH_ATTR monitor_start(void) {
	os_printf("\n--- Start monitor ---\n");

	wifi_set_sleep_type(NONE_SLEEP_T); // to avoid "error: pll_cal exceeds 2ms!!!"

	wifi_set_channel(1);
	ch = 1;
	os_printf("Channel: %d\n", wifi_get_channel());

	//char mac[6] = { 0x00, 0x0e, 0x8e, 0x20, 0x09, 0x60 }; // Libelula
	//wifi_promiscuous_set_mac(mac);

	wifi_set_promiscuous_rx_cb(monitor_rx);
	wifi_promiscuous_enable(1);
	
	conf.node_timeout = 20000000; /* 20 sec in usec */

	os_timer_setfn(&channel_timer, timer_func, NULL);
	os_timer_arm(&channel_timer, channel_time, true);
}

void ICACHE_FLASH_ATTR monitor_stop(void) {
	os_printf("\n--- Stop monitor ---\n");
	wifi_promiscuous_enable(0);
	os_timer_disarm(&channel_timer);
}
