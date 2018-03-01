/*
 * Copyright (C) 2010-2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <fuse.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/hab.h>

/* -------- start of HAB API updates ------------*/

#define hab_rvt_report_event_p					\
(								\
	(is_mx6dqp()) ?						\
	((hab_rvt_report_event_t *)HAB_RVT_REPORT_EVENT_NEW) :	\
	(is_mx6dq() && (soc_rev() >= CHIP_REV_1_5)) ?		\
	((hab_rvt_report_event_t *)HAB_RVT_REPORT_EVENT_NEW) :	\
	(is_mx6sdl() &&	(soc_rev() >= CHIP_REV_1_2)) ?		\
	((hab_rvt_report_event_t *)HAB_RVT_REPORT_EVENT_NEW) :	\
	((hab_rvt_report_event_t *)HAB_RVT_REPORT_EVENT)	\
)

#define hab_rvt_report_status_p					\
(								\
	(is_mx6dqp()) ?						\
	((hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS_NEW) :\
	(is_mx6dq() && (soc_rev() >= CHIP_REV_1_5)) ?		\
	((hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS_NEW) :\
	(is_mx6sdl() &&	(soc_rev() >= CHIP_REV_1_2)) ?		\
	((hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS_NEW) :\
	((hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS)	\
)

#define hab_rvt_authenticate_image_p				\
(								\
	(is_mx6dqp()) ?						\
	((hab_rvt_authenticate_image_t *)HAB_RVT_AUTHENTICATE_IMAGE_NEW) : \
	(is_mx6dq() && (soc_rev() >= CHIP_REV_1_5)) ?		\
	((hab_rvt_authenticate_image_t *)HAB_RVT_AUTHENTICATE_IMAGE_NEW) : \
	(is_mx6sdl() &&	(soc_rev() >= CHIP_REV_1_2)) ?		\
	((hab_rvt_authenticate_image_t *)HAB_RVT_AUTHENTICATE_IMAGE_NEW) : \
	((hab_rvt_authenticate_image_t *)HAB_RVT_AUTHENTICATE_IMAGE)	\
)

#define hab_rvt_entry_p						\
(								\
	(is_mx6dqp()) ?						\
	((hab_rvt_entry_t *)HAB_RVT_ENTRY_NEW) :		\
	(is_mx6dq() && (soc_rev() >= CHIP_REV_1_5)) ?		\
	((hab_rvt_entry_t *)HAB_RVT_ENTRY_NEW) :		\
	(is_mx6sdl() &&	(soc_rev() >= CHIP_REV_1_2)) ?		\
	((hab_rvt_entry_t *)HAB_RVT_ENTRY_NEW) :		\
	((hab_rvt_entry_t *)HAB_RVT_ENTRY)			\
)

#define hab_rvt_exit_p						\
(								\
	(is_mx6dqp()) ?						\
	((hab_rvt_exit_t *)HAB_RVT_EXIT_NEW) :			\
	(is_mx6dq() && (soc_rev() >= CHIP_REV_1_5)) ?		\
	((hab_rvt_exit_t *)HAB_RVT_EXIT_NEW) :			\
	(is_mx6sdl() &&	(soc_rev() >= CHIP_REV_1_2)) ?		\
	((hab_rvt_exit_t *)HAB_RVT_EXIT_NEW) :			\
	((hab_rvt_exit_t *)HAB_RVT_EXIT)			\
)

static inline void hab_rvt_failsafe_new(void)
{
}

#define hab_rvt_failsafe_p				\
(							\
	(is_mx6dqp()) ?					\
	((hab_rvt_failsafe_t *)hab_rvt_failsafe_new) :	\
	(is_mx6dq() && (soc_rev() >= CHIP_REV_1_5)) ?	\
	((hab_rvt_failsafe_t *)hab_rvt_failsafe_new) :	\
	(is_mx6sdl() && (soc_rev() >= CHIP_REV_1_2)) ?	\
	((hab_rvt_failsafe_t *)hab_rvt_failsafe_new) :	\
	((hab_rvt_failsafe_t *)HAB_RVT_FAILSAFE)	\
)

static inline enum hab_status hab_rvt_check_target_new(enum hab_target target,
						       const void *start,
						       size_t bytes)
{
	return HAB_SUCCESS;
}

#define hab_rvt_check_target_p					\
(								\
	(is_mx6dqp()) ?						\
	((hab_rvt_check_target_t *)hab_rvt_check_target_new) :	\
	(is_mx6dq() && (soc_rev() >= CHIP_REV_1_5)) ?		\
	((hab_rvt_check_target_t *)hab_rvt_check_target_new) :	\
	(is_mx6sdl() &&	(soc_rev() >= CHIP_REV_1_2)) ?		\
	((hab_rvt_check_target_t *)hab_rvt_check_target_new) :	\
	((hab_rvt_check_target_t *)HAB_RVT_CHECK_TARGET)	\
)

#define ALIGN_SIZE		0x1000
#define MX6DQ_PU_IROM_MMU_EN_VAR	0x009024a8
#define MX6DLS_PU_IROM_MMU_EN_VAR	0x00901dd0
#define MX6SL_PU_IROM_MMU_EN_VAR	0x00900a18
#define IS_HAB_ENABLED_BIT \
	(is_soc_type(MXC_SOC_MX7ULP) ? 0x80000000 :	\
	 (is_soc_type(MXC_SOC_MX7) ? 0x2000000 : 0x2))

static int ivt_header_error(const char *err_str, struct ivt_header *ivt_hdr)
{
	printf("%s magic=0x%x length=0x%02x version=0x%x\n", err_str,
	       ivt_hdr->magic, ivt_hdr->length, ivt_hdr->version);

	return 1;
}

static int verify_ivt_header(struct ivt_header *ivt_hdr)
{
	int result = 0;

	if (ivt_hdr->magic != IVT_HEADER_MAGIC)
		result = ivt_header_error("bad magic", ivt_hdr);

	if (be16_to_cpu(ivt_hdr->length) != IVT_TOTAL_LENGTH)
		result = ivt_header_error("bad length", ivt_hdr);

	if (ivt_hdr->version != IVT_HEADER_V1 &&
	    ivt_hdr->version != IVT_HEADER_V2)
		result = ivt_header_error("bad version", ivt_hdr);

	return result;
}

#if !defined(CONFIG_SPL_BUILD)

#define MAX_RECORD_BYTES     (8*1024) /* 4 kbytes */

struct record {
	uint8_t  tag;						/* Tag */
	uint8_t  len[2];					/* Length */
	uint8_t  par;						/* Version */
	uint8_t  contents[MAX_RECORD_BYTES];/* Record Data */
	bool	 any_rec_flag;
};

static char *rsn_str[] = {
			  "RSN = HAB_RSN_ANY (0x00)\n",
			  "RSN = HAB_ENG_FAIL (0x30)\n",
			  "RSN = HAB_INV_ADDRESS (0x22)\n",
			  "RSN = HAB_INV_ASSERTION (0x0C)\n",
			  "RSN = HAB_INV_CALL (0x28)\n",
			  "RSN = HAB_INV_CERTIFICATE (0x21)\n",
			  "RSN = HAB_INV_COMMAND (0x06)\n",
			  "RSN = HAB_INV_CSF (0x11)\n",
			  "RSN = HAB_INV_DCD (0x27)\n",
			  "RSN = HAB_INV_INDEX (0x0F)\n",
			  "RSN = HAB_INV_IVT (0x05)\n",
			  "RSN = HAB_INV_KEY (0x1D)\n",
			  "RSN = HAB_INV_RETURN (0x1E)\n",
			  "RSN = HAB_INV_SIGNATURE (0x18)\n",
			  "RSN = HAB_INV_SIZE (0x17)\n",
			  "RSN = HAB_MEM_FAIL (0x2E)\n",
			  "RSN = HAB_OVR_COUNT (0x2B)\n",
			  "RSN = HAB_OVR_STORAGE (0x2D)\n",
			  "RSN = HAB_UNS_ALGORITHM (0x12)\n",
			  "RSN = HAB_UNS_COMMAND (0x03)\n",
			  "RSN = HAB_UNS_ENGINE (0x0A)\n",
			  "RSN = HAB_UNS_ITEM (0x24)\n",
			  "RSN = HAB_UNS_KEY (0x1B)\n",
			  "RSN = HAB_UNS_PROTOCOL (0x14)\n",
			  "RSN = HAB_UNS_STATE (0x09)\n",
			  "RSN = INVALID\n",
			  NULL
};

static char *sts_str[] = {
			  "STS = HAB_SUCCESS (0xF0)\n",
			  "STS = HAB_FAILURE (0x33)\n",
			  "STS = HAB_WARNING (0x69)\n",
			  "STS = INVALID\n",
			  NULL
};

static char *eng_str[] = {
			  "ENG = HAB_ENG_ANY (0x00)\n",
			  "ENG = HAB_ENG_SCC (0x03)\n",
			  "ENG = HAB_ENG_RTIC (0x05)\n",
			  "ENG = HAB_ENG_SAHARA (0x06)\n",
			  "ENG = HAB_ENG_CSU (0x0A)\n",
			  "ENG = HAB_ENG_SRTC (0x0C)\n",
			  "ENG = HAB_ENG_DCP (0x1B)\n",
			  "ENG = HAB_ENG_CAAM (0x1D)\n",
			  "ENG = HAB_ENG_SNVS (0x1E)\n",
			  "ENG = HAB_ENG_OCOTP (0x21)\n",
			  "ENG = HAB_ENG_DTCP (0x22)\n",
			  "ENG = HAB_ENG_ROM (0x36)\n",
			  "ENG = HAB_ENG_HDCP (0x24)\n",
			  "ENG = HAB_ENG_RTL (0x77)\n",
			  "ENG = HAB_ENG_SW (0xFF)\n",
			  "ENG = INVALID\n",
			  NULL
};

static char *ctx_str[] = {
			  "CTX = HAB_CTX_ANY(0x00)\n",
			  "CTX = HAB_CTX_FAB (0xFF)\n",
			  "CTX = HAB_CTX_ENTRY (0xE1)\n",
			  "CTX = HAB_CTX_TARGET (0x33)\n",
			  "CTX = HAB_CTX_AUTHENTICATE (0x0A)\n",
			  "CTX = HAB_CTX_DCD (0xDD)\n",
			  "CTX = HAB_CTX_CSF (0xCF)\n",
			  "CTX = HAB_CTX_COMMAND (0xC0)\n",
			  "CTX = HAB_CTX_AUT_DAT (0xDB)\n",
			  "CTX = HAB_CTX_ASSERT (0xA0)\n",
			  "CTX = HAB_CTX_EXIT (0xEE)\n",
			  "CTX = INVALID\n",
			  NULL
};

static uint8_t hab_statuses[5] = {
	HAB_STS_ANY,
	HAB_FAILURE,
	HAB_WARNING,
	HAB_SUCCESS,
	-1
};

static uint8_t hab_reasons[26] = {
	HAB_RSN_ANY,
	HAB_ENG_FAIL,
	HAB_INV_ADDRESS,
	HAB_INV_ASSERTION,
	HAB_INV_CALL,
	HAB_INV_CERTIFICATE,
	HAB_INV_COMMAND,
	HAB_INV_CSF,
	HAB_INV_DCD,
	HAB_INV_INDEX,
	HAB_INV_IVT,
	HAB_INV_KEY,
	HAB_INV_RETURN,
	HAB_INV_SIGNATURE,
	HAB_INV_SIZE,
	HAB_MEM_FAIL,
	HAB_OVR_COUNT,
	HAB_OVR_STORAGE,
	HAB_UNS_ALGORITHM,
	HAB_UNS_COMMAND,
	HAB_UNS_ENGINE,
	HAB_UNS_ITEM,
	HAB_UNS_KEY,
	HAB_UNS_PROTOCOL,
	HAB_UNS_STATE,
	-1
};

static uint8_t hab_contexts[12] = {
	HAB_CTX_ANY,
	HAB_CTX_FAB,
	HAB_CTX_ENTRY,
	HAB_CTX_TARGET,
	HAB_CTX_AUTHENTICATE,
	HAB_CTX_DCD,
	HAB_CTX_CSF,
	HAB_CTX_COMMAND,
	HAB_CTX_AUT_DAT,
	HAB_CTX_ASSERT,
	HAB_CTX_EXIT,
	-1
};

static uint8_t hab_engines[16] = {
	HAB_ENG_ANY,
	HAB_ENG_SCC,
	HAB_ENG_RTIC,
	HAB_ENG_SAHARA,
	HAB_ENG_CSU,
	HAB_ENG_SRTC,
	HAB_ENG_DCP,
	HAB_ENG_CAAM,
	HAB_ENG_SNVS,
	HAB_ENG_OCOTP,
	HAB_ENG_DTCP,
	HAB_ENG_ROM,
	HAB_ENG_HDCP,
	HAB_ENG_RTL,
	HAB_ENG_SW,
	-1
};

static inline uint8_t get_idx(uint8_t *list, uint8_t tgt)
{
	uint8_t idx = 0;
	uint8_t element = list[idx];
	while (element != -1) {
		if (element == tgt)
			return idx;
		element = list[++idx];
	}
	return -1;
}

static void process_event_record(uint8_t *event_data, size_t bytes)
{
	struct record *rec = (struct record *)event_data;

	printf("\n\n%s", sts_str[get_idx(hab_statuses, rec->contents[0])]);
	printf("%s", rsn_str[get_idx(hab_reasons, rec->contents[1])]);
	printf("%s", ctx_str[get_idx(hab_contexts, rec->contents[2])]);
	printf("%s", eng_str[get_idx(hab_engines, rec->contents[3])]);
}

static void display_event(uint8_t *event_data, size_t bytes)
{
	uint32_t i;

	if (!(event_data && bytes > 0))
		return;

	for (i = 0; i < bytes; i++) {
		if (i == 0)
			printf("\t0x%02x", event_data[i]);
		else if ((i % 8) == 0)
			printf("\n\t0x%02x", event_data[i]);
		else
			printf(" 0x%02x", event_data[i]);
	}

	process_event_record(event_data, bytes);
}

static int get_hab_status(void)
{
	uint32_t index = 0; /* Loop index */
	uint8_t event_data[128]; /* Event data buffer */
	size_t bytes = sizeof(event_data); /* Event size in bytes */
	enum hab_config config = 0;
	enum hab_state state = 0;
	hab_rvt_report_event_t *hab_rvt_report_event;
	hab_rvt_report_status_t *hab_rvt_report_status;

	hab_rvt_report_event = hab_rvt_report_event_p;
	hab_rvt_report_status = hab_rvt_report_status_p;

	if (imx_hab_is_enabled())
		puts("\nSecure boot enabled\n");
	else
		puts("\nSecure boot disabled\n");

	/* Check HAB status */
	if (hab_rvt_report_status(&config, &state) != HAB_SUCCESS) {
		printf("\nHAB Configuration: 0x%02x, HAB State: 0x%02x\n",
		       config, state);

		/* Display HAB Error events */
		while (hab_rvt_report_event(HAB_FAILURE, index, event_data,
					&bytes) == HAB_SUCCESS) {
			puts("\n");
			printf("--------- HAB Event %d -----------------\n",
			       index + 1);
			puts("event data:\n");
			display_event(event_data, bytes);
			puts("\n");
			bytes = sizeof(event_data);
			index++;
		}
	}
	/* Display message if no HAB events are found */
	else {
		printf("\nHAB Configuration: 0x%02x, HAB State: 0x%02x\n",
		       config, state);
		puts("No HAB Events Found!\n\n");
	}
	return 0;
}

static int do_hab_status(cmd_tbl_t *cmdtp, int flag, int argc,
			 char * const argv[])
{
	if ((argc != 1)) {
		cmd_usage(cmdtp);
		return 1;
	}

	get_hab_status();

	return 0;
}

static int do_authenticate_image(cmd_tbl_t *cmdtp, int flag, int argc,
				 char * const argv[])
{
	ulong	addr, length, ivt_offset;
	int	rcode = 0;

	if (argc < 4)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], NULL, 16);
	length = simple_strtoul(argv[2], NULL, 16);
	ivt_offset = simple_strtoul(argv[3], NULL, 16);

	rcode = imx_hab_authenticate_image(addr, length, ivt_offset);
	if (rcode == 0)
		rcode = CMD_RET_SUCCESS;
	else
		rcode = CMD_RET_FAILURE;

	return rcode;
}

static int do_hab_failsafe(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	hab_rvt_failsafe_t *hab_rvt_failsafe;

	if (argc != 1) {
		cmd_usage(cmdtp);
		return 1;
	}

	hab_rvt_failsafe = hab_rvt_failsafe_p;
	hab_rvt_failsafe();

	return 0;
}

U_BOOT_CMD(
		hab_status, CONFIG_SYS_MAXARGS, 1, do_hab_status,
		"display HAB status",
		""
	  );

U_BOOT_CMD(
		hab_auth_img, 4, 0, do_authenticate_image,
		"authenticate image via HAB",
		"addr length ivt_offset\n"
		"addr - image hex address\n"
		"length - image hex length\n"
		"ivt_offset - hex offset of IVT in the image"
	  );

U_BOOT_CMD(
		hab_failsafe, CONFIG_SYS_MAXARGS, 1, do_hab_failsafe,
		"run BootROM failsafe routine",
		""
	  );

#endif /* !defined(CONFIG_SPL_BUILD) */

bool imx_hab_is_enabled(void)
{
	struct imx_sec_config_fuse_t *fuse =
		(struct imx_sec_config_fuse_t *)&imx_sec_config_fuse;
	uint32_t reg;
	int ret;

	ret = fuse_read(fuse->bank, fuse->word, &reg);
	if (ret) {
		puts("\nSecure boot fuse read error\n");
		return ret;
	}

	return (reg & IS_HAB_ENABLED_BIT) == IS_HAB_ENABLED_BIT;
}

int imx_hab_authenticate_image(uint32_t ddr_start, uint32_t image_size,
			       uint32_t ivt_offset)
{
	uint32_t load_addr = 0;
	size_t bytes;
	uint32_t ivt_addr = 0;
	int result = 1;
	ulong start;
	hab_rvt_authenticate_image_t *hab_rvt_authenticate_image;
	hab_rvt_entry_t *hab_rvt_entry;
	hab_rvt_exit_t *hab_rvt_exit;
	hab_rvt_check_target_t *hab_rvt_check_target;
	struct ivt *ivt;
	struct ivt_header *ivt_hdr;
	enum hab_status status;

	hab_rvt_authenticate_image = hab_rvt_authenticate_image_p;
	hab_rvt_entry = hab_rvt_entry_p;
	hab_rvt_exit = hab_rvt_exit_p;
	hab_rvt_check_target = hab_rvt_check_target_p;

	if (!imx_hab_is_enabled()) {
		puts("hab fuse not enabled\n");
		return 0;
	}

	printf("\nAuthenticate image from DDR location 0x%x...\n",
	       ddr_start);

	hab_caam_clock_enable(1);

	/* Calculate IVT address header */
	ivt_addr = ddr_start + ivt_offset;
	ivt = (struct ivt *)ivt_addr;
	ivt_hdr = &ivt->hdr;

	/* Verify IVT header bugging out on error */
	if (verify_ivt_header(ivt_hdr))
		goto hab_caam_clock_disable;

	/* Verify IVT body */
	if (ivt->self != ivt_addr) {
		printf("ivt->self 0x%08x pointer is 0x%08x\n",
		       ivt->self, ivt_addr);
		goto hab_caam_clock_disable;
	}

	start = ddr_start;
	bytes = image_size;

	if (hab_rvt_entry() != HAB_SUCCESS) {
		puts("hab entry function fail\n");
		goto hab_exit_failure_print_status;
	}

	status = hab_rvt_check_target(HAB_TGT_MEMORY, (void *)ddr_start, bytes);
	if (status != HAB_SUCCESS) {
		printf("HAB check target 0x%08x-0x%08x fail\n",
		       ddr_start, ddr_start + bytes);
		goto hab_exit_failure_print_status;
	}
#ifdef DEBUG
	printf("\nivt_offset = 0x%x, ivt addr = 0x%x\n", ivt_offset, ivt_addr);
	printf("ivt entry = 0x%08x, dcd = 0x%08x, csf = 0x%08x\n", ivt->entry,
	       ivt->dcd, ivt->csf);
	puts("Dumping IVT\n");
	print_buffer(ivt_addr, (void *)(ivt_addr), 4, 0x8, 0);

	puts("Dumping CSF Header\n");
	print_buffer(ivt->csf, (void *)(ivt->csf), 4, 0x10, 0);

#if  !defined(CONFIG_SPL_BUILD)
	get_hab_status();
#endif

	puts("\nCalling authenticate_image in ROM\n");
	printf("\tivt_offset = 0x%x\n", ivt_offset);
	printf("\tstart = 0x%08lx\n", start);
	printf("\tbytes = 0x%x\n", bytes);
#endif
	/*
	 * If the MMU is enabled, we have to notify the ROM
	 * code, or it won't flush the caches when needed.
	 * This is done, by setting the "pu_irom_mmu_enabled"
	 * word to 1. You can find its address by looking in
	 * the ROM map. This is critical for
	 * authenticate_image(). If MMU is enabled, without
	 * setting this bit, authentication will fail and may
	 * crash.
	 */
	/* Check MMU enabled */
	if (is_soc_type(MXC_SOC_MX6) && get_cr() & CR_M) {
		if (is_mx6dq()) {
			/*
			 * This won't work on Rev 1.0.0 of
			 * i.MX6Q/D, since their ROM doesn't
			 * do cache flushes. don't think any
			 * exist, so we ignore them.
			 */
			if (!is_mx6dqp())
				writel(1, MX6DQ_PU_IROM_MMU_EN_VAR);
		} else if (is_mx6sdl()) {
			writel(1, MX6DLS_PU_IROM_MMU_EN_VAR);
		} else if (is_mx6sl()) {
			writel(1, MX6SL_PU_IROM_MMU_EN_VAR);
		}
	}

	load_addr = (uint32_t)hab_rvt_authenticate_image(
			HAB_CID_UBOOT,
			ivt_offset, (void **)&start,
			(size_t *)&bytes, NULL);
	if (hab_rvt_exit() != HAB_SUCCESS) {
		puts("hab exit function fail\n");
		load_addr = 0;
	}

hab_exit_failure_print_status:
#if !defined(CONFIG_SPL_BUILD)
	get_hab_status();
#endif

hab_caam_clock_disable:
	hab_caam_clock_enable(0);

	if (load_addr != 0)
		result = 0;

	return result;
}
