/*
 * Copyright (c) 2022 ASPEED Technology Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "provision.h"
#include "checkpoint.h"
#include "i2c_utils.h"
#include "arguments.h"
#include "config.h"
#include "status.h"
#include "info.h"
#include "spdm.h"

static const char short_options[] = "hvb:a:c:p:uk:w:r:dsiS:";
static const struct option
	long_options[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'v' },
	{ "bus", required_argument, NULL, 'b' },
	{ "address", required_argument, NULL, 'a' },
	{ "pfrtoolconf", required_argument, NULL, 'c' },
	{ "provision", required_argument, NULL, 'p' },
	{ "unprovision", no_argument, NULL, 'u' },
	{ "checkpoint", required_argument, NULL, 'k' },
	{ "write_reg", required_argument, NULL, 'w' },
	{ "read_reg", required_argument, NULL, 'r' },
	{ "debug", no_argument, NULL, 'd' },
	{ "status", no_argument, NULL, 's' },
	{ "info", no_argument, NULL, 'i' },
	{ "spdm", required_argument, NULL, 'S' },
	{ 0, 0, 0, 0 }
};

static void usage(FILE *fp, int argc, char **argv)
{
	fprintf(fp,
		"Usage: %s [options]\n\n"
		"Options:\n"
		" -h | --help           Print this message\n"
		" -v | --version        show version\n"
		" -b | --bus            bus number\n"
		" -a | --address        slave address\n"
		" -c | --pfrtoolconf    aspeed pfr tool config\n"
		"                       [default : /usr/share/pfrconfig/aspeed-pfr-tool.conf]\n"
		" -p | --provision      provision\n"
		" -u | --unprovision    unprovision\n"
		" -k | --checkpoint     checkpoint\n"
		" -w | --write_reg      write register\n"
		" -r | --read_reg       read register\n"
		" -d | --debug          debug mode\n"
		" -s | --status         show rot status\n"
		" -i | --info           show bmc/pch version info\n"
		" -S | --spdm           enable or disable SPDM attestation\n"
		"example:\n"
		"--provision /usr/share/pfrconfig/rk_pub.pem\n"
		"--provision show\n"
		"--provision lock\n"
		"--checkpoint start\n"
		"--checkpoint pause\n"
		"--checkpoint resume\n"
		"--checkpoint complete\n"
		"--write_reg <rf_addr> <data> (byte mode)\n"
		"--write_reg <rf_addr> <data1> <data2>... (block mode)\n"
		"--read_reg <rf_addr> (byte mode)\n"
		"--read_reg <rf_addr> <length> (block mode)\n"
		"--status\n"
		"--info\n"
		"--spdm enable\n"
		"--spdm disable\n"
		"",
		argv[0]);
}

void printVersion(void)
{
	printf("ASPEED PFR tool version: %s\n", ASPEED_PFR_TOOL_VERSION);
}

void printArguments(ARGUMENTS args)
{
	printf("I2C_BUS = %d\n", args.i2c_bus);
	printf("ROT_ADDRESS = 0x%02x\n", args.rot_addr);
	printf("BMC_ACTIVE_PFM_OFFSET = 0x%08x\n", args.bmc_active_pfm_offset);
	printf("BMC_STAGING_OFFSET = 0x%08x\n", args.bmc_staging_offset);
	printf("BMC_RECOVERY_OFFSET = 0x%08x\n", args.bmc_recovery_offset);
	printf("PCH_ACTIVE_PFM_OFFSET = 0x%08x\n", args.pch_active_pfm_offset);
	printf("PCH_STAGING_OFFSET = 0x%08x\n", args.pch_staging_offset);
	printf("PCH_RECOVERY_OFFSET = 0x%08x\n", args.pch_recovery_offset);
	printf("Tx Msg\n");
	printRawData(args.tx_msg, args.tx_msg_len);
}

void parseConfigElements(ARGUMENTS *args)
{
	unsigned int len = 0;
	char *line = NULL;
	int read;
	FILE *fp;

	fp = fopen(args->pfr_tool_conf, "r");
	if (fp == NULL) {
		printf("Open %s failed\n", args->pfr_tool_conf);
		exit(EXIT_FAILURE);
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		if (strncmp(line, "I2C_BUS", strlen("I2C_BUS")) == 0)
			args->i2c_bus = strtoul(&line[strlen("I2C_BUS") + 1], 0, 0);
		else if (strncmp(line, "ROT_ADDRESS", strlen("ROT_ADDRESS")) == 0)
			args->rot_addr = strtoul(&line[strlen("ROT_ADDRESS") + 1], 0, 0);
		else if (strncmp(line, "BMC_ACTIVE_PFM_OFFSET", strlen("BMC_ACTIVE_PFM_OFFSET")) == 0)
			args->bmc_active_pfm_offset = strtoul(&line[strlen("BMC_ACTIVE_PFM_OFFSET") + 1], 0, 0);
		else if (strncmp(line, "BMC_STAGING_OFFSET", strlen("BMC_STAGING_OFFSET")) == 0)
			args->bmc_staging_offset = strtoul(&line[strlen("BMC_STAGING_OFFSET") + 1], 0, 0);
		else if (strncmp(line, "BMC_RECOVERY_OFFSET", strlen("BMC_RECOVERY_OFFSET")) == 0)
			args->bmc_recovery_offset = strtoul(&line[strlen("BMC_RECOVERY_OFFSET") + 1], 0, 0);
		else if (strncmp(line, "PCH_ACTIVE_PFM_OFFSET", strlen("PCH_ACTIVE_PFM_OFFSET")) == 0)
			args->pch_active_pfm_offset = strtoul(&line[strlen("PCH_ACTIVE_PFM_OFFSET") + 1], 0, 0);
		else if (strncmp(line, "PCH_STAGING_OFFSET", strlen("PCH_STAGING_OFFSET")) == 0)
			args->pch_staging_offset = strtoul(&line[strlen("PCH_STAGING_OFFSET") + 1], 0, 0);
		else if (strncmp(line, "PCH_RECOVERY_OFFSET", strlen("PCH_RECOVERY_OFFSET")) == 0)
			args->pch_recovery_offset = strtoul(&line[strlen("PCH_RECOVERY_OFFSET") + 1], 0, 0);
	}
}

int main(int argc, char *argv[])
{
	uint8_t rot_address_flag = 0;
	uint8_t unprovision_flag = 0;
	uint8_t checkpoint_flag = 0;
	uint8_t read_reg_value = 0;
	uint8_t provision_flag = 0;
	uint8_t write_reg_flag = 0;
	uint8_t read_reg_flag = 0;
	uint8_t bus_flag = 0;
	uint8_t status_flag = 0;
	uint8_t info_flag = 0;
	int8_t spdm_flag = -1;
	ARGUMENTS args = {0};
	uint8_t rot_addr;
	char option = 0;
	uint8_t bus;
	int i;
	int j;

	args.pfr_tool_conf = "/usr/share/pfrconfig/aspeed-pfr-tool.conf";
	args.debug_flag = 0;

	if (!argv[1]) {
		usage(stdout, argc, argv);
		exit(EXIT_FAILURE);
	}

	while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != (char) -1) {
		switch (option) {
		case 'h':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			printVersion();
			exit(EXIT_SUCCESS);
			break;
		case 'b':
			bus = strtoul(optarg, 0, 0);
			bus_flag = 1;
			break;
		case 'a':
			rot_addr = strtoul(optarg, 0, 0);
			rot_address_flag = 1;
			break;
		case 'c':
			args.pfr_tool_conf = optarg;
			break;
		case 'p':
			args.provision_cmd = optarg;
			provision_flag = 1;
			break;
		case 'u':
			unprovision_flag = 1;
			break;
		case 'k':
			args.checkpoint_cmd = optarg;
			checkpoint_flag = 1;
			break;
		case 'd':
			args.debug_flag = 1;
			break;
		case 'r':
			args.tx_msg[0] = strtoul(optarg, 0, 0);
			args.tx_msg_len += 1;
			read_reg_flag = 1;
			break;
		case 'w':
			args.tx_msg[0] = strtoul(optarg, 0, 0);
			args.tx_msg_len += 1;
			write_reg_flag = 1;
			break;
		case 's':
			status_flag = 1;
			break;
		case 'i':
			info_flag = 1;
			break;
		case 'S':
			if (strcmp(optarg, "enable") == 0) {
				spdm_flag = 1;
			} else if (strcmp(optarg, "disable") == 0) {
				spdm_flag = 0;
			}
			break;
		default:
			usage(stdout, argc, argv);
			exit(EXIT_FAILURE);
			break;
		}
	}

	parseConfigElements(&args);

	if (optind < argc) {
		args.tx_msg_len += argc - optind;
		for (j = 1, i = optind; i < argc; i++, j++)
			args.tx_msg[j] = strtoul(argv[i], 0, 0);
	}

	if (bus_flag)
		args.i2c_bus = bus;

	if (rot_address_flag)
		args.rot_addr = rot_addr;

	if (args.debug_flag)
		printArguments(args);

	args.i2c_fd = i2cOpenDev(args.i2c_bus, args.rot_addr);
	if (read_reg_flag) {
		if (args.tx_msg_len > 2)
			printf("invalid read register command\n");
		else {
			if (args.tx_msg_len == 1) {
				read_reg_value = i2cReadByteData(args, args.tx_msg[0]);
				printf("%02x\n", read_reg_value);
			} else {
				if (args.tx_msg[1] < 1) {
					printf("invalid length %02x\n", args.tx_msg[1]);
				} else {
					args.rx_msg_len = i2cReadBlockData(args, args.tx_msg[0], args.tx_msg[1], args.rx_msg);
					if (args.rx_msg_len > 0)
						printRawData(args.rx_msg, args.rx_msg_len);
				}
			}
		}
	}

	if (write_reg_flag) {
		if (args.tx_msg_len < 2)
			printf("invalid write register command\n");
		else
			if (args.tx_msg_len > 2)
				i2cWriteBlockData(args, args.tx_msg[0], args.tx_msg_len - 1, &args.tx_msg[1]);
			else
				i2cWriteByteData(args, args.tx_msg[0], args.tx_msg[1]);
	}

	if (provision_flag)
		provision(args);

	if (unprovision_flag)
		unprovision(args);

	if (checkpoint_flag)
		checkpoint(args);

	if (status_flag)
		show_status(args);

	if (info_flag)
		show_info(args);

	if (spdm_flag != -1)
		setSPDMFunction(args, spdm_flag);

	if (args.i2c_fd >= 0)
		close(args.i2c_fd);

	return 0;
}
