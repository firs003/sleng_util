#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <linux/route.h>

#include "sleng_debug.h"
#include "dth_network.h"



static void print_net_params(network_params_t *param) {
	printf("[%s](%s):\t%08x\t%08x\t%08x\t%08x\n", param->ifname, (param->up)? "up": "down", param->ip, param->mask, param->gateway, param->broadcast);
}

static char *fgets_skip_comment(char *s, int size, FILE *stream) {
	char *ret;
	do {
		ret = fgets(s, size, stream);
	} while (ret && s[0]=='#');
	return ret;
}


/**************************************************
 * System or board layer
 **************************************************/
// #define NETWORK_PARAMS_FILE_PATH "/disthen/config/network.conf"
#define NETWORK_PARAMS_FILE_PATH	"/etc/network/interfaces"
#define NETWORK_PARAMS_BACKUP_PATH	"/disthen/config/network.back"
#define NETWORK_PARAMS_DEFAULT_PATH	"/disthen/config/network.conf"
#define CDHX_IF_AMOUNT	2
#define DTH_CONFIG_SERVER_TMP_IFR_MAX 32
static const char *ifname_list[] = {
	"eth0",
	"eth1",
	"wlan0",
	"can0",
	"can1",
	"sit0",
	"tun0",
	"lo"
};


static int _check_mac(unsigned char *mac) {
	return 0;
}


static int _check_route(unsigned int route) {
	return 1;
}


static int check_param_valid(network_params_t *new, network_params_t *old_array, int array_size) {
	int ret = 1;
	if (new->dhcp_flag) {
		new->ip = new->mask = new->gateway = 0;
	} else {
		if ((new->ip & new->mask) != (new->gateway & new->mask)) {
			ret = 0;
		}
		if (((htonl(new->ip)&0xff000000) == 0) || ((htonl(new->ip)&0xff) == 0xff)) {
			ret = 0;
		} else if ((new->mask == 0) || (new->mask == 0xffffffff) || ((htonl(new->mask) & 0xff000000) == 0)
				   || ((htonl(new->mask) & 0xff0000) == 0) ){//|| ((htonl(new->mask)&0xff00) == 0)) { //linxj2011-06-01
			ret = 0;
		} else if (((htonl(new->gateway)  & 0xff000000) == 0) || ((htonl(new->gateway) & 0xff) == 0xff)) {
			ret = 0;
		} else if ((new->ip == new->mask) || (new->ip == new->gateway) || (new->mask == new->gateway)) {
			ret = 0;
		}

		if (old_array && array_size > 1) {
			if ((strncmp(new->ifname, old_array[0].ifname, sizeof(new->ifname)) == 0 && (new->ip & new->mask) != (old_array[1].ip & old_array[1].mask))
			 || (strncmp(new->ifname, old_array[1].ifname, sizeof(new->ifname)) == 0 && (new->ip & new->mask) != (old_array[0].ip & old_array[0].mask))) {
				ret = 0;
			}
		}
	}

	return ret;
}


static int load_params_from_file(const char *path, network_params_t *params_array, int array_size) {
	int ret = 0;
	FILE *fp = NULL;

	do {
		int i;
		char buf[1024] = {0, };

		if (!path || !params_array || array_size != CDHX_IF_AMOUNT) {
			ret = -1;
			errno = EINVAL;
			break;
		}

		if ((fp = fopen(path, "r")) == NULL) {
			sleng_error("fopen [%s] error", path);
			break;
		}

		/* Skip the fixed header 6 lines */
		for(i = 0; i < 6; i++) {
			fgets(buf, sizeof(buf), fp);
		}
		sleng_debug("path=%s\n", path);

		memset(params_array, 0, sizeof(network_params_t) * array_size);
		/* Get eth0 and eth1 config */
		for(i = 0; i < array_size; i++) {
			int eth_index;
			fgets_skip_comment(buf, sizeof(buf), fp);
			if (sscanf(buf, "auto eth%d", &eth_index) > 0) {
				char compare[128] = {0, };
				sprintf(params_array[eth_index].ifname, "eth%d", eth_index);
				params_array[eth_index].up = 1;
				fgets_skip_comment(buf, sizeof(buf), fp);
				strncpy(compare, "iface eth0 inet ", strlen("iface eth0 inet "));
				if (eth_index == 1) compare[strlen("iface eth")] = '1';
				if (strncmp(buf, compare, strlen(compare)) == 0) {
					const char *mode = buf + strlen(compare);
					if (strncmp(mode, "dhcp", strlen("dhcp")) == 0) {
						params_array[eth_index].dhcp_flag = 1;
					} else if (strncmp(mode, "static", strlen("static")) == 0) {
						do {
							struct in_addr addr;
							fgets_skip_comment(buf, sizeof(buf), fp);
							if (strlen(buf) > 1 && buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = '\0';
							memset(&addr, 0, sizeof(struct in_addr));
							// sleng_debug("buf[%hhx]=%s, len=%d, addr=%s\n", buf[0], buf, strlen(buf), buf + strlen("address "));
							if (strncmp(buf, "address ", strlen("address ")) == 0 && inet_aton(buf + strlen("address "), &addr)) {
								params_array[eth_index].ip = addr.s_addr;
							} else if (strncmp(buf, "netmask ", strlen("netmask ")) == 0 && inet_aton(buf + strlen("netmask "), &addr)) {
								params_array[eth_index].mask = addr.s_addr;
							} else if (strncmp(buf, "gateway ", strlen("gateway ")) == 0 && inet_aton(buf + strlen("gateway "), &addr)) {
								params_array[eth_index].gateway = addr.s_addr;
							} else if (strncmp(buf, "broadcast ", strlen("broadcast ")) == 0 && inet_aton(buf + strlen("broadcast "), &addr)) {
								params_array[eth_index].broadcast = addr.s_addr;
							}
						} while(buf[0] != '\n' && !feof(fp));
					}
				}
				ret++;
			}
		}

		for(i = 0; i < CDHX_IF_AMOUNT; i++) {
			if (params_array[i].dhcp_flag == 0 && params_array[i].broadcast == 0) params_array[i].broadcast = params_array[i].ip | 0xFF000000;
		}
	} while (0);

	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	return ret;
}


static int save_params_to_file(const char *path, network_params_t *params_array, int array_size) {
	int ret = 0;
	FILE *fp = NULL;

	do {
		int i;
		char buf[1024];

		if (!path || !params_array || array_size != CDHX_IF_AMOUNT) {
			ret = -1;
			errno = EINVAL;
			break;
		}

		if ((fp = fopen(path, "w")) == NULL) {
			sleng_error("fopen [%s] error", path);
			break;
		}

		/* Write the fixed header 5 lines */
		sprintf(buf, "%s%s%s%s%s%s",
			"# interfaces(5) file used by ifup(8) and ifdown(8)\n",
			"# Include files from /etc/network/interfaces.d:\n",
			"source-directory /etc/network/interfaces.d\n",
			"auto lo\n",
			"iface lo inet loopback\n",
			"\n");
		if (fwrite(buf, 1, strlen(buf), fp) == -1) {
			sleng_error("fwrite fixed 6 lines header error");
			ret = -1;
			break;
		}

		for(i = 0; i < array_size; i++) {
			if (params_array[i].up) {
				sprintf(buf, "auto eth%d\n", i);
				if (fwrite(buf, 1, strlen(buf), fp) == -1) {
					sleng_error("fwrite enable(up) for if[%d] error", i);
					ret = -1;
					break;
				}

				if (params_array[i].dhcp_flag) {
					sprintf(buf, "iface eth%d inet dhcp\n", i);
					if (fwrite(buf, 1, strlen(buf), fp) == -1) {
						sleng_error("fwrite dhcp for if[%d] error", i);
						ret = -1;
						break;
					}
				} else {
					struct in_addr addr;

					sprintf(buf, "iface eth%d inet static\n", i);
					if (fwrite(buf, 1, strlen(buf), fp) == -1) {
						sleng_error("fwrite static for if[%d] error", i);
						ret = -1;
						break;
					}

					addr.s_addr = params_array[i].ip;
					sprintf(buf, "address %s\n", inet_ntoa(addr));
					if (fwrite(buf, 1, strlen(buf), fp) == -1) {
						sleng_error("fwrite address for if[%d] error", i);
						ret = -1;
						break;
					}

					addr.s_addr = params_array[i].mask;
					sprintf(buf, "netmask %s\n", inet_ntoa(addr));
					if (fwrite(buf, 1, strlen(buf), fp) == -1) {
						sleng_error("fwrite netmask for if[%d] error", i);
						ret = -1;
						break;
					}

					addr.s_addr = params_array[i].gateway;
					sprintf(buf, "gateway %s\n", inet_ntoa(addr));
					if (fwrite(buf, 1, strlen(buf), fp) == -1) {
						sleng_error("fwrite gateway for if[%d] error", i);
						ret = -1;
						break;
					}

					// addr.s_addr = params_array[i].broadcast;
					// sprintf(buf, "broadcast %s\n", inet_ntoa(addr));
					// if (fwrite(buf, 1, strlen(buf), fp) == -1) {
					// 	sleng_error("fwrite broadcast for if[%d] error", i);
					// 	ret = -1;
					// 	break;
					// }

					sprintf(buf, "\n");
					if (fwrite(buf, 1, strlen(buf), fp) == -1) {
						sleng_error("fwrite \\n for if[%d] error", i);
						ret = -1;
						break;
					}
				}
			}
		}
	} while (0);

	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	return ret;
}


static int gen_netconf_file(const char *path, network_params_t *params) {
	int ret = 0;
	FILE *fp = NULL;

	do {
		int i;
		network_params_t paramv[CDHX_IF_AMOUNT];

		if (!path || !params) {
			sleng_debug("path=%p, params=%p\n", path, params);
			ret = -1;
			errno = EINVAL;
			break;
		}

		for(i = 0; i < CDHX_IF_AMOUNT && strncmp(params->ifname, ifname_list[i], sizeof(params->ifname)); i++);
		if (i == CDHX_IF_AMOUNT) {
			ret = -1;
			errno = EINVAL;
			break;
		}

		/* Get params from system config file */
		if ((ret = load_params_from_file(path, paramv, CDHX_IF_AMOUNT)) == -1) {
		// if (load_params_from_file(NETWORK_PARAMS_BACKUP_PATH, paramv, CDHX_IF_AMOUNT) == -1) {
			sleng_error("load_params_from_file[%s] error", path);
			ret = -1;
			break;
		}

		sleng_debug("read_ret = %d\n", ret);
		/* Change the specified param */
		if (check_param_valid(params, NULL, 0)) paramv[params->ifname[strlen(params->ifname) - 1] - '0'] = *params;
		for(i = 0; i < CDHX_IF_AMOUNT; i++) {
			print_net_params(paramv + i);
		}

		if (save_params_to_file(path, paramv, CDHX_IF_AMOUNT) == -1) {
		// if (save_params_to_file(NETWORK_PARAMS_DEFAULT_PATH, paramv, CDHX_IF_AMOUNT) == -1) {
			sleng_error("save_params_to_file[%s] error", path);
			ret = -1;
			break;
		}

	} while (0);

	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	return ret;
}


/**************************************************
 * API
 **************************************************/
int network_repair_route(const char *config) {
	int ret = 0;
	int sockfd;

	do {
		int i;
		network_params_t paramv[CDHX_IF_AMOUNT];
		unsigned int route[CDHX_IF_AMOUNT] = {0, };
		/* Get params from system config file */
		if ((ret = load_params_from_file(config, paramv, CDHX_IF_AMOUNT)) == -1) {
		// if (load_params_from_file(NETWORK_PARAMS_BACKUP_PATH, paramv, CDHX_IF_AMOUNT) == -1) {
			sleng_error("load_params_from_file[%s] error", config);
			ret = -1;
			break;
		}

		for(i = 0; i < CDHX_IF_AMOUNT; i++) {
			if (paramv[i].up) {
				route[i] = network_get_gateway(paramv[i].ifname);
				if (route[i] != paramv[i].gateway) {
					struct rtentry rt;
					struct sockaddr_in sa = {
						sin_family:	PF_INET,
						sin_port:	0
					};

					if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
						sleng_error("socket");
						ret = -1;
						break;
					}

					memset((char *) &rt, 0, sizeof(struct rtentry));
					// Fill in the other fields.
					rt.rt_flags = (RTF_UP | RTF_GATEWAY);
					rt.rt_dst.sa_family = PF_INET;
					rt.rt_genmask.sa_family = PF_INET;
					sa.sin_addr.s_addr = paramv[i].gateway;
					memcpy((char *) &rt.rt_gateway, (char *) &sa, sizeof(struct sockaddr));
					// Tell the kernel to accept this route.
					if (ioctl(sockfd, SIOCADDRT, &rt) < 0 && errno != EEXIST) {
						sleng_error("set gateway err");
						//return -1;    //sp 12-02-09 cut a bug
						ret = -1;
						break;
					}
				}
			}
		}
	} while (0);

	if (sockfd > 0) close(sockfd);
	return ret;
}


unsigned int network_get_gateway(const char *ifname) {
	FILE *fp;
	char buf[256]; // 128 is enough for linux
	char iface[16];
	unsigned int dest_addr, ret;
	fp = fopen("/proc/net/route", "r");
	if (fp == NULL)
		return -1;
	/* Skip title line */
	fgets(buf, sizeof(buf), fp);
	while (fgets(buf, sizeof(buf), fp)) {
		if (sscanf(buf, "%s\t%08x\t%08x", iface, &dest_addr, &ret) == 3
			&& strncmp(ifname, iface, strlen(ifname)) == 0
			&& ret != 0) {
			break;
		}
	}

	fclose(fp);
	return ret;
}


int network_modify(network_params_t *params, const char *file_path) {
	int ret = 0;
	FILE *fp = NULL;
	struct ifreq ifr;
	struct rtentry rt;
	int sockfd;
	struct sockaddr_in sa = {
		sin_family:	PF_INET,
		sin_port:	0
	};

	do {
		if (!params || !check_param_valid(params, NULL, 0)) {
			errno = EINVAL;
			ret = -1;
			break;
		}

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
			sleng_error("socket");
			ret = -1;
			break;
		}

		memset(&ifr, 0, sizeof(struct ifreq));
		//steven 09-27-09, set macAddr
		strncpy(ifr.ifr_name, params->ifname, IFNAMSIZ);
		sleng_debug("ifr_name=%s\n", ifr.ifr_name);

		// strncpy(ifr.ifr_name, params->ifname, IFNAMSIZ);
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
			sleng_error("get flags err");
			ret = -1;
			break;
		}
		if (params->up != (ifr.ifr_flags & IFF_UP)) {
			if (params->up)
				ifr.ifr_flags |= IFF_UP;
			else
				ifr.ifr_flags &= ~IFF_UP;
			if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
				sleng_error("set flags err");
				ret = -1;
				break;
			}
		}

		if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
			sleng_error("get MAC err");
			ret = -1;
			break;
		}
		if (_check_mac(params->mac)) {	//NOT support, sleng 20190424
			// strncpy(ifr.ifr_name, params->ifname, IFNAMSIZ);
			memcpy(ifr.ifr_ifru.ifru_hwaddr.sa_data, params->mac, IFHWADDRLEN);
			// print_in_hex(ifr.ifr_ifru.ifru_hwaddr.sa_data, IFHWADDRLEN, "New Mac", NULL);
			if (ioctl(sockfd, SIOCSIFHWADDR, &ifr) < 0) {
				sleng_error("set MAC err");
				ret = -1;
				break;
			}
		}

		if (params->dhcp_flag) {
			char cmd[64] = {0, };
			params->ip = params->mask = params->gateway = 0;
			printf("\n\n\n------------------------------------net_cfg.dhcp_flag = %d\n", params->dhcp_flag);
			sprintf(cmd, "dhclient %s", params->ifname);
			if (system(cmd)) {
				printf("dhclient %s failed!\n", params->ifname);
				ret = -1;
				break;
			}
		} else {	/* Static IP */
			strncpy(ifr.ifr_name, params->ifname, IFNAMSIZ);

			//steven 09-27-09, set ipaddr
			sa.sin_addr.s_addr = params->ip;
			// strncpy(ifr.ifr_name, params->ifname, IFNAMSIZ);
			memcpy((char *) &ifr.ifr_addr, (char *) &sa, sizeof(struct sockaddr));
			if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
				sleng_error("set ipaddr err\n");
				ret = -1;
				break;
			}

			//steven 09-27-09, set mask
			sa.sin_addr.s_addr = params->mask;
			// strncpy(ifr.ifr_name, params->ifname, IFNAMSIZ);
			memcpy((char *) &ifr.ifr_addr, (char *) &sa, sizeof(struct sockaddr));
			if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
				sleng_error("set mask err");
				//return -1;    //sp 12-02-09 cut a bug
				ret = -1;
				break;
			}

			if (params->up && _check_route(params->gateway)) {
				//steven 09-27-09, set gateway Addr
				// Clean out the RTREQ structure.
				memset((char *) &rt, 0, sizeof(struct rtentry));
				// Fill in the other fields.
				rt.rt_flags = (RTF_UP | RTF_GATEWAY);
				rt.rt_dst.sa_family = PF_INET;
				rt.rt_genmask.sa_family = PF_INET;
				sa.sin_addr.s_addr = params->gateway;
				memcpy((char *) &rt.rt_gateway, (char *) &sa, sizeof(struct sockaddr));
				// Tell the kernel to accept this route.
				if (ioctl(sockfd, SIOCADDRT, &rt) < 0 && errno != EEXIST) {
					sleng_error("set gateway err");
					//return -1;    //sp 12-02-09 cut a bug
					// ret = -1;
					// break;
				}
			}
		}

		if (file_path) {
			if (gen_netconf_file(file_path, params) == -1) {
				sleng_error("gen_netconf_file error");
				ret = -1;
				break;
			}
		}
	} while (0);

	if (sockfd > 0) close(sockfd);
	if (fp) fclose(fp);

	return ret;
}


int network_getstaus(network_params_t *out_array, size_t array_size) {
	int ret = 0, out_size;
	struct ifreq ifr;
	int sockfd;
	// network_params_t *param = (network_params_t *)out_array;
	struct sockaddr_in sa = {
		sin_family:	PF_INET,
		sin_port:	0
	};
	struct ifreq *ifr_array = NULL;
	struct ifconf ifc;

	do {
		int i;

		if (!out_array || array_size == 0) {
			errno = EINVAL;
			ret = -1;
			break;
		}

		ifr_array = calloc(DTH_CONFIG_SERVER_TMP_IFR_MAX, sizeof(struct ifreq));
		if(ifr_array == NULL) {
			sleng_error("calloc");
			ret = -1;
			break;
		}
		memset(&ifr, 0, sizeof(struct ifreq));
		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			sleng_error("socket");
			ret = -1;
			break;
		}
		ifc.ifc_len = DTH_CONFIG_SERVER_TMP_IFR_MAX * sizeof(struct ifreq);
		ifc.ifc_buf = (void *)ifr_array;
		if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
			sleng_error("set ifc list err");
			ret = -1;
			break;
		}

		sleng_debug("ifr_count=%lu\n", (unsigned long)ifc.ifc_len/sizeof(struct ifreq));
		out_size = (ifc.ifc_len/sizeof(struct ifreq) <= array_size)? ifc.ifc_len/sizeof(struct ifreq): array_size;
		for (i = 0; i < out_size; i++) {
			sleng_debug("%d.ifname=%s\n", i, ifr_array[i].ifr_name);
			strncpy(out_array[i].ifname, ifr_array[i].ifr_name, IFNAMSIZ);
			//steven 09-27-09, set ipaddr
			strncpy(ifr.ifr_name, ifr_array[i].ifr_name, IFNAMSIZ);
			if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0) {
				sleng_error("get ipaddr err");
				ret = -1;
				break;
			}
			memcpy((char *)&sa, (char *)&ifr.ifr_addr, sizeof(struct sockaddr));
			out_array[i].ip = sa.sin_addr.s_addr;

			//steven 09-27-09, get mask
			strncpy(ifr.ifr_name, ifr_array[i].ifr_name, IFNAMSIZ);
			if (ioctl(sockfd, SIOCGIFNETMASK, &ifr) < 0) {
				sleng_error("get mask err");
				//ret = -1;
				//break;    //sp 12-02-09 cut a bug
			}
			memcpy((char *)&sa, (char *)&ifr.ifr_addr, sizeof(struct sockaddr));
			out_array[i].mask = sa.sin_addr.s_addr;

			strncpy(ifr.ifr_name, ifr_array[i].ifr_name, IFNAMSIZ);
			if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
				sleng_error("get MAC err");
				ret = -1;
				break;
			}
			memcpy(out_array[i].mac, ifr.ifr_ifru.ifru_hwaddr.sa_data, IFHWADDRLEN);

			strncpy(ifr.ifr_name, ifr_array[i].ifr_name, IFNAMSIZ);
			if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
				sleng_error("get flags err");
				ret = -1;
				break;
			}
			out_array[i].up = ifr.ifr_flags & IFF_UP;

#if 0
			//how to get GATEWAY? ls, 2013-02-25
			//steven 09-27-09, set gateway Addr
			// Clean out the RTREQ structure.
			memset((char *) &rt, 0, sizeof(struct rtentry));
			// Fill in the other fields.
			rt.rt_flags = (RTF_UP | RTF_GATEWAY);
			rt.rt_dst.sa_family = PF_INET;
			rt.rt_genmask.sa_family = PF_INET;

			sa.sin_addr.s_addr = out_array[i].gateway;
			memcpy((char *) &rt.rt_gateway, (char *) &sa, sizeof(struct sockaddr));
			// Tell the kernel to accept this route.
			if (ioctl(sockfd, SIOCADDRT, &rt) < 0) {
				close(sockfd);
				sleng_error("set gateway err");
				//return -1;    //sp 12-02-09 cut a bug
			}
#else
			out_array[i].gateway = network_get_gateway(out_array[i].ifname);
			sleng_debug("param[%s]->gateway=%08x\n", out_array[i].ifname, out_array[i].gateway);
#endif
		}
		ret = i;
	} while (0);

	if (sockfd > 0) close(sockfd);
	if (ifr_array) free(ifr_array);
	return ret;
}


int network_getstaus_single(network_params_t *out, const char *ifname) {
	int ret = 0;
	struct ifreq ifr;
	int sockfd;
	struct sockaddr_in sa = {
		sin_family:	PF_INET,
		sin_port:	0
	};

	do {
		if (!out || !ifname) {
			errno = EINVAL;
			ret = -1;
			break;
		}

		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			sleng_error("socket");
			ret = -1;
			break;
		}

		memset(&ifr, 0, sizeof(struct ifreq));

		strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
			sleng_error("get flags err");
			ret = -1;
			break;
		}
		out->up = ifr.ifr_flags & IFF_UP;
		strncpy(out->ifname, ifname, sizeof(out->ifname));

		//steven 09-27-09, set ipaddr
		strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
		if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0 && out->up) {
			sleng_error("get ipaddr err");
			ret = -1;
			break;
		}
		memcpy((char *)&sa, (char *)&ifr.ifr_addr, sizeof(struct sockaddr));
		out->ip = sa.sin_addr.s_addr;

		//steven 09-27-09, get mask
		strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
		if (ioctl(sockfd, SIOCGIFNETMASK, &ifr) < 0 && out->up) {
			sleng_error("get mask err");
			//ret = -1;
			//break;    //sp 12-02-09 cut a bug
		}
		memcpy((char *)&sa, (char *)&ifr.ifr_addr, sizeof(struct sockaddr));
		out->mask = sa.sin_addr.s_addr;

		strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
		if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0 && out->up) {
			sleng_error("get MAC err");
			ret = -1;
			break;
		}
		memcpy(out->mac, ifr.ifr_ifru.ifru_hwaddr.sa_data, IFHWADDRLEN);

#if 0
		//how to get GATEWAY? ls, 2013-02-25
		//steven 09-27-09, set gateway Addr
		// Clean out the RTREQ structure.
		memset((char *) &rt, 0, sizeof(struct rtentry));
		// Fill in the other fields.
		rt.rt_flags = (RTF_UP | RTF_GATEWAY);
		rt.rt_dst.sa_family = PF_INET;
		rt.rt_genmask.sa_family = PF_INET;

		sa.sin_addr.s_addr = out->gateway;
		memcpy((char *) &rt.rt_gateway, (char *) &sa, sizeof(struct sockaddr));
		// Tell the kernel to accept this route.
		if (ioctl(sockfd, SIOCADDRT, &rt) < 0) {
			close(sockfd);
			sleng_error("set gateway err");
			//return -1;    //sp 12-02-09 cut a bug
		}
#else
		out->gateway = network_get_gateway(ifname);
		sleng_debug("param[%s]->gateway=%08x\n", ifname, out->gateway);
#endif
	} while (0);

	if (sockfd > 0) close(sockfd);
	// if (ifr_array) free(ifr_array);
	return ret;
}
